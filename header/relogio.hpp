#pragma once
// ============================================================================
// relogio.hpp - Relógio global do sistema simulado, em ticks (req. 1.1).
//
// DECISÃO: NÃO é singleton. Ele faz parte do EstadoSistema, que é copiado a cada
// tick para formar o histórico; um singleton global ficaria fora dessas cópias e
// quebraria o "retroceder". O tempo simulado não depende do tempo real.
// ============================================================================
class Relogio {
private:
    int tick_ = 0;

public:
    int tick() const;
    void incrementa();   // avança 1 tick
    void decrementa();   // retrocede 1 tick (nunca abaixo de 0)
    void ajusta(int t);  // salto direto (ir para um tick)
};
