#pragma once
// ============================================================================
// gantt.hpp - Gráfico de Gantt.
// IDEIA: montarLayout() transforma os dados da simulação em FORMAS simples
// (retângulo, linha, texto). Quem desenha só precisa entender essas formas:
//     gerarSvg()    -> imagem em arquivo (SVG)
//     abrirJanela() -> janela do Windows
//     gerarAscii()  -> o mesmo gráfico em texto, no terminal
// ============================================================================
#include <functional>
#include <string>
#include <vector>

namespace gantt {

// ---------- Dados que o simulador entrega ----------
enum class TipoEvento { CHEGADA, FIM, PERDA_PRAZO, SORTEIO };
struct Evento { int instante; TipoEvento tipo; int idTarefa; };  // instante = fronteira entre ticks
struct InfoTarefa { int id; std::string cor; };                  // cor = "RRGGBB"
struct Amostra {                  // o que aconteceu em UM tick
    std::vector<int> cpu;         // cpu[i] = id da tarefa na CPU i (-1 = CPU desligada)
    std::vector<int> suspensas;   // ids das tarefas suspensas
};
struct DadosGantt {
    int qtdCpus = 1;
    std::vector<InfoTarefa> tarefas;
    std::vector<Amostra> amostras;  // índice = tick
    std::vector<Evento> eventos;
};

// ---------- Formas desenháveis ----------
enum class TipoForma { RETANGULO, PONTILHADO, LINHA, TEXTO };
struct Forma {
    TipoForma tipo = TipoForma::RETANGULO;
    double x = 0, y = 0, w = 0, h = 0;  // LINHA vai de (x,y) até (x+w,y+h); TEXTO usa só x,y (y = meio)
    std::string cor = "000000";         // RRGGBB (cor do preenchimento, da linha ou do texto)
    std::string texto;
    int alinha = 1;                     // texto: 0 esquerda, 1 centro, 2 direita
};
struct Layout {
    std::vector<Forma> formas;
    int largura = 0, altura = 0;
};

const int LARG_TICK = 28;  // pixels de cada tick
const int MARG_ESQ = 50;   // espaço dos rótulos T1, T2...

// Mostra os ticks de 'desde' até 'ate' (desde = 0 mostra tudo).
Layout montarLayout(const DadosGantt& d, int ate, int desde = 0);
std::string gerarSvg(const Layout& l);
bool salvarSvg(const DadosGantt& d, const std::string& caminho);  // imagem COMPLETA (req. 2.4)
std::string gerarAscii(const DadosGantt& d, int ate, int desde = 0);

// ---------- Janela (só Windows) ----------
struct Controles {  // a janela não conhece o simulador: chama estas funções
    std::function<const DadosGantt&()> dados;
    std::function<int()> tickAtual;
    std::function<void()> avancar, voltar, executarTudo;
};
// Teclas: seta direita avança | seta esquerda volta | End executa tudo | S salva SVG.
// Bloqueia até a janela fechar. Devolve false fora do Windows.
bool abrirJanela(const Controles& c);

}  // namespace gantt
