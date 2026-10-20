#pragma once
// ============================================================================
// cpu.hpp - Um processador simulado.
// A fila de prontos NÃO fica aqui: ela é GLOBAL do sistema (req. 1.2), isto é,
// é o conjunto de tarefas PRONTAS dentro do EstadoSistema (ver simulador.hpp).
// Uma CPU sem tarefa fica DESLIGADA (ligada() == false).
// ============================================================================
struct CPU {
    int id = 0;
    int tarefaId = -1;        // tarefa em execução, ou -1
    int ticksDesligada = 0;   // total de ticks em que ficou desligada (informado ao usuário)

    bool ligada() const { return tarefaId >= 0; }
};
