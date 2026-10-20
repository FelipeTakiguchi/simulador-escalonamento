// ============================================================================
// main.cpp - Interface com o usuário (terminal; janela gráfica opcional no Windows).
// Uso:  simulador [arquivo_config] [--passo | --completo] [--janela]
//   passo-a-passo: avançar/voltar, inspecionar e editar (digite 'ajuda')
//   completo     : roda tudo e mostra só o resultado final
// ============================================================================
#include <iostream>
#include <sstream>
#include <string>

#include "header/configuracao.hpp"
#include "header/gantt.hpp"
#include "header/simulador.hpp"
#include "header/util.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

using namespace std;

namespace {

// Configuração de exemplo embutida (passa pelo mesmo leitor do arquivo).
const char* EXEMPLO = "EDF;0;2\n1;E6194B;0;2;6;6\n2;4363D8;0;1;4;4\n3;3CB44B;1;3;8;8\n4;F58231;2;4;12;12\n";

const char* AJUDA =
    "COMANDOS (ENTER = avançar 1 tick)\n"
    "  n [K]      avança K ticks          b [K]   volta K ticks        ir T   vai ao instante T\n"
    "  fim        executa até o final     estado  mostra CPUs, fila de prontos e tarefas\n"
    "  tarefa ID  detalhes de uma tarefa  gantt   mostra o gráfico     janela abre a janela (Windows)\n"
    "  svg [arq]  salva a imagem SVG      sair\n"
    "EDIÇÃO (vale a partir do instante atual)\n"
    "  mod tarefa ID CAMPO VALOR   CAMPO: estado | restante | duracao | periodo | prazo | ingresso | cor\n"
    "                              ex.: mod tarefa 2 estado SUSPENSA\n"
    "  mod sistema CAMPO VALOR     CAMPO: algoritmo | quantum | cpus (cpus só no instante 0)\n"
    "  nova LINHA                  ex.: nova 5;00FFFF;10;2;8;8        remover ID\n";

void mostraGantt(Simulador& sim) { cout << "\n" << gantt::gerarAscii(sim.dados(), sim.tickAtual(), max(0, sim.tickAtual() - 40)); }

void salvaSvg(Simulador& sim, const string& arquivo) {
    if (gantt::salvarSvg(sim.dados(), arquivo)) cout << "Imagem salva em: " << arquivo << "\n";
    else cout << "ERRO: não foi possível gravar '" << arquivo << "'.\n";
}

// No fim da simulação: mostra o resultado e grava a imagem completa (req. 2.4).
void verificaFim(Simulador& sim) {
    if (sim.tickAtual() == sim.maiorTickCalculado() && sim.situacao() != Situacao::EM_ANDAMENTO) {
        cout << "\n*** Fim da simulação: " << descricaoSituacao(sim.situacao()) << " ***\n" << sim.relatorioFinal();
        salvaSvg(sim, "gantt_final.svg");
    }
}

void avanca(Simulador& sim, int k) {
    int de = sim.tickAtual(), feitos = 0;
    while (feitos < k && sim.avancar()) ++feitos;
    if (feitos == 0) { cout << "Não é possível avançar: simulação " << descricaoSituacao(sim.situacao()) << ".\n"; return; }
    cout << "Instante " << de << " -> " << sim.tickAtual() << "\n" << sim.descricaoEventos(de, sim.tickAtual());
    mostraGantt(sim);
    verificaFim(sim);
}

void abreJanela(Simulador& sim) {
    gantt::Controles c;
    c.dados = [&]() -> const gantt::DadosGantt& { return sim.dados(); };
    c.tickAtual = [&]() { return sim.tickAtual(); };
    c.avancar = [&]() { if (sim.avancar()) verificaFim(sim); };
    c.voltar = [&]() { sim.voltar(); };
    c.executarTudo = [&]() { while (sim.avancar()) {} verificaFim(sim); };
    if (!gantt::abrirJanela(c)) cout << "A janela só existe no Windows. Use o gráfico do terminal e o comando 'svg'.\n";
}

void mostra(const Resultado& r) { cout << (r.ok ? "OK: " : "NÃO FOI POSSÍVEL: ") << r.msg << "\n"; }

// Executa um comando; devolve false para sair.
bool comando(Simulador& sim, const string& cmd, const string& resto) {
    int k = 1;
    istringstream is(resto);
    if (cmd == "sair") return false;
    else if (cmd == "ajuda") cout << AJUDA;
    else if (cmd == "n") avanca(sim, paraInt(resto, k) && k > 0 ? k : 1);
    else if (cmd == "b") {
        int feitos = 0, passos = paraInt(resto, k) && k > 0 ? k : 1;
        while (feitos < passos && sim.voltar()) ++feitos;
        cout << "Agora no instante " << sim.tickAtual() << " (o relógio voltou junto).\n";
        mostraGantt(sim);
    } else if (cmd == "ir") {
        if (!paraInt(resto, k) || k < 0) cout << "Uso: ir T   (T = instante >= 0)\n";
        else { sim.irPara(k); cout << "Agora no instante " << sim.tickAtual() << ".\n"; mostraGantt(sim); verificaFim(sim); }
    } else if (cmd == "fim") { while (sim.avancar()) {} mostraGantt(sim); verificaFim(sim); }
    else if (cmd == "estado") cout << sim.estadoTexto();
    else if (cmd == "tarefa") cout << (paraInt(resto, k) ? sim.tarefaTexto(k) : "Uso: tarefa ID\n");
    else if (cmd == "gantt") mostraGantt(sim);
    else if (cmd == "janela") abreJanela(sim);
    else if (cmd == "svg") salvaSvg(sim, resto.empty() ? "gantt.svg" : resto);
    else if (cmd == "nova") mostra(sim.adicionarTarefa(resto));
    else if (cmd == "remover") { if (paraInt(resto, k)) mostra(sim.removerTarefa(k)); else cout << "Uso: remover ID\n"; }
    else if (cmd == "mod") {
        string alvo, a, b, valor;
        is >> alvo;
        if (minusculas(alvo) == "tarefa") {
            is >> a >> b;  // a = id, b = campo
            getline(is, valor);
            if (paraInt(a, k) && !b.empty() && !trim(valor).empty()) mostra(sim.editarTarefa(k, b, valor));
            else cout << "Uso: mod tarefa ID CAMPO VALOR\n";
        } else if (minusculas(alvo) == "sistema") {
            is >> a;  // a = campo
            getline(is, valor);
            if (!a.empty() && !trim(valor).empty()) mostra(sim.editarSistema(a, valor));
            else cout << "Uso: mod sistema CAMPO VALOR\n";
        } else cout << "Uso: mod tarefa ID CAMPO VALOR   ou   mod sistema CAMPO VALOR\n";
    } else cout << "Comando '" << cmd << "' desconhecido. Digite 'ajuda'.\n";
    return true;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // acentos corretos no terminal do Windows
    SetConsoleCP(CP_UTF8);
#endif
    string caminho;
    bool completo = false, passo = false, janela = false;
    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        if (a == "--completo") completo = true;
        else if (a == "--passo") passo = true;
        else if (a == "--janela") janela = true;
        else caminho = a;
    }
    cout << "=== Simulador de SO multitarefa (algoritmos: " << listaAlgoritmos() << ") ===\n";

    // Carrega a configuração; se vier erro e o programa for interativo, pergunta de novo.
    Configuracao cfg;
    while (true) {
        string l = caminho;
        if (caminho.empty()) {
            cout << "\nArquivo de configuração (ENTER = exemplo embutido): " << flush;
            if (!getline(cin, l)) return 1;
        }
        cfg = trim(l).empty() ? carregarTexto(EXEMPLO) : carregarArquivo(l);
        for (const string& a : cfg.avisos) cout << "  AVISO: " << a << "\n";
        for (const string& e : cfg.erros) cout << "  ERRO: " << e << "\n";
        if (cfg.ok()) break;
        if (!caminho.empty()) return 1;
    }
    cout << "Carregado: " << cfg.algoritmo << ", " << cfg.qtdCpus << " CPU(s), " << cfg.tarefas.size() << " tarefa(s) periódica(s).\n";

    if (!completo && !passo) {
        string l;
        cout << "Modo: [1] passo-a-passo  [2] execução completa (ENTER = 1): " << flush;
        if (!getline(cin, l)) return 1;
        completo = (trim(l) == "2");
    }

    Simulador sim(cfg);
    if (completo) {  // execução completa: só o resultado final
        while (sim.avancar()) {}
        cout << "\n" << (sim.tickAtual() <= 100 ? gantt::gerarAscii(sim.dados(), sim.tickAtual()) : "(gráfico longo: veja a imagem SVG)\n");
        verificaFim(sim);
        return 0;
    }

    cout << "\nPassos: ENTER avança 1 tick; digite 'ajuda' para ver os comandos.\n\n" << sim.estadoTexto();
    if (janela) abreJanela(sim);
    string linha;
    while (cout << "\nsim[t=" << sim.tickAtual() << "]> " << flush, getline(cin, linha)) {
        istringstream is(trim(linha));
        string cmd, resto;
        is >> cmd;
        getline(is, resto);
        if (cmd.empty()) avanca(sim, 1);
        else if (!comando(sim, minusculas(cmd), trim(resto))) break;
    }
    return 0;
}
