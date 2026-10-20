#include "header/escalonador.hpp"

#include <algorithm>
#include <tuple>

#include "header/util.hpp"

std::map<std::string, FuncEscalonador>& registroEscalonadores() {
    // Variável estática local: garante que o mapa existe antes de qualquer registro,
    // independentemente da ordem de inicialização entre arquivos .cpp.
    static std::map<std::string, FuncEscalonador> registro;
    return registro;
}

RegistraEscalonador::RegistraEscalonador(const std::string& nome, FuncEscalonador f) {
    registroEscalonadores()[maiusculas(nome)] = std::move(f);
}

bool algoritmoExiste(const std::string& nome) {
    return registroEscalonadores().count(maiusculas(trim(nome))) > 0;
}

std::string listaAlgoritmos() {
    std::string r;
    for (auto& par : registroEscalonadores()) r += (r.empty() ? "" : ", ") + par.first;
    return r;
}

Decisao escolheMelhores(const std::vector<Tarefa>& tarefas, const Contexto& ctx,
                        const std::function<long long(const Tarefa&)>& chave) {
    // Candidatas: tarefas prontas ou já executando (a fila de prontos é global).
    std::vector<const Tarefa*> cand;
    for (const Tarefa& t : tarefas)
        if (t.executavel()) cand.push_back(&t);

    // Ordem de decisão (req. 4.3). A chave de prioridade do algoritmo vem primeiro;
    // em caso de empate valem, NESTA ordem:
    //   (1) a que já estava executando (evita troca de contexto inútil)
    //   (2) menor prazo (deadline absoluto)
    //   (3) chegou antes (menor ingresso)
    //   (4) menor duração
    // Se TUDO empata, sobra o sorteio (5), tratado abaixo.
    auto tupla = [&](const Tarefa* t) {
        return std::make_tuple(chave(*t), t->estado == EstadoTarefa::EXECUTANDO ? 0 : 1,
                               t->deadlineAbsoluto, t->ingresso, t->duracao);
    };
    std::stable_sort(cand.begin(), cand.end(),
                     [&](const Tarefa* a, const Tarefa* b) { return tupla(a) < tupla(b); });

    Decisao d;
    int livres = ctx.qtdCpus;
    size_t i = 0;
    while (i < cand.size() && livres > 0) {
        // Grupo de tarefas totalmente empatadas.
        size_t j = i;
        while (j < cand.size() && tupla(cand[j]) == tupla(cand[i])) ++j;
        const int tam = (int)(j - i);
        if (tam <= livres) {  // todas cabem: não há o que sortear
            for (size_t k = i; k < j; ++k) d.escolhidas.push_back(cand[k]->id);
            livres -= tam;
        } else {  // empate total e faltam CPUs: SORTEIO (req. 4.3-5)
            std::vector<const Tarefa*> grupo(cand.begin() + i, cand.begin() + j);
            while (livres > 0) {
                int r = ctx.sorteio ? ctx.sorteio((int)grupo.size()) : 0;
                d.escolhidas.push_back(grupo[r]->id);
                d.sorteadas.push_back(grupo[r]->id);
                grupo.erase(grupo.begin() + r);
                --livres;
            }
        }
        i = j;
    }
    return d;
}

// ----------------------------------------------------------------------------
// Algoritmos implementados (projeto A)
// ----------------------------------------------------------------------------
namespace {

// Rate Monotonic preemptivo: prioridade FIXA; quanto MENOR o período, MAIOR a prioridade.
Decisao escalonaRM(const std::vector<Tarefa>& ts, const Contexto& ctx) {
    return escolheMelhores(ts, ctx, [](const Tarefa& t) { return (long long)t.periodo; });
}

// Earliest Deadline First preemptivo: prioridade DINÂMICA; vence o deadline absoluto mais próximo.
Decisao escalonaEDF(const std::vector<Tarefa>& ts, const Contexto& ctx) {
    return escolheMelhores(ts, ctx, [](const Tarefa& t) { return (long long)t.deadlineAbsoluto; });
}

// >>> Para adicionar um algoritmo novo: escreva a função acima e acrescente UMA linha aqui. <<<
RegistraEscalonador registraRM("RM", escalonaRM);
RegistraEscalonador registraEDF("EDF", escalonaEDF);

}  // namespace
