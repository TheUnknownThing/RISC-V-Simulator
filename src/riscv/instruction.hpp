#ifndef RISCV_INSTRUCTION_HPP
#define RISCV_INSTRUCTION_HPP

#include <cstdint>
#include <variant>

namespace riscv {
    enum class InstructionType { I, S, B, U, J, R };

    enum class I_ArithmeticOp { ADDI, ANDI, ORI, XORI, SLLI, SRLI, SRAI, SLTI, SLTIU };
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

    using DecodedInstruction = std::variant<
        R_Instruction,
        I_Instruction,
        S_Instruction,
        B_Instruction,
        U_Instruction,
        J_Instruction,
        std::monostate
    >;
}

#endif