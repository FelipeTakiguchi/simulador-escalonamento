#pragma once
// ============================================================================
// simulador.hpp - Motor da simulação.
//
// COMO O TEMPO FUNCIONA
//   hist_[t] = estado completo do sistema no instante t (antes de executar o tick t).
//   avancar()  : calcula hist_[t+1] a partir de hist_[t].
//   voltar()   : só move o cursor para hist_[t-1] (os estados já estão guardados).
//   Editar algo no instante t muda hist_[t] e APAGA o futuro, que é recalculado.
// ============================================================================
#include <cstdint>
#include <string>
#include <vector>

#include "configuracao.hpp"
#include "cpu.hpp"
#include "escalonador.hpp"
#include "gantt.hpp"
#include "relogio.hpp"
#include "tarefa.hpp"

// Tudo o que define o sistema em um instante (é copiado a cada tick para formar o histórico).
struct EstadoSistema {
    Relogio relogio;
    std::string algoritmo = "EDF";
    int quantum = 0;
    std::vector<Tarefa> tarefas;
    std::vector<CPU> cpus;
    uint32_t semente = 2463534242u;  // sorteio de desempate: fica no estado para refazer dar o mesmo resultado
    int tick() const { return relogio.tick(); }
};

enum class Situacao { EM_ANDAMENTO, CONCLUIDA, BLOQUEADA, LIMITE_ATINGIDO };
const char* descricaoSituacao(Situacao s);

struct Resultado { bool ok; std::string msg; };  // resultado de uma edição; se ok=false, msg diz o MOTIVO

class Simulador {
public:
    static constexpr int LIMITE_TICKS = 100000;  // proteção contra simulação que nunca termina

    explicit Simulador(const Configuracao& cfg);

    // Navegação no tempo
    bool avancar();
    bool voltar();
    void irPara(int tick);
    int tickAtual() const { return cur_; }
    int maiorTickCalculado() const { return (int)hist_.size() - 1; }
    Situacao situacao() const;

    // Edição (vale a partir do instante atual)
    Resultado editarTarefa(int id, const std::string& campo, const std::string& valor);
    Resultado editarSistema(const std::string& campo, const std::string& valor);
    Resultado adicionarTarefa(const std::string& linhaConfig);
    Resultado removerTarefa(int id);

    // Visualização
    const gantt::DadosGantt& dados();  // dados do Gantt até o tick atual
    std::string estadoTexto() const;
    std::string tarefaTexto(int id) const;
    std::string relatorioFinal() const;
    std::string descricaoEventos(int deTick, int ateTick) const;

private:
    void executaInstante(EstadoSistema& s);
    void iniciaAtivacao(Tarefa& k, int liberacao);
    std::vector<int> filaProntos() const;
    Resultado concluiEdicao(const std::string& msg);

    std::vector<EstadoSistema> hist_;
    std::vector<size_t> nEventos_;          // quantos eventos existiam em cada estado do histórico
    std::vector<gantt::Amostra> amostras_;  // amostras_[t] = o que aconteceu no tick t
    std::vector<gantt::Evento> eventos_;
    int cur_ = 0;                           // instante exibido
    gantt::DadosGantt visao_;
    bool visaoSuja_ = true;
};
