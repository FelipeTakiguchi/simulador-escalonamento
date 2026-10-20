#include "header/relogio.hpp"

int Relogio::tick() const { return tick_; }
void Relogio::incrementa() { ++tick_; }
void Relogio::decrementa() { if (tick_ > 0) --tick_; }
void Relogio::ajusta(int t) { tick_ = t < 0 ? 0 : t; }
