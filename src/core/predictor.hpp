#ifndef CORE_PREDICTOR_HPP
#define CORE_PREDICTOR_HPP

class Predictor {
public:
  Predictor();
  bool predict();
  void update(bool taken);

private:
  enum class State { STRONG_TAKEN, WEAK_TAKEN, WEAK_NOT_TAKEN, STRONG_NOT_TAKEN };
  State state;
};

inline Predictor::Predictor() : state(State::STRONG_NOT_TAKEN) {}

inline bool Predictor::predict() {
  return state == State::STRONG_TAKEN || state == State::WEAK_TAKEN;
}

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
#endif