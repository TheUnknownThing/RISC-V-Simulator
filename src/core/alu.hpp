#ifndef CORE_ALU_HPP
#define CORE_ALU_HPP

#include "../riscv/instruction.hpp"
#include <cstdint>
#include <stdexcept>
#include <stdint.h>

class ALU {
public:
  ALU();
  int32_t execute(int32_t a, int32_t b, riscv::R_ArithmeticOp) const;
  bool is_available() const;
};

inline ALU::ALU() {}

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

inline bool ALU::is_available() const {
  return true;
}

#endif // CORE_ALU_HPP