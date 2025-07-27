#ifndef RISCV_DECODER_HPP
#define RISCV_DECODER_HPP

#include "instruction.hpp"
#include <stdexcept>

namespace riscv {

inline int32_t sign_extend(uint32_t value, int bits) {
  uint32_t sign_bit = 1U << (bits - 1);
  return (value & sign_bit) ? (value | ~((1U << bits) - 1)) : value;
}

inline DecodedInstruction decode(uint32_t instruction) {
  uint32_t opcode = instruction & 0x7F;
  uint32_t rd = (instruction >> 7) & 0x1F;
  uint32_t funct3 = (instruction >> 12) & 0x7;
  uint32_t rs1 = (instruction >> 15) & 0x1F;
  uint32_t rs2 = (instruction >> 20) & 0x1F;
  uint32_t funct7 = (instruction >> 25) & 0x7F;

  switch (opcode) {
  // LUI, AUIPC (U-Type)
  case 0b0110111:
    return U_Instruction{U_Op::LUI, rd, (int32_t)(instruction & 0xFFFFF000)};
  case 0b0010111:
    return U_Instruction{U_Op::AUIPC, rd, (int32_t)(instruction & 0xFFFFF000)};

  // JAL (J-Type)
  case 0b1101111: {
    uint32_t imm_val = ((instruction >> 12) & 0xFF) << 12 | // imm[19:12]
                       ((instruction >> 20) & 0x1) << 11 |  // imm[11]
                       ((instruction >> 21) & 0x3FF) << 1 | // imm[10:1]
                       ((instruction >> 31) & 0x1) << 20;   // imm[20]
    return J_Instruction{J_Op::JAL, rd, sign_extend(imm_val, 21)};
  }

  // JALR (I-Type)
  case 0b1100111: {
    int32_t imm = sign_extend((instruction >> 20), 12);
    return I_Instruction{I_JumpOp::JALR, rd, rs1, imm};
  }

  // Branch (B-Type)
  case 0b1100011: {
    uint32_t imm_val = ((instruction >> 7) & 0x1) << 11 |  // imm[11]
                       ((instruction >> 8) & 0xF) << 1 |   // imm[4:1]
                       ((instruction >> 25) & 0x3F) << 5 | // imm[10:5]
                       ((instruction >> 31) & 0x1) << 12;  // imm[12]
    int32_t imm = sign_extend(imm_val, 13);

    B_BranchOp op;
    switch (funct3) {
    case 0b000:
      op = B_BranchOp::BEQ;
      break;
    case 0b001:
      op = B_BranchOp::BNE;
      break;
    case 0b100:
      op = B_BranchOp::BLT;
      break;
    case 0b101:
      op = B_BranchOp::BGE;
      break;
    case 0b110:
      op = B_BranchOp::BLTU;
      break;
    case 0b111:
      op = B_BranchOp::BGEU;
      break;
    default:
      return std::monostate{};
    }
    return B_Instruction{op, rs1, rs2, imm};
  }

  // Loads (I-Type)
  case 0b0000011: {
    int32_t imm = sign_extend((instruction >> 20), 12);
    I_LoadOp op;
    switch (funct3) {
    case 0b000:
      op = I_LoadOp::LB;
      break;
    case 0b001:
      op = I_LoadOp::LH;
      break;
    case 0b010:
      op = I_LoadOp::LW;
      break;
    case 0b100:
      op = I_LoadOp::LBU;
      break;
    case 0b101:
      op = I_LoadOp::LHU;
      break;
    default:
      return std::monostate{};
    }
    return I_Instruction{op, rd, rs1, imm};
  }

  // Stores (S-Type)
  case 0b0100011: {
    uint32_t imm_val = ((instruction >> 7) & 0x1F) | ((instruction >> 25) << 5);
    int32_t imm = sign_extend(imm_val, 12);
    S_StoreOp op;
    switch (funct3) {
    case 0b000:
      op = S_StoreOp::SB;
      break;
    case 0b001:
      op = S_StoreOp::SH;
      break;
    case 0b010:
      op = S_StoreOp::SW;
      break;
    default:
      return std::monostate{};
    }
    return S_Instruction{op, rs1, rs2, imm};
  }

  // Immediate Arithmetic (I-Type)
  case 0b0010011: {
    int32_t imm = sign_extend((instruction >> 20), 12);
    I_ArithmeticOp op;
    switch (funct3) {
    case 0b000:
      op = I_ArithmeticOp::ADDI;
      break;
    case 0b010:
      op = I_ArithmeticOp::SLTI;
      break;
    case 0b011:
      op = I_ArithmeticOp::SLTIU;
      break;
    case 0b100:
      op = I_ArithmeticOp::XORI;
      break;
    case 0b110:
      op = I_ArithmeticOp::ORI;
      break;
    case 0b111:
      op = I_ArithmeticOp::ANDI;
      break;
    case 0b001:
      op = I_ArithmeticOp::SLLI;
      imm &= 0x1F;
      break; // Special case for shamt
    case 0b101:
      if ((imm >> 5) == 0b0100000) {
        op = I_ArithmeticOp::SRAI;
      } // SRAI has funct7
      else {
        op = I_ArithmeticOp::SRLI;
      }
      imm &= 0x1F; // Special case for shamt
      break;
    default:
      return std::monostate{};
    }
    return I_Instruction{op, rd, rs1, imm};
  }

  // Register Arithmetic (R-Type)
  case 0b0110011: {
    R_ArithmeticOp op;
    switch (funct3) {
    case 0b000:
      op = (funct7 == 0b0100000) ? R_ArithmeticOp::SUB : R_ArithmeticOp::ADD;
      break;
    case 0b001:
      op = R_ArithmeticOp::SLL;
      break;
    case 0b010:
      op = R_ArithmeticOp::SLT;
      break;
    case 0b011:
      op = R_ArithmeticOp::SLTU;
      break;
    case 0b100:
      op = R_ArithmeticOp::XOR;
      break;
    case 0b101:
      op = (funct7 == 0b0100000) ? R_ArithmeticOp::SRA : R_ArithmeticOp::SRL;
      break;
    case 0b110:
      op = R_ArithmeticOp::OR;
      break;
    case 0b111:
      op = R_ArithmeticOp::AND;
      break;
    default:
      return std::monostate{};
    }
    return R_Instruction{op, rd, rs1, rs2};
  }
  }
  return std::monostate{};
}

} // namespace riscv

#endif