#ifndef CORE_PREDICTOR_HPP
#define CORE_PREDICTOR_HPP

#include "../riscv/instruction.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <variant>

struct PredictorInstruction {
  uint32_t pc;
  uint32_t rs1;
  uint32_t rs2;
  uint32_t dest_tag;
  int32_t imm;
  std::variant<riscv::I_JumpOp, riscv::J_Op, riscv::B_BranchOp> branch_type;
};

struct PredictorResult {
  bool prediction;
  uint32_t pc;
  uint32_t dest_tag;
  uint32_t target_pc; // predicted target address
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
  uint32_t calculate_target_pc(const PredictorInstruction &instr) const;
  bool is_unconditional_jump(const std::variant<riscv::I_JumpOp, riscv::J_Op,
                                                riscv::B_BranchOp> &type) const;
};

inline Predictor::Predictor()
    : current_instruction(std::nullopt), broadcast_result(std::nullopt),
      next_broadcast_result(std::nullopt), busy(false),
      state(State::WEAK_NOT_TAKEN) {}

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

inline bool Predictor::is_unconditional_jump(
    const std::variant<riscv::I_JumpOp, riscv::J_Op, riscv::B_BranchOp> &type)
    const {
  return std::holds_alternative<riscv::J_Op>(type) ||
         std::holds_alternative<riscv::I_JumpOp>(type);
}

inline uint32_t
Predictor::calculate_target_pc(const PredictorInstruction &instr) const {
  if (std::holds_alternative<riscv::B_BranchOp>(instr.branch_type) ||
      std::holds_alternative<riscv::J_Op>(instr.branch_type)) {
    return instr.pc + instr.imm;
  } else if (std::holds_alternative<riscv::I_JumpOp>(instr.branch_type)) {
    return (instr.rs1 + instr.imm) & ~1;
  }
  return instr.pc + 4;
}

inline void Predictor::tick() {
  broadcast_result = next_broadcast_result;

  if (current_instruction.has_value()) {
    PredictorResult new_result;
    new_result.dest_tag = current_instruction->dest_tag;

    // JAL and JALR are always taken
    if (is_unconditional_jump(current_instruction->branch_type)) {
      new_result.prediction = true;
    } else {
      new_result.prediction = predict();
    }

    new_result.pc = current_instruction->pc;
    new_result.target_pc = calculate_target_pc(*current_instruction);

    next_broadcast_result = new_result;
    current_instruction = std::nullopt;
  } else {
    next_broadcast_result = std::nullopt;
    busy = false;
  }
}

#endif // CORE_PREDICTOR_HPP