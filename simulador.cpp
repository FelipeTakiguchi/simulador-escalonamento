// simulador.cpp - motor da simulação: um tick, histórico, edição e relatórios.
#include "header/simulador.hpp"

#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>

#include "header/util.hpp"

namespace {

Tarefa* acha(EstadoSistema& s, int id) {
    for (Tarefa& t : s.tarefas)
        if (t.id == id) return &t;
    return nullptr;
}
const Tarefa* acha(const EstadoSistema& s, int id) { return acha(const_cast<EstadoSistema&>(s), id); }

std::string T(int id) { return "T" + std::to_string(id); }

// A simulação termina quando todas as tarefas terminaram. Fica BLOQUEADA se não há mais
// nada executável nem ativação futura (só sobraram tarefas suspensas).
Situacao situacaoDe(const EstadoSistema& s) {
    bool todasTerminadas = true, haProgresso = false;
    for (const Tarefa& t : s.tarefas) {
        if (t.estado != EstadoTarefa::TERMINADA) todasTerminadas = false;
        if (t.executavel() || (t.estado != EstadoTarefa::TERMINADA && t.proximaAtivacao >= 0 && t.liberadas < MAX_ATIVACOES)) haProgresso = true;
    }
    if (todasTerminadas) return Situacao::CONCLUIDA;
    if (!haProgresso) return Situacao::BLOQUEADA;
    if (s.tick() >= Simulador::LIMITE_TICKS) return Situacao::LIMITE_ATINGIDO;
    return Situacao::EM_ANDAMENTO;
}

}  // namespace

const char* descricaoSituacao(Situacao s) {
    switch (s) {
        case Situacao::EM_ANDAMENTO: return "em andamento";
        case Situacao::CONCLUIDA: return "concluída (todas as tarefas terminaram)";
        case Situacao::BLOQUEADA: return "parada (só restam tarefas suspensas)";
        default: return "interrompida (limite de ticks atingido)";
    }
}

Simulador::Simulador(const Configuracao& cfg) {
    EstadoSistema s;
    s.algoritmo = cfg.algoritmo;
    s.quantum = cfg.quantum;
    s.cpus.resize(cfg.qtdCpus);
    for (int i = 0; i < cfg.qtdCpus; ++i) s.cpus[i].id = i;
    for (Tarefa t : cfg.tarefas) {
        t.proximaAtivacao = t.ingresso;  // a 1ª ativação chega no instante de ingresso
        s.tarefas.push_back(t);
    }
    hist_.push_back(s);
    nEventos_.push_back(0);
}

// ============================================================================
// UM TICK: libera tarefas -> escalona -> executa -> trata términos e prazos
// ============================================================================
void Simulador::iniciaAtivacao(Tarefa& k, int liberacao) {
    k.restante = k.duracao;
    k.ativacaoAtual = liberacao;
    k.deadlineAbsoluto = liberacao + k.prazo;  // o prazo conta a partir da ativação
    k.perdeuPrazo = false;
    if (k.estado != EstadoTarefa::SUSPENSA) k.estado = EstadoTarefa::PRONTA;
}

void Simulador::executaInstante(EstadoSistema& s) {
    const int t = s.tick();

    // 1) Chegada das ativações periódicas deste instante.
    for (Tarefa& k : s.tarefas)
        if (k.estado != EstadoTarefa::TERMINADA && k.proximaAtivacao >= 0 && k.proximaAtivacao <= t && k.liberadas < MAX_ATIVACOES) {
            ++k.liberadas;
            eventos_.push_back({t, gantt::TipoEvento::CHEGADA, k.id});
            if (k.temAtivacao()) k.pendentes.push_back(t);  // a anterior ainda não acabou: espera na fila
            else iniciaAtivacao(k, t);
            k.proximaAtivacao = (k.liberadas < MAX_ATIVACOES) ? t + k.periodo : -1;
        }

    // 2) O algoritmo (buscado pelo NOME no registro) decide quais tarefas executam.
    Contexto ctx;
    ctx.tick = t;
    ctx.qtdCpus = (int)s.cpus.size();
    ctx.quantum = s.quantum;
    ctx.sorteio = [&s](int n) {  // sorteio determinístico: voltar e avançar repete os mesmos resultados
        s.semente = s.semente * 1664525u + 1013904223u;
        return (int)((s.semente >> 16) % (uint32_t)n);
    };
    Decisao d = registroEscalonadores().at(s.algoritmo)(s.tarefas, ctx);
    std::set<int> escolhidas(d.escolhidas.begin(), d.escolhidas.end());

    // 3) Preempção: quem estava numa CPU e não foi escolhido volta para a fila de prontos.
    for (CPU& c : s.cpus)
        if (c.tarefaId >= 0 && !escolhidas.count(c.tarefaId)) {
            Tarefa* k = acha(s, c.tarefaId);
            k->estado = EstadoTarefa::PRONTA;
            k->cpu = -1;
            ++k->preempcoes;
            c.tarefaId = -1;
        }
    // Quem foi escolhido e não está em CPU ocupa uma CPU livre (quem já estava numa CPU continua nela).
    for (int id : d.escolhidas) {
        Tarefa* k = acha(s, id);
        if (k->cpu >= 0) continue;
        for (CPU& c : s.cpus)
            if (c.tarefaId < 0) { c.tarefaId = id; k->cpu = c.id; break; }
        if (std::find(d.sorteadas.begin(), d.sorteadas.end(), id) != d.sorteadas.end())
            eventos_.push_back({t, gantt::TipoEvento::SORTEIO, id});  // desempate por sorteio aparece no Gantt
    }

    // 4) Executa 1 tick. CPU sem tarefa fica DESLIGADA.
    gantt::Amostra a;
    a.cpu.assign(s.cpus.size(), -1);
    for (CPU& c : s.cpus) {
        if (c.tarefaId < 0) { ++c.ticksDesligada; continue; }
        Tarefa* k = acha(s, c.tarefaId);
        k->estado = EstadoTarefa::EXECUTANDO;
        --k->restante;
        ++k->tempoExecutado;
        a.cpu[c.id] = k->id;
    }
    for (const Tarefa& k : s.tarefas)
        if (k.estado == EstadoTarefa::SUSPENSA) a.suspensas.push_back(k.id);
    amostras_.push_back(a);

    // 5) Término de ativações (acontece no instante t+1).
    const int agora = t + 1;
    for (CPU& c : s.cpus) {
        if (c.tarefaId < 0) continue;
        Tarefa* k = acha(s, c.tarefaId);
        if (k->restante > 0) continue;
        c.tarefaId = -1;
        k->cpu = -1;
        ++k->concluidas;
        eventos_.push_back({agora, gantt::TipoEvento::FIM, k->id});
        if (k->concluidas >= MAX_ATIVACOES) k->estado = EstadoTarefa::TERMINADA;
        else if (!k->pendentes.empty()) { iniciaAtivacao(*k, k->pendentes.front()); k->pendentes.erase(k->pendentes.begin()); }
        else k->estado = EstadoTarefa::AGUARDANDO_PERIODO;
    }

    // 6) Perda de prazo: a tarefa continua executando, mas o erro aparece no Gantt.
    for (Tarefa& k : s.tarefas)
        if (k.temAtivacao() && !k.perdeuPrazo && agora >= k.deadlineAbsoluto) {
            k.perdeuPrazo = true;
            ++k.perdasPrazo;
            eventos_.push_back({agora, gantt::TipoEvento::PERDA_PRAZO, k.id});
        }

    s.relogio.incrementa();
}

// ============================================================================
// Navegação
// ============================================================================
Situacao Simulador::situacao() const { return situacaoDe(hist_[cur_]); }

bool Simulador::avancar() {
    visaoSuja_ = true;
    if (cur_ + 1 < (int)hist_.size()) { ++cur_; return true; }  // já calculado antes (depois de voltar)
    if (situacaoDe(hist_[cur_]) != Situacao::EM_ANDAMENTO) return false;
    EstadoSistema prox = hist_[cur_];  // cópia: o estado anterior continua intacto no histórico
    executaInstante(prox);
    hist_.push_back(prox);
    nEventos_.push_back(eventos_.size());
    ++cur_;
    return true;
}

bool Simulador::voltar() {
    if (cur_ == 0) return false;
    --cur_;
    visaoSuja_ = true;
    return true;
}

void Simulador::irPara(int tick) {
    while (cur_ > tick && voltar()) {}
    while (cur_ < tick && avancar()) {}
}

// ============================================================================
// Edição: tudo vale a partir do instante atual; o futuro já calculado é descartado.
// ============================================================================
Resultado Simulador::concluiEdicao(const std::string& msg) {
    const bool tinhaFuturo = (int)hist_.size() > cur_ + 1;
    hist_.resize(cur_ + 1);
    nEventos_.resize(cur_ + 1);
    amostras_.resize(cur_);
    eventos_.resize(nEventos_[cur_]);
    visaoSuja_ = true;
    return {true, msg + (tinhaFuturo ? "\n(O trecho já simulado depois deste instante foi descartado e será recalculado.)" : "")};
}

Resultado Simulador::editarTarefa(int id, const std::string& campoIn, const std::string& valorIn) {
    EstadoSistema& s = hist_[cur_];
    Tarefa* k = acha(s, id);
    if (!k) return {false, "Não existe tarefa com id " + std::to_string(id) + "."};
    const std::string campo = minusculas(trim(campoIn)), valor = trim(valorIn);
    int v = 0;
    const bool inteiro = paraInt(valor, v);
    auto tiraDaCpu = [&]() { if (k->cpu >= 0) { s.cpus[k->cpu].tarefaId = -1; k->cpu = -1; } };

    if (campo == "estado") {
        const std::string e = maiusculas(valor);
        if (k->estado == EstadoTarefa::TERMINADA) return {false, "A tarefa já terminou."};
        if (e == "SUSPENSA") { tiraDaCpu(); k->estado = EstadoTarefa::SUSPENSA; }
        else if (e == "PRONTA") {
            if (k->estado != EstadoTarefa::SUSPENSA) return {false, "Só uma tarefa SUSPENSA pode voltar a PRONTA; as demais ficam prontas sozinhas quando chega a ativação."};
            k->estado = k->temAtivacao() ? EstadoTarefa::PRONTA : (k->liberadas ? EstadoTarefa::AGUARDANDO_PERIODO : EstadoTarefa::NOVA);
        } else if (e == "TERMINADA") {
            tiraDaCpu();
            k->restante = 0;
            k->pendentes.clear();
            k->proximaAtivacao = -1;
            k->estado = EstadoTarefa::TERMINADA;
        } else return {false, "Estado inválido. Use SUSPENSA, PRONTA ou TERMINADA (EXECUTANDO quem decide é o escalonador)."};
    } else if (campo == "restante") {
        if (!inteiro || v < 1) return {false, "Informe um inteiro >= 1."};
        if (!k->temAtivacao()) return {false, "A tarefa não tem ativação em andamento."};
        k->restante = v;
    } else if (campo == "duracao") {
        if (!inteiro || v < 1) return {false, "A duração deve ser um inteiro >= 1 (vale para as próximas ativações; a atual muda com 'restante')."};
        k->duracao = v;
    } else if (campo == "periodo") {
        if (!inteiro || v < 1) return {false, "O período deve ser um inteiro >= 1 (período 0 seria aperiódica, que o projeto A não suporta)."};
        if (k->liberadas > 0 && k->proximaAtivacao >= 0) k->proximaAtivacao = std::max(s.tick(), k->proximaAtivacao - k->periodo + v);
        k->periodo = v;
    } else if (campo == "prazo") {
        if (!inteiro || v < 1) return {false, "O prazo deve ser um inteiro >= 1."};
        k->prazo = v;
        if (k->temAtivacao()) k->deadlineAbsoluto = k->ativacaoAtual + v;
    } else if (campo == "ingresso") {
        if (k->liberadas > 0) return {false, "A tarefa já foi ativada; o ingresso não pode mais mudar."};
        if (!inteiro || v < s.tick()) return {false, "O ingresso deve ser um inteiro >= " + std::to_string(s.tick()) + " (o relógio está em " + std::to_string(s.tick()) + ")."};
        k->ingresso = k->proximaAtivacao = v;
    } else if (campo == "cor") {
        std::string c = valor;
        if (!normalizaCor(c)) return {false, "Cor inválida: use 6 dígitos hexadecimais (ex.: F0E0D0)."};
        k->cor = c;
    } else {
        return {false, "Campo desconhecido. Use: estado, restante, duracao, periodo, prazo, ingresso ou cor."};
    }
    return concluiEdicao("Tarefa " + std::to_string(id) + ": '" + campo + "' alterado.");
}

Resultado Simulador::editarSistema(const std::string& campoIn, const std::string& valorIn) {
    EstadoSistema& s = hist_[cur_];
    const std::string campo = minusculas(trim(campoIn)), valor = trim(valorIn);
    int v = 0;
    const bool inteiro = paraInt(valor, v);
    if (campo == "algoritmo") {
        if (!algoritmoExiste(valor)) return {false, "Algoritmo desconhecido. Disponíveis: " + listaAlgoritmos() + "."};
        s.algoritmo = maiusculas(valor);
    } else if (campo == "quantum") {
        if (!inteiro || v < 0) return {false, "O quantum deve ser um inteiro >= 0."};
        s.quantum = v;
    } else if (campo == "cpus") {
        if (cur_ != 0) return {false, "O número de CPUs só muda no instante 0, pois o histórico já registra a execução com " + std::to_string(s.cpus.size()) + " CPU(s). Use 'ir 0'."};
        if (!inteiro || v < 1 || v > MAX_CPUS) return {false, "Informe um inteiro entre 1 e " + std::to_string(MAX_CPUS) + "."};
        s.cpus.assign(v, CPU{});
        for (int i = 0; i < v; ++i) s.cpus[i].id = i;
    } else {
        return {false, "Parâmetro desconhecido. Use: algoritmo, quantum ou cpus."};
    }
    return concluiEdicao("Sistema: '" + campo + "' alterado.");
}

Resultado Simulador::adicionarTarefa(const std::string& linha) {
    EstadoSistema& s = hist_[cur_];
    std::set<int> ids;
    for (const Tarefa& t : s.tarefas) ids.insert(t.id);
    Tarefa t;
    std::vector<std::string> erros, avisos;
    bool ignorada;
    if (!analisaLinhaTarefa(linha, 0, ids, (int)s.tarefas.size(), t, erros, avisos, ignorada)) return {false, juntaLinhas(erros)};
    if (ignorada) return {false, juntaLinhas(avisos)};
    if (t.ingresso < s.tick()) return {false, "O ingresso já passou (relógio em " + std::to_string(s.tick()) + "); use um valor >= " + std::to_string(s.tick()) + "."};
    t.proximaAtivacao = t.ingresso;
    s.tarefas.push_back(t);
    return concluiEdicao("Tarefa " + std::to_string(t.id) + " adicionada.");
}

Resultado Simulador::removerTarefa(int id) {
    EstadoSistema& s = hist_[cur_];
    for (size_t i = 0; i < s.tarefas.size(); ++i)
        if (s.tarefas[i].id == id) {
            if (s.tarefas[i].liberadas > 0) return {false, "A tarefa já foi ativada e consta no histórico. Para encerrá-la use: mod tarefa " + std::to_string(id) + " estado TERMINADA"};
            s.tarefas.erase(s.tarefas.begin() + i);
            return concluiEdicao("Tarefa " + std::to_string(id) + " removida.");
        }
    return {false, "Não existe tarefa com id " + std::to_string(id) + "."};
}

// ============================================================================
// Visualização e relatórios
// ============================================================================
const gantt::DadosGantt& Simulador::dados() {
    if (visaoSuja_) {  // só recalcula quando algo mudou
        const EstadoSistema& s = hist_[cur_];
        visao_.qtdCpus = (int)s.cpus.size();
        visao_.tarefas.clear();
        for (const Tarefa& t : s.tarefas) visao_.tarefas.push_back({t.id, t.cor});
        visao_.amostras.assign(amostras_.begin(), amostras_.begin() + cur_);
        visao_.eventos.assign(eventos_.begin(), eventos_.begin() + nEventos_[cur_]);
        visaoSuja_ = false;
    }
    return visao_;
}

// Fila de prontos global: o próprio algoritmo, com "infinitas" CPUs, devolve todas em ordem de prioridade.
std::vector<int> Simulador::filaProntos() const {
    const EstadoSistema& s = hist_[cur_];
    Contexto ctx;
    ctx.tick = s.tick();
    ctx.qtdCpus = 1000000;
    ctx.quantum = s.quantum;
    std::vector<int> fila;
    for (int id : registroEscalonadores().at(s.algoritmo)(s.tarefas, ctx).escolhidas)
        if (acha(s, id)->estado == EstadoTarefa::PRONTA) fila.push_back(id);
    return fila;
}

std::string Simulador::estadoTexto() const {
    const EstadoSistema& s = hist_[cur_];
    std::ostringstream o;
    o << "=== Instante " << s.tick() << " | " << s.algoritmo << " | situação: " << descricaoSituacao(situacao()) << " ===\n";
    for (const CPU& c : s.cpus)
        o << "CPU" << c.id << ": " << (c.ligada() ? "executando " + T(c.tarefaId) : "DESLIGADA") << "   (já ficou desligada " << c.ticksDesligada << " tick(s))\n";
    o << "Fila de prontos: ";
    for (int id : filaProntos()) o << T(id) << " ";
    o << "\n" << std::left << std::setw(5) << "ID" << std::setw(12) << "Estado" << std::setw(6) << "CPU" << std::setw(10) << "Restante"
      << std::setw(10) << "Deadline" << std::setw(10) << "ProxAtiv" << "Perdas\n";
    for (const Tarefa& t : s.tarefas)
        o << std::setw(5) << T(t.id) << std::setw(12) << nomeEstado(t.estado) << std::setw(6) << (t.cpu >= 0 ? "CPU" + std::to_string(t.cpu) : "-")
          << std::setw(10) << (t.temAtivacao() ? std::to_string(t.restante) : "-") << std::setw(10) << (t.temAtivacao() ? std::to_string(t.deadlineAbsoluto) : "-")
          << std::setw(10) << (t.proximaAtivacao >= 0 ? std::to_string(t.proximaAtivacao) : "-") << t.perdasPrazo << "\n";
    return o.str();
}

std::string Simulador::tarefaTexto(int id) const {
    const Tarefa* t = acha(hist_[cur_], id);
    if (!t) return "Não existe tarefa com id " + std::to_string(id) + ".\n";
    std::ostringstream o;
    o << "--- Tarefa " << t->id << " (instante " << cur_ << ") ---\n"
      << "Estado: " << nomeEstado(t->estado) << " | cor " << t->cor << "\n"
      << "Parâmetros: ingresso=" << t->ingresso << " duracao=" << t->duracao << " periodo=" << t->periodo << " prazo=" << t->prazo << "\n"
      << "Ativação atual: " << (t->temAtivacao() ? "restam " + std::to_string(t->restante) + " tick(s), deadline " + std::to_string(t->deadlineAbsoluto) : "nenhuma") << "\n"
      << "Liberadas: " << t->liberadas << " | concluídas: " << t->concluidas << " | em espera: " << t->pendentes.size() << "\n"
      << "Tempo de CPU: " << t->tempoExecutado << " | preempções: " << t->preempcoes << " | prazos perdidos: " << t->perdasPrazo << "\n"
      << "Lista de eventos (projeto B): " << (t->listaEventos.empty() ? "(vazia)" : t->listaEventos) << "\n";
    return o.str();
}

std::string Simulador::descricaoEventos(int de, int ate) const {
    const char* nomes[4] = {"chegada de ", "término de ", "PRAZO PERDIDO por ", "sorteio escolheu "};
    std::ostringstream o;
    for (size_t i = nEventos_[de]; i < nEventos_[std::min(ate, (int)hist_.size() - 1)]; ++i)
        o << "  t=" << eventos_[i].instante << ": " << nomes[(int)eventos_[i].tipo] << T(eventos_[i].idTarefa) << "\n";
    return o.str();
}

std::string Simulador::relatorioFinal() const {
    const EstadoSistema& s = hist_[cur_];
    std::ostringstream o;
    o << "===== RESULTADO (instante " << s.tick() << ", " << s.algoritmo << ", " << s.cpus.size() << " CPU(s)) =====\n"
      << std::left << std::setw(8) << "Tarefa" << std::setw(12) << "Estado" << std::setw(12) << "Concluídas" << std::setw(11) << "Tempo CPU" << "Prazos perdidos\n";
    for (const Tarefa& t : s.tarefas)
        o << std::setw(8) << T(t.id) << std::setw(12) << nomeEstado(t.estado) << std::setw(12) << t.concluidas << std::setw(11) << t.tempoExecutado << t.perdasPrazo << "\n";
    for (const CPU& c : s.cpus) o << "CPU" << c.id << " ficou desligada " << c.ticksDesligada << " de " << s.tick() << " tick(s)\n";
    return o.str();
}
