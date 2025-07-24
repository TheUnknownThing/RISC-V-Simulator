#ifndef CORE_CPU_HPP
#define CORE_CPU_HPP

#include "../riscv/decoder.hpp"
#include "../tomasulo/reorder_buffer.hpp"
#include "../tomasulo/reservation_station.hpp"
#include "../utils/binary_loader.hpp"
#include "../utils/logger.hpp"
#include "alu.hpp"
#include "memory.hpp"
#include "predictor.hpp"
#include "register_file.hpp"

class CPU {
  RegisterFile reg_file;
  ReorderBuffer rob;
  ReservationStation rs;
  BinaryLoader loader;
  ALU alu;
  Memory mem;
  Predictor pred;
  uint32_t pc;

public:
  CPU(std::string filename);
  void Tick();

private:
  riscv::DecodedInstruction fetch();
  void issue(riscv::DecodedInstruction instr);
  void execute();
  void broadcast();
  void commit();
};

inline CPU::CPU(std::string filename)
    : reg_file(), rob(reg_file), rs(reg_file), loader(filename), pc(0) {}

inline void CPU::Tick() {
  riscv::DecodedInstruction instr = fetch();
  issue(instr);
  execute();
  broadcast();
  commit();
}

inline riscv::DecodedInstruction CPU::fetch() {
  uint32_t instr = loader.fetchInstruction(pc);
  riscv::DecodedInstruction decoded_instr = riscv::decode(instr);
  pc += 4;
  return decoded_instr;
}

inline void CPU::issue(riscv::DecodedInstruction instr) {
  if (std::holds_alternative<std::monostate>(instr)) {
    throw std::runtime_error("Invalid instruction");
  }

  std::optional<uint32_t> rd, rs1, rs2;
  std::optional<int32_t> imm;

  if (auto *r_instr = std::get_if<riscv::R_Instruction>(&instr)) {
    rd = r_instr->rd;
    rs1 = r_instr->rs1;
    rs2 = r_instr->rs2;
  } else if (auto *i_instr = std::get_if<riscv::I_Instruction>(&instr)) {
    rd = i_instr->rd;
    rs1 = i_instr->rs1;
    imm = i_instr->imm;
  } else if (auto *s_instr = std::get_if<riscv::S_Instruction>(&instr)) {
    rs1 = s_instr->rs1;
    rs2 = s_instr->rs2;
    imm = s_instr->imm;
  } else if (auto *b_instr = std::get_if<riscv::B_Instruction>(&instr)) {
    rs1 = b_instr->rs1;
    rs2 = b_instr->rs2;
    imm = b_instr->imm;
  } else if (auto *u_instr = std::get_if<riscv::U_Instruction>(&instr)) {
    rd = u_instr->rd;
    imm = u_instr->imm;
  } else if (auto *j_instr = std::get_if<riscv::J_Instruction>(&instr)) {
    rd = j_instr->rd;
    imm = j_instr->imm;
  }

  int id = rob.add_entry(instr, rd);

  if (id != -1) {
    rs.add_entry(instr, rs1, rs2, imm, id);
  } else {
    // ROB entry is full, do not issue
    pc -= 4;
  }
}

inline void CPU::execute() {
  for (int i = 0; i < rs.rs.size(); i++) {
    ReservationStationEntry &ent = rs.rs.get(i);

    // Skip if operands not ready
    if (ent.qj != 0 || ent.qk != 0) {
      continue;
    }

    bool dispatched = false;

    if (std::holds_alternative<riscv::R_Instruction>(ent.op)) {
      // R-type -> ALU
      if (alu.is_available()) {
        ALUInstruction instruction;
        instruction.a = ent.vj;
        instruction.b = ent.vk;
        instruction.op = std::get<riscv::R_Instruction>(ent.op).op;
        alu.set_instruction(instruction);
        dispatched = true;
      }
    } else if (auto *i_instr = std::get_if<riscv::I_Instruction>(&ent.op)) {
      // I-type -> check operation subtype
      if (std::holds_alternative<riscv::I_LoadOp>(i_instr->op)) {
        // Load -> Memory unit
        if (mem.is_available()) {
          mem.execute(ent);
          dispatched = true;
        }
      } else if (std::holds_alternative<riscv::I_ArithmeticOp>(i_instr->op)) {
        // Arithmetic -> ALU
        if (alu.is_available()) {
          ALUInstruction instruction;
          instruction.a = ent.vj;
          instruction.b = ent.vk;
          instruction.op = std::get<riscv::I_ArithmeticOp>(i_instr->op);
          alu.set_instruction(instruction);
          dispatched = true;
        }
      } else if (std::holds_alternative<riscv::I_JumpOp>(i_instr->op)) {
        // Jump -> Predictor
        if (pred.is_available()) {
          pred.execute(ent);
          dispatched = true;
        }
      }
    } else if (std::holds_alternative<riscv::S_Instruction>(ent.op)) {
      // Store -> Memory unit
      if (mem.is_available()) {
        mem.execute(ent);
        dispatched = true;
      }
    } else if (std::holds_alternative<riscv::B_Instruction>(ent.op)) {
      // Branch -> Predictor
      if (pred.is_available()) {
        pred.execute(ent);
        dispatched = true;
      }
    } else if (std::holds_alternative<riscv::U_Instruction>(ent.op)) {
      // U-type -> ALU
      if (alu.is_available()) {
        ALUInstruction instruction;
        instruction.a = ent.vj;
        instruction.b = ent.vk;
        instruction.op = std::get<riscv::U_Instruction>(ent.op).op;
        alu.set_instruction(instruction);
        dispatched = true;
      }
    } else if (std::holds_alternative<riscv::J_Instruction>(ent.op)) {
      // Jump -> Predictor
      if (pred.is_available()) {
        pred.execute(ent);
        dispatched = true;
      }
    }

    if (dispatched) {
      rs.rs.remove(i);
      i--;
    }
  }
}

inline void CPU::broadcast() { rob.receive_broadcast(); }

inline void CPU::commit() { rob.commit(); }

#endif // CORE_CPU_HPP