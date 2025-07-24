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
};

class ALU {
  ALUInstruction instruction;
  int32_t result;
  uint32_t cycle_remaining;
public:
  ALU();
  bool is_available() const;
  void tick();
  void set_instruction(ALUInstruction instruction);
  int32_t get_result() const;

private:
  int32_t execute(int32_t a, int32_t b, riscv::R_ArithmeticOp) const;
};

inline ALU::ALU() : cycle_remaining(0) {}

inline int32_t ALU::execute(int32_t a, int32_t b,
                             riscv::R_ArithmeticOp op) const {
  switch (op) {
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
}

inline void ALU::set_instruction(ALUInstruction instruction) {
  this->instruction = instruction;
  cycle_remaining = 1;
}

inline int32_t ALU::get_result() const {
  return result;
}

inline bool ALU::is_available() const {
  return cycle_remaining == 0;
}

inline void ALU::tick() {
  if (cycle_remaining > 0) {
    cycle_remaining--;
  } else {
    result = execute(instruction.a, instruction.b, instruction.op);
    cycle_remaining = 0;
  }
}

#endif // CORE_ALU_HPP