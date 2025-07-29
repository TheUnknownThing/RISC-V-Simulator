#ifndef RISCV_INSTRUCTION_HPP
#define RISCV_INSTRUCTION_HPP

#include <cstdint>
#include <string>
#include <variant>

namespace riscv {
enum class InstructionType { I, S, B, U, J, R };

enum class I_ArithmeticOp {
  ADDI,
  ANDI,
  ORI,
  XORI,
  SLLI,
  SRLI,
  SRAI,
  SLTI,
  SLTIU
};
enum class I_LoadOp { LB, LH, LW, LBU, LHU };
enum class I_JumpOp { JALR };
enum class S_StoreOp { SB, SH, SW };
enum class B_BranchOp { BEQ, BNE, BLT, BGE, BLTU, BGEU };
enum class U_Op { LUI, AUIPC };
enum class J_Op { JAL };
enum class R_ArithmeticOp { ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU };

struct R_Instruction {
  R_ArithmeticOp op;
  uint32_t rd, rs1, rs2;
};

struct I_Instruction {
  std::variant<I_ArithmeticOp, I_LoadOp, I_JumpOp> op;
  uint32_t rd, rs1;
  int32_t imm;
};

struct S_Instruction {
  S_StoreOp op;
  uint32_t rs1, rs2;
  int32_t imm;
};

struct B_Instruction {
  B_BranchOp op;
  uint32_t rs1, rs2;
  int32_t imm;
};

struct U_Instruction {
  U_Op op;
  uint32_t rd;
  int32_t imm;
};

struct J_Instruction {
  J_Op op;
  uint32_t rd;
  int32_t imm;
};

using DecodedInstruction =
    std::variant<R_Instruction, I_Instruction, S_Instruction, B_Instruction,
                 U_Instruction, J_Instruction, std::monostate>;

inline std::string to_string(const DecodedInstruction &instr) {
  if (std::holds_alternative<R_Instruction>(instr)) {
    const auto &r_instr = std::get<R_Instruction>(instr);
    return "R_Instruction{op=" + std::to_string(static_cast<int>(r_instr.op)) +
           ", rd=" + std::to_string(r_instr.rd) +
           ", rs1=" + std::to_string(r_instr.rs1) +
           ", rs2=" + std::to_string(r_instr.rs2) + "}";
  } else if (std::holds_alternative<I_Instruction>(instr)) {
    const auto &i_instr = std::get<I_Instruction>(instr);
    return "I_Instruction{op=" +
           std::visit(
               [](auto &&op) { return std::to_string(static_cast<int>(op)); },
               i_instr.op) +
           ", rd=" + std::to_string(i_instr.rd) +
           ", rs1=" + std::to_string(i_instr.rs1) +
           ", imm=" + std::to_string(i_instr.imm) + "}";
  } else if (std::holds_alternative<S_Instruction>(instr)) {
    const auto &s_instr = std::get<S_Instruction>(instr);
    return "S_Instruction{op=" + std::to_string(static_cast<int>(s_instr.op)) +
           ", rs1=" + std::to_string(s_instr.rs1) +
           ", rs2=" + std::to_string(s_instr.rs2) +
           ", imm=" + std::to_string(s_instr.imm) + "}";
  } else if (std::holds_alternative<B_Instruction>(instr)) {
    const auto &b_instr = std::get<B_Instruction>(instr);
    return "B_Instruction{op=" + std::to_string(static_cast<int>(b_instr.op)) +
           ", rs1=" + std::to_string(b_instr.rs1) +
           ", rs2=" + std::to_string(b_instr.rs2) +
           ", imm=" + std::to_string(b_instr.imm) + "}";
  } else if (std::holds_alternative<U_Instruction>(instr)) {
    const auto &u_instr = std::get<U_Instruction>(instr);
    return "U_Instruction{op=" + std::to_string(static_cast<int>(u_instr.op)) +
           ", rd=" + std::to_string(u_instr.rd) +
           ", imm=" + std::to_string(u_instr.imm) + "}";
  } else if (std::holds_alternative<J_Instruction>(instr)) {
    const auto &j_instr = std::get<J_Instruction>(instr);
    return "J_Instruction{op=" + std::to_string(static_cast<int>(j_instr.op)) +
           ", rd=" + std::to_string(j_instr.rd) +
           ", imm=" + std::to_string(j_instr.imm) + "}";
  } else {
    return "Invalid DecodedInstruction";
  }
}
} // namespace riscv

#endif