#pragma once
// ============================================================================
// tarefa.hpp - TCB (Task Control Block): TODAS as informações de uma tarefa
// (antes, durante e depois da simulação) ficam nesta única estrutura (req. 1.3).
//
// DECISÃO: é um struct "valor" (sem ponteiros) para que o estado do sistema inteiro
// possa ser copiado a cada tick. Essas cópias formam o HISTÓRICO usado para
// retroceder/avançar a simulação (req. 1.5.2).
// ============================================================================
#include <string>
#include <vector>

// Estados possíveis de uma tarefa.
//  NOVA                : ainda não chegou o instante de ingresso
//  PRONTA              : tem uma ativação pendente e espera uma CPU (fila de prontos)
//  EXECUTANDO          : está em uma CPU
//  SUSPENSA            : bloqueada (hoje só por edição manual; no projeto B: mutex, E/S...)
//  AGUARDANDO_PERIODO  : terminou uma ativação e espera a próxima (tarefas periódicas)
//  TERMINADA           : executou todas as ativações (MAX_ATIVACOES) ou foi encerrada
enum class EstadoTarefa { NOVA, PRONTA, EXECUTANDO, SUSPENSA, AGUARDANDO_PERIODO, TERMINADA };

const char* nomeEstado(EstadoTarefa e);

// Tarefas periódicas nunca terminam por definição; para a simulação acabar,
// assume-se que terminam após 10 execuções (req. 4.4).
constexpr int MAX_ATIVACOES = 10;

struct Tarefa {
    // ---- Parâmetros (vêm do arquivo de configuração) ----
    int id = 0;
    std::string cor = "808080";  // RRGGBB
    int ingresso = 0;            // instante da primeira ativação
    int duracao = 1;             // tempo de CPU necessário por ativação
    int periodo = 0;             // > 0: periódica (única suportada no projeto A)
    int prazo = 0;               // deadline RELATIVO ao instante de cada ativação
    std::string listaEventos;    // guardada crua; será interpretada no projeto B

    // ---- Estado dinâmico ----
    EstadoTarefa estado = EstadoTarefa::NOVA;
    int cpu = -1;                // CPU onde executa (-1 = nenhuma)
    int restante = 0;            // ticks de CPU que faltam na ativação atual (0 = sem ativação)
    int ativacaoAtual = -1;      // instante em que a ativação atual foi liberada
    int deadlineAbsoluto = 0;    // ativacaoAtual + prazo
    int proximaAtivacao = 0;     // instante da próxima liberação (-1 = não há mais)
    int liberadas = 0;           // quantas ativações já chegaram
    int concluidas = 0;          // quantas ativações já terminaram
    std::vector<int> pendentes;  // liberações que chegaram com a ativação anterior ainda incompleta
    bool perdeuPrazo = false;    // a ativação atual já estourou o prazo?

    // ---- Estatísticas ----
    int tempoExecutado = 0;
    int perdasPrazo = 0;
    int preempcoes = 0;

    bool temAtivacao() const { return restante > 0; }
    // Pode ser escolhida pelo escalonador? (suspensa/terminada/aguardando não podem)
    bool executavel() const {
        return restante > 0 && (estado == EstadoTarefa::PRONTA || estado == EstadoTarefa::EXECUTANDO);
    }
};
