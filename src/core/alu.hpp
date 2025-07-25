#ifndef CORE_ALU_HPP
#define CORE_ALU_HPP
#include "../riscv/instruction.hpp"
#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <variant>

struct ALUInstruction {
  int32_t a;
  int32_t b;
  std::variant<riscv::R_ArithmeticOp, riscv::I_ArithmeticOp, riscv::U_Op> op;
  uint32_t dest_tag;
};

struct ALUResult {
  int32_t result;
  uint32_t dest_tag;
};

class ALU {
  std::optional<ALUInstruction> current_instruction;
  std::optional<ALUResult> broadcast_result;
  std::optional<ALUResult> next_broadcast_result;
  bool busy;

public:
  ALU();
  bool is_available() const;
  bool has_result_for_broadcast() const;
  void tick();
  void set_instruction(ALUInstruction instruction);
  ALUResult get_result_for_broadcast() const;

private:
  int32_t execute(
      int32_t a, int32_t b,
      std::variant<riscv::R_ArithmeticOp, riscv::I_ArithmeticOp, riscv::U_Op>
          op) const;
};

inline ALU::ALU()
    : current_instruction(std::nullopt), broadcast_result(std::nullopt),
      next_broadcast_result(std::nullopt),
      busy(false) {}

inline bool ALU::is_available() const { return !busy; }

inline bool ALU::has_result_for_broadcast() const {
  return broadcast_result.has_value();
}

inline void ALU::set_instruction(ALUInstruction instruction) {
  current_instruction = instruction;
  busy = true;
}

inline ALUResult ALU::get_result_for_broadcast() const {
  if (!broadcast_result.has_value()) {
    throw std::runtime_error("No result available for broadcast");
  }
  return broadcast_result.value();
}

inline void ALU::tick() {
  broadcast_result = next_broadcast_result;

  if (current_instruction.has_value()) {
    ALUResult new_result;
    new_result.result = execute(current_instruction->a, current_instruction->b,
                                current_instruction->op);
    new_result.dest_tag = current_instruction->dest_tag;

    next_broadcast_result = new_result;
    current_instruction = std::nullopt;
  } else {
    next_broadcast_result = std::nullopt;
    busy = false;
  }
}

inline int32_t ALU::execute(
    int32_t a, int32_t b,
    std::variant<riscv::R_ArithmeticOp, riscv::I_ArithmeticOp, riscv::U_Op> op)
    const {

  if (std::holds_alternative<riscv::R_ArithmeticOp>(op)) {
    switch (std::get<riscv::R_ArithmeticOp>(op)) {
    case riscv::R_ArithmeticOp::ADD:
      return a + b;
    case riscv::R_ArithmeticOp::SUB:
      return a - b;
    case riscv::R_ArithmeticOp::AND:
      return a & b;
    case riscv::R_ArithmeticOp::OR:
      return a | b;
    case riscv::R_ArithmeticOp::XOR:
      return a ^ b;
    case riscv::R_ArithmeticOp::SLL:
      return a << (b & 0x1F);
    case riscv::R_ArithmeticOp::SRL:
      return static_cast<uint32_t>(a) >> (b & 0x1F);
    case riscv::R_ArithmeticOp::SRA:
      return a >> (b & 0x1F);
    case riscv::R_ArithmeticOp::SLT:
      return (a < b) ? 1 : 0;
    case riscv::R_ArithmeticOp::SLTU:
      return (static_cast<uint32_t>(a) < static_cast<uint32_t>(b)) ? 1 : 0;
    default:
      throw std::runtime_error("Invalid R-type arithmetic operation");
    }
  } else if (std::holds_alternative<riscv::I_ArithmeticOp>(op)) {
    switch (std::get<riscv::I_ArithmeticOp>(op)) {
    case riscv::I_ArithmeticOp::ADDI:
      return a + b;
    case riscv::I_ArithmeticOp::ANDI:
      return a & b;
    case riscv::I_ArithmeticOp::ORI:
      return a | b;
    case riscv::I_ArithmeticOp::XORI:
      return a ^ b;
    case riscv::I_ArithmeticOp::SLLI:
      return a << (b & 0x1F);
    case riscv::I_ArithmeticOp::SRLI:
      return static_cast<uint32_t>(a) >> (b & 0x1F);
    case riscv::I_ArithmeticOp::SRAI:
      return a >> (b & 0x1F);
    case riscv::I_ArithmeticOp::SLTI:
      return (a < b) ? 1 : 0;
    case riscv::I_ArithmeticOp::SLTIU:
      return (static_cast<uint32_t>(a) < static_cast<uint32_t>(b)) ? 1 : 0;
    default:
      throw std::runtime_error("Invalid I-type arithmetic operation");
    }
  } else if (std::holds_alternative<riscv::U_Op>(op)) {
    switch (std::get<riscv::U_Op>(op)) {
    case riscv::U_Op::LUI:
      return b << 12;
    case riscv::U_Op::AUIPC:
      return a + (b << 12);
    default:
      throw std::runtime_error("Invalid U-type operation");
    }
  }
  throw std::runtime_error("Invalid arithmetic operation variant");
}

#endif // CORE_ALU_HPP