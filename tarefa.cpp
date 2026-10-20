#include "header/tarefa.hpp"

const char* nomeEstado(EstadoTarefa e) {
    switch (e) {
        case EstadoTarefa::NOVA: return "NOVA";
        case EstadoTarefa::PRONTA: return "PRONTA";
        case EstadoTarefa::EXECUTANDO: return "EXECUTANDO";
        case EstadoTarefa::SUSPENSA: return "SUSPENSA";
        case EstadoTarefa::AGUARDANDO_PERIODO: return "AGUARDANDO";
        case EstadoTarefa::TERMINADA: return "TERMINADA";
    }
    return "?";
}
