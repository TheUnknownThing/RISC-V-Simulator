#ifndef CORE_PREDICTOR_HPP
#define CORE_PREDICTOR_HPP

#include "../riscv/instruction.hpp"
#include "../utils/logger.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <variant>
#include <sstream>
#include <iomanip>

struct PredictorInstruction {
  uint32_t pc;
  uint32_t rs1;
  uint32_t rs2;
  std::optional<uint32_t> dest_tag;
  int32_t imm;
  std::variant<riscv::I_JumpOp, riscv::J_Op, riscv::B_BranchOp> branch_type;
};

struct PredictorResult {
  bool prediction;
  uint32_t pc;
  std::optional<uint32_t> dest_tag;
  uint32_t target_pc; // predicted target address
  bool is_mispredicted; // Whether this was a misprediction
  uint32_t correct_target; // Correct target if mispredicted
};

class Predictor {
  std::optional<PredictorInstruction> current_instruction;
  std::optional<PredictorResult> broadcast_result;
  std::optional<PredictorResult> next_broadcast_result;
  bool busy;

  // Helper function for hex formatting
  std::string to_hex(uint32_t value) const {
    std::stringstream ss;
    ss << "0x" << std::hex << value;
    return ss.str();
  }

public:
  Predictor();
  void update(bool taken);
  bool is_available() const;
  void set_instruction(PredictorInstruction instruction);
  bool has_result_for_broadcast() const;
  PredictorResult get_result_for_broadcast() const;
  void tick();
  bool is_prediction_correct() const;
  uint32_t calculate_target_pc() const;

private:
  enum class State {
    STRONG_TAKEN,
    WEAK_TAKEN,
    WEAK_NOT_TAKEN,
    STRONG_NOT_TAKEN
  };
  State state;

  bool predict() const;
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
  PredictorResult result = broadcast_result.value();
  LOG_DEBUG("Returning predictor result: target=" + to_hex(result.target_pc) + ", prediction=" + std::to_string(result.prediction));
  return result;
}

inline void Predictor::set_instruction(PredictorInstruction instruction) {
  current_instruction = instruction;
  busy = true;
}

inline bool Predictor::predict() const {
  return state == State::STRONG_TAKEN || state == State::WEAK_TAKEN;
}

inline bool Predictor::is_prediction_correct() const {
  if (std::holds_alternative<riscv::B_BranchOp>(current_instruction->branch_type)) {
    auto op = std::get<riscv::B_BranchOp>(current_instruction->branch_type);
    bool should_take;
    switch (op) {
    case riscv::B_BranchOp::BEQ:
      should_take = (current_instruction->rs1 == current_instruction->rs2);
      break;
    case riscv::B_BranchOp::BNE:
      should_take = (current_instruction->rs1 != current_instruction->rs2);
      break;
    case riscv::B_BranchOp::BLT:
      should_take = (static_cast<int32_t>(current_instruction->rs1) <
                     static_cast<int32_t>(current_instruction->rs2));
      break;
    case riscv::B_BranchOp::BGE:
      should_take = (static_cast<int32_t>(current_instruction->rs1) >=
                     static_cast<int32_t>(current_instruction->rs2));
      break;
    case riscv::B_BranchOp::BLTU:
      should_take = (static_cast<uint32_t>(current_instruction->rs1) <
                     static_cast<uint32_t>(current_instruction->rs2));
      break;
    case riscv::B_BranchOp::BGEU:
      should_take = (static_cast<uint32_t>(current_instruction->rs1) >=
                     static_cast<uint32_t>(current_instruction->rs2));
      break;
    default:
      throw std::runtime_error("Invalid branch operation type");
    }
    LOG_DEBUG("Branch evaluation: rs1=" + std::to_string(current_instruction->rs1) + 
              ", rs2=" + std::to_string(current_instruction->rs2) + 
              ", should_take=" + std::to_string(should_take));
    return should_take;
  }
  return false;
}

inline bool Predictor::is_unconditional_jump(
    const std::variant<riscv::I_JumpOp, riscv::J_Op, riscv::B_BranchOp> &type)
    const {
  return std::holds_alternative<riscv::J_Op>(type) ||
         std::holds_alternative<riscv::I_JumpOp>(type);
}

inline uint32_t
Predictor::calculate_target_pc() const {
  if (std::holds_alternative<riscv::B_BranchOp>(current_instruction->branch_type) ||
      std::holds_alternative<riscv::J_Op>(current_instruction->branch_type)) {
    uint32_t target = current_instruction->pc + current_instruction->imm;
    LOG_DEBUG("Branch/JAL target calculation: " + to_hex(current_instruction->pc) + " + " + 
              std::to_string(current_instruction->imm) + " = " + to_hex(target));
    return target;
  } else if (std::holds_alternative<riscv::I_JumpOp>(current_instruction->branch_type)) {
    uint32_t target = (current_instruction->rs1 + current_instruction->imm) & ~1U;
    LOG_DEBUG("JALR target calculation: (" + std::to_string(current_instruction->rs1) + " + " + 
              std::to_string(current_instruction->imm) + ") & ~1 = " + to_hex(target));
    return target;
  }
  return current_instruction->pc + 4;
}

inline void Predictor::tick() {
  broadcast_result = next_broadcast_result;
  next_broadcast_result = std::nullopt;

  if (current_instruction.has_value()) {
    PredictorResult new_result;
    new_result.dest_tag = current_instruction->dest_tag;
    new_result.pc = current_instruction->pc;
    new_result.target_pc = calculate_target_pc();
    new_result.is_mispredicted = false;
    new_result.correct_target = new_result.target_pc;

    // JAL and JALR are always taken
    if (is_unconditional_jump(current_instruction->branch_type)) {
      new_result.prediction = true;
    } else {
      new_result.prediction = predict();
      bool actual_taken = is_prediction_correct();
      
      if (new_result.prediction != actual_taken) {
        new_result.is_mispredicted = true;
        new_result.correct_target = actual_taken ? new_result.target_pc : (new_result.pc + 4);
        LOG_DEBUG("Branch misprediction detected! Predicted: " + 
                  std::to_string(new_result.prediction) + ", Actual: " + std::to_string(actual_taken));
      }
      
      update(actual_taken);
    }
    
    LOG_DEBUG("Predictor calculating: PC=" + to_hex(current_instruction->pc) + 
              ", imm=" + std::to_string(current_instruction->imm) + 
              ", target=" + to_hex(new_result.target_pc) +
              ", mispredicted=" + std::to_string(new_result.is_mispredicted));

    next_broadcast_result = new_result;
    current_instruction = std::nullopt;
    busy = false;
  } else {
    next_broadcast_result = std::nullopt;
    busy = false;
  }
}

#endif // CORE_PREDICTOR_HPP