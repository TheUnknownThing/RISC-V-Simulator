#ifndef CORE_PREDICTOR_HPP
#define CORE_PREDICTOR_HPP

#include <cstdint>
struct PredictorInstruction {
  uint32_t pc;
  bool taken;
};

class Predictor {
  PredictorInstruction instruction;
  uint32_t cycle_remaining;
public:
  Predictor();
  void predict();
  void update(bool taken);
  bool is_available() const;
  void set_instruction(PredictorInstruction instruction);
  bool get_prediction() const;
  void tick();

private:
  enum class State { STRONG_TAKEN, WEAK_TAKEN, WEAK_NOT_TAKEN, STRONG_NOT_TAKEN };
  State state;
};

inline Predictor::Predictor() : state(State::STRONG_NOT_TAKEN), cycle_remaining(0) {}

inline void Predictor::update(bool taken) {
  if (state == State::STRONG_TAKEN && taken) {
    state = State::STRONG_TAKEN;
  } else if (state == State::WEAK_TAKEN && taken) {
    state = State::STRONG_TAKEN;
  } else if (state == State::WEAK_NOT_TAKEN && !taken) {
    state = State::STRONG_NOT_TAKEN;
  } else if (state == State::STRONG_NOT_TAKEN && !taken) {
    state = State::WEAK_NOT_TAKEN;
  }
}

inline bool Predictor::is_available() const {
  return cycle_remaining == 0;
}

inline bool Predictor::get_prediction() const {
  if (cycle_remaining == 0) {
    return state == State::STRONG_TAKEN || state == State::WEAK_TAKEN;
  }
  return false;
}

inline void Predictor::set_instruction(PredictorInstruction instruction) {
  this->instruction = instruction;
  cycle_remaining = 1;
}

inline void Predictor::tick() {
  if (cycle_remaining > 0) {
    cycle_remaining--;
  }
}

#endif