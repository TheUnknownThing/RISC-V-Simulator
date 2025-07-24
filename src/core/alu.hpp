#ifndef CORE_ALU_HPP
#define CORE_ALU_HPP

#include "../riscv/instruction.hpp"
#include <cstdint>
#include <stdexcept>
#include <stdint.h>

struct ALUInstruction {
  int32_t a;
  int32_t b;
  std::variant<riscv::R_ArithmeticOp, riscv::I_ArithmeticOp, riscv::U_Op> op;
  uint32_t dest_tag;
};

class ALU {
  ALUInstruction instruction;
  int32_t result;
  uint32_t dest_tag;
  uint32_t cycle_remaining;
public:
  ALU();
  bool is_available() const;
  void tick();
  void set_instruction(ALUInstruction instruction);
  std::pair<int32_t, uint32_t> get_result() const;

private:
  int32_t execute(int32_t a, int32_t b, std::variant<riscv::R_ArithmeticOp, riscv::I_ArithmeticOp, riscv::U_Op> op) const;
};

inline ALU::ALU() : cycle_remaining(0), dest_tag(0) {}

inline int32_t ALU::execute(int32_t a, int32_t b,
                             std::variant<riscv::R_ArithmeticOp, riscv::I_ArithmeticOp, riscv::U_Op> op) const {
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
    return a << b;
  case riscv::R_ArithmeticOp::SRL:
    return a >> b;
  case riscv::R_ArithmeticOp::SRA:
    return a >> b;
  case riscv::R_ArithmeticOp::SLT:
    return a < b;
  case riscv::R_ArithmeticOp::SLTU:
    return static_cast<uint32_t>(a) < static_cast<uint32_t>(b);
    default:
      throw std::runtime_error("Invalid arithmetic operation");
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
      return a << b;
    case riscv::I_ArithmeticOp::SRLI:
      return a >> b;
    case riscv::I_ArithmeticOp::SRAI:
      return a >> b;
    case riscv::I_ArithmeticOp::SLTI:
      return a < b;
    case riscv::I_ArithmeticOp::SLTIU:
      return static_cast<uint32_t>(a) < static_cast<uint32_t>(b);
    default:
      throw std::runtime_error("Invalid arithmetic operation");
    }
  } else if (std::holds_alternative<riscv::U_Op>(op)) {
    switch (std::get<riscv::U_Op>(op)) {
    case riscv::U_Op::LUI:
      return a << 12;
    case riscv::U_Op::AUIPC:
      return a + b;
    default:
      throw std::runtime_error("Invalid arithmetic operation");
    }
  }
  throw std::runtime_error("Invalid arithmetic operation");
}

inline void ALU::set_instruction(ALUInstruction instruction) {
  this->instruction = instruction;
  cycle_remaining = 1;
  dest_tag = instruction.dest_tag;
}

inline std::pair<int32_t, uint32_t> ALU::get_result() const {
  return std::make_pair(result, dest_tag);
}

inline bool ALU::is_available() const {
  return cycle_remaining == 0;
}

inline void ALU::tick() {
  if (cycle_remaining == 1) {
    result = execute(instruction.a, instruction.b, instruction.op);
    cycle_remaining = 0;
  }
}

#endif // CORE_ALU_HPP