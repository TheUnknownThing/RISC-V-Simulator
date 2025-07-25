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

enum class ALUState {
  IDLE,        // Ready to accept new instruction
  BUSY,        // Executing instruction
  RESULT_READY // Result is ready but not consumed yet
};

class ALU {
  ALUInstruction instruction;
  ALUResult result;
  uint32_t cycle_remaining;
  ALUState state;

public:
  ALU();
  bool is_available() const; // Can accept new instruction
  bool has_result() const;   // Has result ready to be consumed
  void tick();
  void set_instruction(ALUInstruction instruction);
  ALUResult get_result();        // Consumes the result
  ALUResult peek_result() const; // View result without consuming

private:
  int32_t execute(
      int32_t a, int32_t b,
      std::variant<riscv::R_ArithmeticOp, riscv::I_ArithmeticOp, riscv::U_Op>
          op) const;
};

inline ALU::ALU() : cycle_remaining(0), state(ALUState::IDLE) {}

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
      return a << (b & 0x1F); // Only use lower 5 bits for shift amount
    case riscv::R_ArithmeticOp::SRL:
      return static_cast<uint32_t>(a) >> (b & 0x1F); // Logical right shift
    case riscv::R_ArithmeticOp::SRA:
      return a >> (b & 0x1F); // Arithmetic right shift
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
      return b << 12; // LUI loads immediate into upper 20 bits
    case riscv::U_Op::AUIPC:
      return a + (b << 12); // Add upper immediate to PC
    default:
      throw std::runtime_error("Invalid U-type operation");
    }
  }
  throw std::runtime_error("Invalid arithmetic operation variant");
}

inline void ALU::set_instruction(ALUInstruction instruction) {
  if (state != ALUState::IDLE) {
    throw std::runtime_error("ALU is not available for new instruction");
  }

  this->instruction = instruction;
  cycle_remaining = 1; // You can adjust this for multi-cycle operations
  state = ALUState::BUSY;
}

inline ALUResult ALU::get_result() {
  if (state != ALUState::RESULT_READY) {
    throw std::runtime_error("ALU result is not ready");
  }

  ALUResult temp_result = result;
  state = ALUState::IDLE; // Consume the result
  return temp_result;
}

inline ALUResult ALU::peek_result() const {
  if (state != ALUState::RESULT_READY) {
    throw std::runtime_error("ALU result is not ready");
  }
  return result;
}

inline bool ALU::is_available() const { return state == ALUState::IDLE; }

inline bool ALU::has_result() const { return state == ALUState::RESULT_READY; }

inline void ALU::tick() {
  if (state == ALUState::BUSY && cycle_remaining > 0) {
    cycle_remaining--;
    if (cycle_remaining == 0) {
      // Compute result and transition to RESULT_READY state
      result.result = execute(instruction.a, instruction.b, instruction.op);
      result.dest_tag = instruction.dest_tag;
      state = ALUState::RESULT_READY;
    }
  }
  // If state is IDLE or RESULT_READY, do nothing
}

#endif // CORE_ALU_HPP