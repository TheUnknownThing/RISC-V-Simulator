#ifndef CORE_ALU_HPP
#define CORE_ALU_HPP

#include "../riscv/instruction.hpp"
#include <cstdint>
#include <stdexcept>
#include <stdint.h>

class ALU {
public:
  ALU();
  uint32_t execute(uint32_t a, uint32_t b, riscv::R_ArithmeticOp) const;
};

inline ALU::ALU() {}

inline uint32_t ALU::execute(uint32_t a, uint32_t b,
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
    return a < b;
  default:
    throw std::runtime_error("Invalid arithmetic operation");
  }
}

#endif // CORE_ALU_HPP