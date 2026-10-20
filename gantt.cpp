// gantt.cpp - monta o layout do Gantt e gera SVG e texto. (Explicação geral em gantt.hpp)
#include "header/gantt.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace gantt {
namespace {

const int ALT = 28;   // altura de cada linha do gráfico
const int TOPO = 30;  // espaço acima da primeira linha

Forma nova(TipoForma t, double x, double y, double w, double h, const std::string& cor = "000000",
           const std::string& texto = "", int alinha = 1) {
    Forma f;
    f.tipo = t; f.x = x; f.y = y; f.w = w; f.h = h; f.cor = cor; f.texto = texto; f.alinha = alinha;
    return f;
}

// Letra preta em fundo claro, branca em fundo escuro.
std::string corTexto(const std::string& hex) {
    int r = std::stoi(hex.substr(0, 2), nullptr, 16), g = std::stoi(hex.substr(2, 2), nullptr, 16),
        b = std::stoi(hex.substr(4, 2), nullptr, 16);
    return (0.299 * r + 0.587 * g + 0.114 * b > 150) ? "000000" : "FFFFFF";
}

}  // namespace

Layout montarLayout(const DadosGantt& d, int ate, int desde) {
    Layout L;
    const int n = std::min<int>(ate, (int)d.amostras.size());
    desde = std::max(0, std::min(desde, n));
    auto X = [&](int t) { return MARG_ESQ + (t - desde) * LARG_TICK; };  // posição horizontal de um instante
    auto add = [&](const Forma& f) { L.formas.push_back(f); };

    // Req. 2.5: o MENOR id fica embaixo (perto do eixo X); por isso a ordem de cima para baixo é decrescente.
    std::vector<InfoTarefa> ordem = d.tarefas;
    std::sort(ordem.begin(), ordem.end(), [](const InfoTarefa& a, const InfoTarefa& b) { return a.id > b.id; });
    std::map<int, int> linha;
    std::map<int, std::string> cor;
    for (size_t i = 0; i < ordem.size(); ++i) { linha[ordem[i].id] = (int)i; cor[ordem[i].id] = ordem[i].cor; }
    auto Y = [&](int id) { return TOPO + linha[id] * ALT; };  // topo da linha da tarefa
    const int yEixo = TOPO + (int)ordem.size() * ALT;

    // Rótulos e grade.
    for (size_t i = 0; i < ordem.size(); ++i) {
        int y = TOPO + (int)i * ALT;
        add(nova(TipoForma::TEXTO, MARG_ESQ - 8, y + ALT / 2, 0, 0, "000000", "T" + std::to_string(ordem[i].id), 2));
        add(nova(TipoForma::LINHA, MARG_ESQ, y + ALT, X(n) - MARG_ESQ, 0, "DDDDDD"));
    }
    for (int t = desde; t <= n; ++t) {
        add(nova(TipoForma::LINHA, X(t), TOPO, 0, yEixo - TOPO, "EEEEEE"));
        add(nova(TipoForma::TEXTO, X(t), yEixo + 10, 0, 0, "000000", std::to_string(t)));
    }
    add(nova(TipoForma::LINHA, MARG_ESQ, yEixo, X(n) - MARG_ESQ, 0));

    // Um quadrado por tick: colorido = executando (o rótulo C0, C1... é a CPU); pontilhado = suspensa;
    // sem nada = pronta. Abaixo, uma faixa por CPU (cinza = CPU desligada).
    for (int t = desde; t < n; ++t) {
        const Amostra& a = d.amostras[t];
        for (size_t c = 0; c < a.cpu.size(); ++c) {
            int id = a.cpu[c];
            int yCpu = yEixo + 25 + (int)c * ALT;
            if (id < 0) {
                add(nova(TipoForma::RETANGULO, X(t), yCpu, LARG_TICK, ALT - 4, "CCCCCC"));
                continue;
            }
            add(nova(TipoForma::RETANGULO, X(t), Y(id) + 3, LARG_TICK, ALT - 6, cor[id]));
            add(nova(TipoForma::TEXTO, X(t) + LARG_TICK / 2, Y(id) + ALT / 2, 0, 0, corTexto(cor[id]), "C" + std::to_string(c)));
            add(nova(TipoForma::RETANGULO, X(t), yCpu, LARG_TICK, ALT - 4, cor[id]));
            add(nova(TipoForma::TEXTO, X(t) + LARG_TICK / 2, yCpu + (ALT - 4) / 2, 0, 0, corTexto(cor[id]), "T" + std::to_string(id)));
        }
        for (int id : a.suspensas)
            add(nova(TipoForma::PONTILHADO, X(t), Y(id) + 3, LARG_TICK, ALT - 6));
    }

    // Eventos: cada tipo tem um símbolo e uma cor própria, no instante exato em que acontece.
    for (const Evento& e : d.eventos) {
        if (e.instante < desde || e.instante > n || !linha.count(e.idTarefa)) continue;
        int x = X(e.instante), y = Y(e.idTarefa) + ALT / 2;
        switch (e.tipo) {
            case TipoEvento::CHEGADA:     add(nova(TipoForma::TEXTO, x - 5, y, 0, 0, "008800", "▲")); break;
            case TipoEvento::FIM:         add(nova(TipoForma::TEXTO, x + 5, y, 0, 0, "0046C8", "▼")); break;
            case TipoEvento::PERDA_PRAZO: add(nova(TipoForma::TEXTO, x, y, 0, 0, "D20000", "X")); break;
            case TipoEvento::SORTEIO:     add(nova(TipoForma::TEXTO, x, y, 0, 0, "BE00BE", "◆")); break;
        }
    }

    // Legenda.
    int yLeg = yEixo + 25 + d.qtdCpus * ALT + 10;
    const char* simb[4] = {"▲", "▼", "X", "◆"}, *cores[4] = {"008800", "0046C8", "D20000", "BE00BE"};
    const char* nomes[4] = {"chegada", "término", "prazo perdido", "sorteio (desempate)"};
    for (int i = 0; i < 4; ++i) {
        add(nova(TipoForma::TEXTO, 12 + i * 130, yLeg, 0, 0, cores[i], simb[i]));
        add(nova(TipoForma::TEXTO, 22 + i * 130, yLeg, 0, 0, "000000", nomes[i], 0));
    }
    add(nova(TipoForma::PONTILHADO, 12, yLeg + 14, 18, 14));
    add(nova(TipoForma::TEXTO, 36, yLeg + 21, 0, 0, "000000", "= suspensa     C0, C1... = CPU em que a tarefa executa     cinza = CPU desligada     sem cor = pronta", 0));

    L.largura = std::max(640, X(n) + 20);
    L.altura = yLeg + 40;
    return L;
}

// ---------------------------------------------------------------- SVG
std::string gerarSvg(const Layout& l) {
    std::ostringstream o;
    o << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << l.largura
      << "\" height=\"" << l.altura << "\" font-family=\"Arial\" font-size=\"12\">\n"
      // padrão de pontos usado nas tarefas suspensas
      << "<defs><pattern id=\"pontos\" width=\"6\" height=\"6\" patternUnits=\"userSpaceOnUse\"><rect width=\"6\" height=\"6\" fill=\"white\"/>"
         "<circle cx=\"3\" cy=\"3\" r=\"1.3\" fill=\"black\"/></pattern></defs>\n<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    for (const Forma& f : l.formas) {
        const std::string c = "#" + f.cor;
        if (f.tipo == TipoForma::LINHA)
            o << "<line x1=\"" << f.x << "\" y1=\"" << f.y << "\" x2=\"" << f.x + f.w << "\" y2=\"" << f.y + f.h << "\" stroke=\"" << c << "\"/>\n";
        else if (f.tipo == TipoForma::TEXTO)
            o << "<text x=\"" << f.x << "\" y=\"" << f.y << "\" text-anchor=\"" << (f.alinha == 0 ? "start" : f.alinha == 1 ? "middle" : "end")
              << "\" dominant-baseline=\"central\" fill=\"" << c << "\">" << f.texto << "</text>\n";  // nossos textos não têm & nem <
        else
            o << "<rect x=\"" << f.x << "\" y=\"" << f.y << "\" width=\"" << f.w << "\" height=\"" << f.h << "\" fill=\""
              << (f.tipo == TipoForma::PONTILHADO ? "url(#pontos)" : c) << "\" stroke=\"black\"/>\n";
    }
    o << "</svg>\n";
    return o.str();
}

bool salvarSvg(const DadosGantt& d, const std::string& caminho) {
    std::ofstream arq(std::filesystem::u8path(caminho), std::ios::binary);  // u8path: aceita acentos no caminho (Windows)
    if (!arq) return false;
    arq << gerarSvg(montarLayout(d, (int)d.amostras.size()));  // todos os ticks, sem limite
    return (bool)arq;
}

// ---------------------------------------------------------------- Texto (terminal)
std::string gerarAscii(const DadosGantt& d, int ate, int desde) {
    const int n = std::min<int>(ate, (int)d.amostras.size());
    desde = std::max(0, std::min(desde, n));
    auto celula = [](std::string s) { s.resize(4, ' '); return s; };  // cada tick ocupa 4 caracteres
    auto rotulo = [](std::string s) { s.resize(6, ' '); return s; };

    std::vector<InfoTarefa> ordem = d.tarefas;
    std::sort(ordem.begin(), ordem.end(), [](const InfoTarefa& a, const InfoTarefa& b) { return a.id > b.id; });

    std::ostringstream o;
    if (desde > 0) o << "(ticks " << desde << " a " << n << "; o comando svg mostra tudo)\n";
    o << rotulo("tick");
    for (int t = desde; t <= n; ++t) o << celula(std::to_string(t));
    o << "\n";
    const char* SIMB = "^v!?";  // chegada, término, prazo perdido, sorteio
    for (const InfoTarefa& tf : ordem) {
        o << rotulo("T" + std::to_string(tf.id) + " |");
        for (int t = desde; t <= n; ++t) {
            char marca = ' ';  // evento deste instante ('*' = mais de um)
            for (const Evento& e : d.eventos)
                if (e.idTarefa == tf.id && e.instante == t) marca = (marca == ' ') ? SIMB[(int)e.tipo] : '*';
            std::string conteudo;  // #N = executando na CPU N; ~~ = suspensa
            if (t < n) {
                for (size_t c = 0; c < d.amostras[t].cpu.size(); ++c)
                    if (d.amostras[t].cpu[c] == tf.id) conteudo = "#" + std::to_string(c);
                for (int s : d.amostras[t].suspensas)
                    if (s == tf.id) conteudo = "~~";
            }
            o << celula(std::string(1, marca) + conteudo);
        }
        o << "\n";
    }
    for (int c = 0; c < d.qtdCpus; ++c) {
        o << rotulo("CPU" + std::to_string(c) + " |");
        for (int t = desde; t < n; ++t) {
            int id = c < (int)d.amostras[t].cpu.size() ? d.amostras[t].cpu[c] : -1;
            o << celula(" " + (id >= 0 ? "T" + std::to_string(id) : std::string("--")));
        }
        o << "\n";
    }
    o << "Legenda: #N executando na CPU N | ~~ suspensa | -- CPU desligada | ^ chegada  v termino  ! prazo perdido  ? sorteio  * varios\n";
    return o.str();
}

}  // namespace gantt
