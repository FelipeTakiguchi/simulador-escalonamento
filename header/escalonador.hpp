#pragma once
// ============================================================================
// escalonador.hpp - Interface dos algoritmos de escalonamento.
//
// DECISÃO (req. 4.2): um algoritmo é apenas UMA FUNÇÃO que, dadas as tarefas e o
// contexto, devolve quais tarefas devem estar executando agora. A simulação NÃO
// conhece algoritmos específicos: ela busca o nome (ex.: "RM") no registro.
// Para criar um novo algoritmo basta escrever uma função e registrá-la com UMA
// linha (ver o fim de escalonador.cpp); nenhum código da simulação muda.
// ============================================================================
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "tarefa.hpp"

// Informações extras que o algoritmo pode usar.
struct Contexto {
    int tick = 0;
    int qtdCpus = 1;
    int quantum = 0;                  // 0 = sem quantum (RM/EDF não o utilizam)
    std::function<int(int)> sorteio;  // sorteio(n) -> inteiro em [0, n); determinístico (semente no estado)
};

// Resultado de uma decisão.
struct Decisao {
    std::vector<int> escolhidas;  // ids das tarefas que devem executar (no máximo qtdCpus), em ordem de prioridade
    std::vector<int> sorteadas;   // ids que foram escolhidos por SORTEIO (desempate final, req. 4.3-5)
};

using FuncEscalonador = std::function<Decisao(const std::vector<Tarefa>&, const Contexto&)>;

// Registro nome (MAIÚSCULAS) -> função.
std::map<std::string, FuncEscalonador>& registroEscalonadores();
bool algoritmoExiste(const std::string& nome);  // aceita qualquer combinação de maiúsculas/minúsculas
std::string listaAlgoritmos();                   // "EDF, RM"

// Registro automático: declarar um objeto estático registra o algoritmo.
struct RegistraEscalonador {
    RegistraEscalonador(const std::string& nome, FuncEscalonador f);
};

// Ferramenta para escrever novos algoritmos baseados em prioridade: basta fornecer a
// "chave" (MENOR chave = MAIOR prioridade). Aplica toda a regra de desempate do req. 4.3.
Decisao escolheMelhores(const std::vector<Tarefa>& tarefas, const Contexto& ctx,
                        const std::function<long long(const Tarefa&)>& chave);
