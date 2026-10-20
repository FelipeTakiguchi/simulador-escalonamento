// configuracao.cpp - lê e valida o arquivo de configuração (formato em configuracao.hpp).
// Regras: espaços e linhas em branco são ignorados; maiúsculas/minúsculas valem igual;
// o ';' no fim da linha é opcional; campo vazio = valor padrão; erros trazem o número da linha.
#include "header/configuracao.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "header/escalonador.hpp"
#include "header/util.hpp"

namespace {

// Cores usadas quando o arquivo não informa a cor da tarefa (todas diferentes).
const char* PALETA[10] = {"E6194B", "3CB44B", "4363D8", "F58231", "911EB4", "42D4F4", "F032E6", "BFEF45", "FABED4", "469990"};

std::string pref(int n) { return n > 0 ? "Linha " + std::to_string(n) + ": " : ""; }

// Lê um inteiro >= minimo. Texto vazio = valor padrão. Se for inválido, registra o erro.
void lerInt(const std::string& txt, int padrao, int minimo, const char* nome, int n, int& out, std::vector<std::string>& erros) {
    out = padrao;
    if (txt.empty()) return;
    if (!paraInt(txt, out))
        erros.push_back(pref(n) + "o campo '" + nome + "' = '" + txt + "' não é um número inteiro.");
    else if (out < minimo)
        erros.push_back(pref(n) + "o campo '" + nome + "' = " + txt + " é inválido (mínimo " + std::to_string(minimo) + ").");
}

// 1ª linha do arquivo: algoritmo;quantum;qtde_cpus
void analisaSistema(const std::string& linha, int n, Configuracao& cfg) {
    auto f = divide(linha, ';');
    for (auto& x : f) x = trim(x);
    f.resize(std::max<size_t>(f.size(), 3));
    if (!f[0].empty()) {
        std::string alg = maiusculas(f[0]);
        if (algoritmoExiste(alg)) cfg.algoritmo = alg;
        else cfg.erros.push_back(pref(n) + "algoritmo '" + f[0] + "' desconhecido. Disponíveis: " + listaAlgoritmos() + ".");
    }
    lerInt(f[1], 0, 0, "quantum", n, cfg.quantum, cfg.erros);
    lerInt(f[2], 1, 1, "qtde_cpus", n, cfg.qtdCpus, cfg.erros);
    if (cfg.qtdCpus > MAX_CPUS) cfg.erros.push_back(pref(n) + "qtde_cpus maior que o máximo (" + std::to_string(MAX_CPUS) + ").");
}

}  // namespace

std::string corPadrao(int i) { return PALETA[((i % 10) + 10) % 10]; }

// Aceita "f0e0d0" ou "#F0E0D0"; devolve false se não forem 6 dígitos hexadecimais.
bool normalizaCor(std::string& c) {
    c = trim(c);
    if (!c.empty() && c[0] == '#') c.erase(0, 1);
    if (c.size() != 6) return false;
    for (char ch : c)
        if (!std::isxdigit((unsigned char)ch)) return false;
    c = maiusculas(c);
    return true;
}

// Uma linha de tarefa: id;cor;ingresso;duracao;periodo;prazo;lista_eventos
bool analisaLinhaTarefa(const std::string& linha, int n, std::set<int>& ids, int indiceCor, Tarefa& t,
                        std::vector<std::string>& erros, std::vector<std::string>& avisos, bool& ignorada) {
    ignorada = false;
    const size_t erros0 = erros.size();
    auto f = divide(linha, ';');
    for (auto& x : f) x = trim(x);
    f.resize(std::max<size_t>(f.size(), 6));

    Tarefa k;
    if (!paraInt(f[0], k.id) || k.id < 0) erros.push_back(pref(n) + "id '" + f[0] + "' inválido (use um inteiro >= 0).");
    else if (ids.count(k.id)) erros.push_back(pref(n) + "id " + f[0] + " repetido: cada tarefa precisa de um id único.");

    k.cor = f[1].empty() ? corPadrao(indiceCor) : f[1];
    if (!normalizaCor(k.cor)) erros.push_back(pref(n) + "cor '" + f[1] + "' inválida (use 6 dígitos hexadecimais, ex.: F0E0D0).");

    lerInt(f[2], 0, 0, "ingresso", n, k.ingresso, erros);
    lerInt(f[3], 1, 1, "duracao", n, k.duracao, erros);
    lerInt(f[4], 0, 0, "periodo", n, k.periodo, erros);
    lerInt(f[5], 0, 0, "prazo", n, k.prazo, erros);
    if (k.prazo == 0) k.prazo = std::max(k.periodo, 1);  // prazo vazio (ou 0) = igual ao período
    for (size_t i = 6; i < f.size(); ++i) k.listaEventos += (i > 6 ? ";" : "") + f[i];  // projeto B: guardada crua
    k.listaEventos = trim(k.listaEventos);

    if (erros.size() > erros0) return false;
    ids.insert(k.id);
    if (k.periodo == 0) {  // aperiódica: ignorada no projeto A (req. 4.4)
        ignorada = true;
        avisos.push_back(pref(n) + "a tarefa " + std::to_string(k.id) + " é aperiódica (período 0) e foi IGNORADA: o projeto A só suporta tarefas periódicas.");
        return true;
    }
    t = k;
    return true;
}

Configuracao carregarTexto(const std::string& texto) {
    Configuracao cfg;
    std::set<int> ids;
    std::istringstream in(texto);
    std::string linha;
    int n = 0, uteis = 0, indiceCor = 0;
    while (std::getline(in, linha)) {
        ++n;
        if (n == 1 && linha.compare(0, 3, "\xEF\xBB\xBF") == 0) linha.erase(0, 3);  // marca BOM que alguns editores gravam
        if (trim(linha).empty()) continue;                                         // linha em branco não é erro
        if (++uteis == 1) { analisaSistema(linha, n, cfg); continue; }
        Tarefa t;
        bool ignorada;
        if (analisaLinhaTarefa(linha, n, ids, indiceCor++, t, cfg.erros, cfg.avisos, ignorada) && !ignorada) cfg.tarefas.push_back(t);
    }
    if (uteis < 2) cfg.erros.push_back("O arquivo precisa de ao menos 2 linhas: parâmetros do sistema e uma tarefa.");
    else if (cfg.tarefas.empty() && cfg.erros.empty()) cfg.erros.push_back("Nenhuma tarefa periódica válida no arquivo.");

    std::set<std::string> vistas;  // cada tarefa deve ter uma cor diferente (req. 2.1)
    for (const Tarefa& t : cfg.tarefas)
        if (!vistas.insert(t.cor).second) cfg.avisos.push_back("A cor " + t.cor + " da tarefa " + std::to_string(t.id) + " repete a de outra tarefa.");
    return cfg;
}

Configuracao carregarArquivo(const std::string& caminhoBruto) {
    std::string caminho = trim(caminhoBruto);
    if (caminho.size() >= 2 && caminho.front() == '"' && caminho.back() == '"') caminho = caminho.substr(1, caminho.size() - 2);  // aspas do "arrastar arquivo"
    std::ifstream arq(std::filesystem::u8path(caminho), std::ios::binary);  // qualquer pasta/nome; u8path aceita acentos
    if (!arq) {
        Configuracao cfg;
        cfg.erros.push_back("Não foi possível abrir '" + caminho + "'. Confira o caminho e o nome do arquivo.");
        return cfg;
    }
    std::ostringstream conteudo;
    conteudo << arq.rdbuf();
    return carregarTexto(conteudo.str());
}
