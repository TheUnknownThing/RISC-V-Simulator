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
  LSB mem;
  Predictor pred;
  uint32_t pc;

public:
  CPU(std::string filename);
  int run();
private:
  riscv::DecodedInstruction fetch();
  void issue(riscv::DecodedInstruction instr);
  void execute();
  void broadcast();
  void commit();
  void Tick();
};

inline CPU::CPU(std::string filename)
    : reg_file(), rob(reg_file, alu, pred, mem, rs), rs(reg_file), loader(filename), pc(0) {
  LOG_INFO("CPU initialized with binary file: " + filename);
  LOG_DEBUG("Initial PC: 0x" + std::to_string(pc));
}

inline int CPU::run() {
  LOG_INFO("Starting CPU execution loop");
  int cycle_count = 0;
  
  while (true) {
    cycle_count++;
    LOG_DEBUG("=== Cycle " + std::to_string(cycle_count) + " ===");
    LOG_DEBUG("PC: 0x" + std::to_string(pc));
    
    Tick();
    
    if (pc == 0) { // NEED TO MODIFY THE EXIT CONDITION
      LOG_INFO("CPU execution terminated after " + std::to_string(cycle_count) + " cycles");
      return 0;
    }
  }
}

inline void CPU::Tick() {
  LOG_DEBUG("--- Fetch Stage ---");
  riscv::DecodedInstruction instr = fetch();
  
  LOG_DEBUG("--- Issue Stage ---");
  issue(instr);
  
  LOG_DEBUG("--- Execute Stage ---");
  execute();
  
  LOG_DEBUG("--- Broadcast Stage ---");
  broadcast();
  
  LOG_DEBUG("--- Commit Stage ---");
  commit();
}

inline riscv::DecodedInstruction CPU::fetch() {
  LOG_DEBUG("Fetching instruction from PC: 0x" + std::to_string(pc));
  
  uint32_t instr = loader.fetchInstruction(pc);
  LOG_DEBUG("Raw instruction: 0x" + std::to_string(instr));
  
  riscv::DecodedInstruction decoded_instr = riscv::decode(instr);
  pc += 4;
  
  LOG_DEBUG("Instruction fetched and decoded, PC updated to: 0x" + std::to_string(pc));
  return decoded_instr;
}

inline void CPU::issue(riscv::DecodedInstruction instr) {
  if (std::holds_alternative<std::monostate>(instr)) {
    LOG_ERROR("Attempting to issue invalid instruction");
    throw std::runtime_error("Invalid instruction");
  }

  LOG_DEBUG("Issuing instruction");

  std::optional<uint32_t> rd, rs1, rs2;
  std::optional<int32_t> imm;

  if (auto *r_instr = std::get_if<riscv::R_Instruction>(&instr)) {
    LOG_DEBUG("R-type instruction detected");
    rd = r_instr->rd;
    rs1 = r_instr->rs1;
    rs2 = r_instr->rs2;
  } else if (auto *i_instr = std::get_if<riscv::I_Instruction>(&instr)) {
    LOG_DEBUG("I-type instruction detected");
    rd = i_instr->rd;
    rs1 = i_instr->rs1;
    imm = i_instr->imm;
  } else if (auto *s_instr = std::get_if<riscv::S_Instruction>(&instr)) {
    LOG_DEBUG("S-type instruction detected");
    rs1 = s_instr->rs1;
    rs2 = s_instr->rs2;
    imm = s_instr->imm;
  } else if (auto *b_instr = std::get_if<riscv::B_Instruction>(&instr)) {
    LOG_DEBUG("B-type instruction detected");
    rs1 = b_instr->rs1;
    rs2 = b_instr->rs2;
    imm = b_instr->imm;
  } else if (auto *u_instr = std::get_if<riscv::U_Instruction>(&instr)) {
    LOG_DEBUG("U-type instruction detected");
    rd = u_instr->rd;
    imm = u_instr->imm;
  } else if (auto *j_instr = std::get_if<riscv::J_Instruction>(&instr)) {
    LOG_DEBUG("J-type instruction detected");
    rd = j_instr->rd;
    imm = j_instr->imm;
  }

  int id = rob.add_entry(instr, rd);

  if (id != -1) {
    LOG_DEBUG("Added entry to ROB with ID: " + std::to_string(id));
    rs.add_entry(instr, rs1, rs2, imm, id);
    LOG_DEBUG("Added entry to Reservation Station");
  } else {
    LOG_WARN("ROB is full, instruction not issued, rolling back PC");
    // ROB entry is full, do not issue
    pc -= 4;
  }
}

inline void CPU::execute() {
  LOG_DEBUG("Scanning reservation stations for ready instructions");
  int dispatched_count = 0;
  
  for (int i = 0; i < rs.rs.size(); i++) {
    ReservationStationEntry &ent = rs.rs.get(i);

    // Skip if operands not ready
    if (ent.qj != 0 || ent.qk != 0) {
      LOG_DEBUG("RS entry " + std::to_string(i) + " waiting for operands (qj=" + 
                std::to_string(ent.qj) + ", qk=" + std::to_string(ent.qk) + ")");
      continue;
    }

    bool dispatched = false;

    if (std::holds_alternative<riscv::R_Instruction>(ent.op)) {
      // R-type -> ALU
      if (alu.is_available()) {
        LOG_DEBUG("Dispatching R-type instruction to ALU (tag=" + std::to_string(ent.dest_tag) + ")");
        ALUInstruction instruction;
        instruction.a = ent.vj;
        instruction.b = ent.vk;
        instruction.op = std::get<riscv::R_Instruction>(ent.op).op;
        instruction.dest_tag = ent.dest_tag;
        alu.set_instruction(instruction);
        dispatched = true;
      } else {
        LOG_DEBUG("ALU busy, cannot dispatch R-type instruction");
      }
    } else if (auto *i_instr = std::get_if<riscv::I_Instruction>(&ent.op)) {
      // I-type -> check operation subtype
      if (std::holds_alternative<riscv::I_LoadOp>(i_instr->op)) {
        // Load -> Memory unit
        if (mem.is_available()) {
          LOG_DEBUG("Dispatching I-type load instruction to memory unit (tag=" + std::to_string(ent.dest_tag) + ")");
          LSBInstruction instruction;
          instruction.op_type = LSBOpType::LOAD;
          instruction.address = ent.vj;
          instruction.dest_tag = ent.dest_tag;
          instruction.rob_id = ent.dest_tag;
          mem.add_instruction(instruction);
          dispatched = true;
        } else {
          LOG_DEBUG("Memory unit busy, cannot dispatch load instruction");
        }
      } else if (std::holds_alternative<riscv::I_ArithmeticOp>(i_instr->op)) {
        // Arithmetic -> ALU
        if (alu.is_available()) {
          LOG_DEBUG("Dispatching I-type arithmetic instruction to ALU (tag=" + std::to_string(ent.dest_tag) + ")");
          ALUInstruction instruction;
          instruction.a = ent.vj;
          instruction.b = ent.vk;
          instruction.op = std::get<riscv::I_ArithmeticOp>(i_instr->op);
          instruction.dest_tag = ent.dest_tag;
          alu.set_instruction(instruction);
          dispatched = true;
        } else {
          LOG_DEBUG("ALU busy, cannot dispatch I-type arithmetic instruction");
        }
      } else if (std::holds_alternative<riscv::I_JumpOp>(i_instr->op)) {
        // Jump -> Predictor
        if (pred.is_available()) {
          LOG_DEBUG("Dispatching I-type jump instruction to predictor (tag=" + std::to_string(ent.dest_tag) + ")");
          PredictorInstruction instruction;
          instruction.pc = pc;
          instruction.rs1 = ent.vj;
          instruction.rs2 = ent.vk;
          instruction.dest_tag = ent.dest_tag;
          instruction.imm = ent.imm;
          instruction.branch_type = std::get<riscv::I_JumpOp>(i_instr->op);
          pred.set_instruction(instruction);
          dispatched = true;
        } else {
          LOG_DEBUG("Predictor busy, cannot dispatch jump instruction");
        }
      }
    } else if (std::holds_alternative<riscv::S_Instruction>(ent.op)) {
      // Store -> Memory unit
      if (mem.is_available()) {
        LOG_DEBUG("Dispatching S-type store instruction to memory unit (tag=" + std::to_string(ent.dest_tag) + ")");
        LSBInstruction instruction;
        instruction.op_type = LSBOpType::STORE;
        instruction.address = ent.vj;
        instruction.data = ent.vk;
        instruction.dest_tag = ent.dest_tag;
        instruction.rob_id = ent.dest_tag;
        mem.add_instruction(instruction);
        dispatched = true;
      } else {
        LOG_DEBUG("Memory unit busy, cannot dispatch store instruction");
      }
    } else if (std::holds_alternative<riscv::B_Instruction>(ent.op)) {
      // Branch -> Predictor
      if (pred.is_available()) {
        LOG_DEBUG("Dispatching B-type branch instruction to predictor (tag=" + std::to_string(ent.dest_tag) + ")");
        PredictorInstruction instruction;
        instruction.pc = pc;
        instruction.rs1 = ent.vj;
        instruction.rs2 = ent.vk;
        instruction.dest_tag = ent.dest_tag;
        instruction.imm = ent.imm;
        instruction.branch_type = std::get<riscv::B_Instruction>(ent.op).op;
        pred.set_instruction(instruction);
        dispatched = true;
      } else {
        LOG_DEBUG("Predictor busy, cannot dispatch branch instruction");
      }
    } else if (std::holds_alternative<riscv::U_Instruction>(ent.op)) {
      // U-type -> ALU
      if (alu.is_available()) {
        LOG_DEBUG("Dispatching U-type instruction to ALU (tag=" + std::to_string(ent.dest_tag) + ")");
        ALUInstruction instruction;
        instruction.a = ent.vj;
        instruction.b = ent.vk;
        instruction.op = std::get<riscv::U_Instruction>(ent.op).op;
        instruction.dest_tag = ent.dest_tag;
        alu.set_instruction(instruction);
        dispatched = true;
      } else {
        LOG_DEBUG("ALU busy, cannot dispatch U-type instruction");
      }
    } else if (std::holds_alternative<riscv::J_Instruction>(ent.op)) {
      // Jump -> Predictor
      if (pred.is_available()) {
        LOG_DEBUG("Dispatching J-type jump instruction to predictor (tag=" + std::to_string(ent.dest_tag) + ")");
        PredictorInstruction instruction;
        instruction.pc = pc;
        instruction.rs1 = ent.vj;
        instruction.rs2 = ent.vk;
        instruction.dest_tag = ent.dest_tag;
        instruction.imm = ent.imm;
        instruction.branch_type = std::get<riscv::J_Instruction>(ent.op).op;
        pred.set_instruction(instruction);
        dispatched = true;
      } else {
        LOG_DEBUG("Predictor busy, cannot dispatch J-type jump instruction");
      }
    }

    if (dispatched) {
      LOG_DEBUG("Removing dispatched instruction from reservation station");
      rs.rs.remove(i);
      i--;
      dispatched_count++;
    }
  }

  LOG_DEBUG("Dispatched " + std::to_string(dispatched_count) + " instructions in this cycle");
  LOG_DEBUG("Ticking execution units...");
  
  alu.tick();
  mem.tick();
  pred.tick();
}

inline void CPU::broadcast() {
  LOG_DEBUG("Broadcasting execution results");
  rob.receive_broadcast();
}

inline void CPU::commit() { 
  LOG_DEBUG("Committing completed instructions");
  rob.commit(); 
}

#endif // CORE_CPU_HPP