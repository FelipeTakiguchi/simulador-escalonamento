#pragma once
// ============================================================================
// configuracao.hpp - Leitura do arquivo de configuração (req. 3.2 e 3.3).
//
//   algoritmo_escalonamento;quantum;qtde_cpus
//   id;cor;ingresso;duracao;periodo;prazo;lista_eventos
// ============================================================================
#include <set>
#include <string>
#include <vector>

#include "tarefa.hpp"

constexpr int MAX_CPUS = 64;  // o enunciado fala em "no máximo N processadores"

struct Configuracao {
    // Valores PADRÃO (req. 3.2): são sobrescritos pelo que o arquivo informar.
    std::string algoritmo = "EDF";
    int quantum = 0;   // 0 = sem quantum
    int qtdCpus = 1;
    std::vector<Tarefa> tarefas;       // apenas as tarefas periódicas aceitas
    std::vector<std::string> avisos;   // situações não fatais (campo vazio -> padrão, aperiódica ignorada...)
    std::vector<std::string> erros;    // erros: se houver algum, a configuração NÃO pode ser usada

    bool ok() const { return erros.empty(); }
};

Configuracao carregarArquivo(const std::string& caminho);  // qualquer caminho/nome (req. 3.3.4)
Configuracao carregarTexto(const std::string& texto);

// Interpreta UMA linha de tarefa. Usada também pelo comando "nova" durante a simulação.
// numLinha = 0 quando não vem de arquivo. 'ids' guarda os ids já usados (unicidade).
// Retorna false se houve erro. 'ignorada' = true se a tarefa é aperiódica (descartada).
bool analisaLinhaTarefa(const std::string& linha, int numLinha, std::set<int>& ids, int indiceCor,
                        Tarefa& saida, std::vector<std::string>& erros,
                        std::vector<std::string>& avisos, bool& ignorada);

std::string corPadrao(int indice);
bool normalizaCor(std::string& cor);  // aceita "#f0e0d0"/"F0E0D0"; devolve false se inválida
