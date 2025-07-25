#ifndef CORE_PREDICTOR_HPP
#define CORE_PREDICTOR_HPP

#include <cstdint>
#include <optional>
#include <stdexcept>

struct PredictorInstruction {
  uint32_t pc;
};

struct PredictorResult {
  bool prediction;
  uint32_t pc;
};

class Predictor {
  std::optional<PredictorInstruction> current_instruction;
  std::optional<PredictorResult> broadcast_result;
  std::optional<PredictorResult> next_broadcast_result;
  bool busy;

public:
  Predictor();
  void update(bool taken);
  bool is_available() const;
  void set_instruction(PredictorInstruction instruction);
  bool has_result_for_broadcast() const;
  PredictorResult get_result_for_broadcast() const;
  void tick();

private:
  enum class State {
    STRONG_TAKEN,
    WEAK_TAKEN,
    WEAK_NOT_TAKEN,
    STRONG_NOT_TAKEN
  };
  State state;

  bool predict() const;
};

inline Predictor::Predictor()
    : current_instruction(std::nullopt), broadcast_result(std::nullopt),
      next_broadcast_result(std::nullopt), state(State::WEAK_NOT_TAKEN),
      busy(false) {}

inline void Predictor::update(bool taken) {
  switch (state) {
  case State::STRONG_TAKEN:
    if (!taken)
      state = State::WEAK_TAKEN;
    break;
  case State::WEAK_TAKEN:
    if (taken)
      state = State::STRONG_TAKEN;
    else
      state = State::WEAK_NOT_TAKEN;
    break;
  case State::WEAK_NOT_TAKEN:
    if (taken)
      state = State::WEAK_TAKEN;
    else
      state = State::STRONG_NOT_TAKEN;
    break;
  case State::STRONG_NOT_TAKEN:
    if (taken)
      state = State::WEAK_NOT_TAKEN;
    break;
  }
}

inline bool Predictor::is_available() const { return !busy; }

inline bool Predictor::has_result_for_broadcast() const {
  return broadcast_result.has_value();
}

inline PredictorResult Predictor::get_result_for_broadcast() const {
  if (!broadcast_result.has_value()) {
    throw std::runtime_error("No prediction result available for broadcast");
  }
  return broadcast_result.value();
}

inline void Predictor::set_instruction(PredictorInstruction instruction) {
  current_instruction = instruction;
  busy = true;
}

inline bool Predictor::predict() const {
  return state == State::STRONG_TAKEN || state == State::WEAK_TAKEN;
}

inline void Predictor::tick() {
  broadcast_result = next_broadcast_result;

  if (current_instruction.has_value()) {
    PredictorResult new_result;
    new_result.prediction = predict();
    new_result.pc = current_instruction->pc;

    next_broadcast_result = new_result;
    current_instruction = std::nullopt;
  } else {
    next_broadcast_result = std::nullopt;
    busy = false;
  }
}

#endif // CORE_PREDICTOR_HPP