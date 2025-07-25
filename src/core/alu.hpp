#ifndef CORE_ALU_HPP
#define CORE_ALU_HPP
#include "../riscv/instruction.hpp"
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
  ALUInstruction instruction;
  ALUResult current_result;  // Result being computed this cycle
  ALUResult previous_result; // Result from previous cycle (for broadcast)
  uint32_t cycle_remaining;
  bool has_previous_result;

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

inline ALU::ALU() : cycle_remaining(0), has_previous_result(false) {}

inline bool ALU::is_available() const {
  return cycle_remaining == 0;
}

inline bool ALU::has_result_for_broadcast() const {
  return has_previous_result;
}

inline void ALU::set_instruction(ALUInstruction instruction) {
  if (!is_available()) {
    throw std::runtime_error("ALU is busy");
  }

  this->instruction = instruction;
  cycle_remaining = 1;
}

inline ALUResult ALU::get_result_for_broadcast() const {
  if (!has_previous_result) {
    throw std::runtime_error("No result available for broadcast");
  }
  return previous_result;
}

inline void ALU::tick() {
  // current becomes previous
  if (cycle_remaining == 1) {
    current_result.result =
        execute(instruction.a, instruction.b, instruction.op);
    current_result.dest_tag = instruction.dest_tag;
  }

  if (cycle_remaining == 1) {
    previous_result = current_result;
    has_previous_result = true;
  } else if (cycle_remaining == 0 && has_previous_result) {
    // Previous result has been available for broadcast for one cycle
    has_previous_result = false; // Clear it after one cycle
  }

  if (cycle_remaining > 0) {
    cycle_remaining--;
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