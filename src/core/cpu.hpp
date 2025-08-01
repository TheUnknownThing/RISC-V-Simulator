#ifndef CORE_CPU_HPP
#define CORE_CPU_HPP

#include "../riscv/decoder.hpp"
#include "../tomasulo/reorder_buffer.hpp"
#include "../tomasulo/reservation_station.hpp"
#include "../utils/binary_loader.hpp"
#include "../utils/exceptions.hpp"
#include "../utils/logger.hpp"
#include "alu.hpp"
#include "memory.hpp"
#include "predictor.hpp"
#include "register_file.hpp"
#include "riscv/instruction.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <variant>

class CPU {
  RegisterFile reg_file;
  ReorderBuffer rob;
  ReservationStation rs;
  BinaryLoader loader;
  ALU alu;
  LSB mem;
  Predictor pred;
  uint32_t pc;

  std::optional<riscv::DecodedInstruction> fetched_instruction;
  uint32_t fetched_pc;
  bool stall_fetch;

  // Helper function for hex formatting
  std::string to_hex(uint32_t value) const {
    std::stringstream ss;
    ss << "0x" << std::hex << value;
    return ss.str();
  }

public:
  CPU(std::string filename);
  CPU();
  int run();

private:
  riscv::DecodedInstruction fetch();
  void issue(riscv::DecodedInstruction instr);
  void dispatch();
  void commit();
  void Tick();
  void TickPrioritized();
  void handle_branch_prediction(riscv::DecodedInstruction &instr,
                                uint32_t current_pc);
};

inline CPU::CPU(std::string filename)
    : reg_file(), rob(reg_file, alu, pred, mem, rs), rs(), loader(filename),
      pc(0), fetched_instruction(std::nullopt), fetched_pc(0),
      stall_fetch(false) {
  LOG_INFO("CPU initialized with binary file: " + filename);

  mem.get_memory().initialize_from_loader(loader.get_memory());

  LOG_DEBUG("Initial PC: 0x" + std::to_string(pc));
}

inline CPU::CPU()
    : reg_file(), rob(reg_file, alu, pred, mem, rs), rs(), loader(), pc(0),
      fetched_instruction(std::nullopt), fetched_pc(0), stall_fetch(false) {
  LOG_INFO("CPU initializing with binary data from stdin");

  // Load data from stdin
  loader.load_from_stdin();

  mem.get_memory().initialize_from_loader(loader.get_memory());

  LOG_DEBUG("Initial PC: 0x" + std::to_string(pc));
}

inline int CPU::run() {
  LOG_INFO("Starting CPU execution loop");
  int cycle_count = 0;

  try {
    while (true) {
      cycle_count++;
      LOG_DEBUG("======================= Cycle " + std::to_string(cycle_count) +
                " =======================");
      LOG_DEBUG("PC: " + to_hex(pc) + " (decimal: " + std::to_string(pc) + ")");

      Tick();

      if (cycle_count > 2000000000) {
        LOG_WARN("Cycle limit reached, terminating execution");
        return reg_file.read(10); // Return value from a0 register
      }
    }
  } catch (const ProgramTerminationException &e) {
    LOG_INFO("Program terminated normally: " + std::string(e.what()));
    int exit_code = e.get_exit_code();
    LOG_INFO("Final exit code: " + std::to_string(exit_code));
    return exit_code;
  }
}

inline void CPU::Tick() {
  LOG_DEBUG("======================= Beginning parallel cycle "
            "=======================");

  alu.tick();
  if (mem.has_result_for_broadcast()) {
    MemoryResult mem_result = mem.get_result_for_broadcast();
    if (mem_result.is_load()) {
      rob.receive_memory_result(mem_result);
      rs.receive_broadcast(mem_result.data, mem_result.dest_tag);
    }
  }

  pred.tick();
  if (alu.has_result_for_broadcast()) {
    ALUResult alu_result = alu.get_result_for_broadcast();
    rob.receive_alu_result(alu_result);
    rs.receive_broadcast(alu_result.result, alu_result.dest_tag);
  }

  mem.tick();
  if (pred.has_result_for_broadcast()) {
    PredictorResult pred_result = pred.get_result_for_broadcast();
    rob.receive_predictor_result(pred_result);
    if (pred_result.dest_tag.has_value()) {
      rs.receive_broadcast(pred_result.pc, pred_result.dest_tag.value());
    }
  }

  dispatch();

  LOG_DEBUG("--- Commit Stage ---");
  commit();

  if (!stall_fetch && !rob.isFull()) {
    LOG_DEBUG("--- Fetch Stage ---");
    try {
      if (!fetched_instruction.has_value()) {
        fetched_pc = pc;
        riscv::DecodedInstruction instr = fetch();
        fetched_instruction = instr;
        LOG_INFO("Fetched instruction from pc: " + to_hex(fetched_pc));
      }

      if (fetched_instruction.has_value()) {
        LOG_DEBUG("--- Issue Stage ---");
        issue(fetched_instruction.value());
        fetched_instruction = std::nullopt;
      }
    } catch (const std::exception &e) {
      LOG_WARN("Fetch/Issue stage exception: " + std::string(e.what()));
      pc = fetched_pc;
      fetched_instruction = std::nullopt;
    }
  } else {
    if (rob.isFull()) {
      LOG_DEBUG("--- Fetch Stage stalled (ROB full) ---");
    } else if (stall_fetch) {
      LOG_DEBUG("--- Fetch Stage stalled (branch misprediction recovery) ---");
    }
  }

  stall_fetch = false;
}

inline riscv::DecodedInstruction CPU::fetch() {
  LOG_DEBUG("Fetching instruction from PC: " + to_hex(pc) +
            " (decimal: " + std::to_string(pc) + ")");

  uint32_t instr = loader.fetchInstruction(pc);
  LOG_DEBUG("Raw instruction: 0x" + std::to_string(instr));

  riscv::DecodedInstruction decoded_instr = riscv::decode(instr);
  pc += 4;

  LOG_DEBUG("Instruction fetched and decoded, PC updated to: " + to_hex(pc));
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
    LOG_DEBUG("J-type immediate value: " + std::to_string(imm.value()));
  }

  int id = rob.add_entry(instr, rd, pc - 4);
  if (id != -1) {
    int32_t vj = 0, vk = 0;
    uint32_t qj = std::numeric_limits<uint32_t>::max(),
             qk = std::numeric_limits<uint32_t>::max();

    if (rs1.has_value()) {
      uint32_t rob_tag = reg_file.get_rob(rs1.value());
      if (rob_tag == std::numeric_limits<uint32_t>::max()) {
        vj = reg_file.read(rs1.value());
        LOG_DEBUG("Source operand 1 (rs1) is ready: reg" +
                  std::to_string(rs1.value()) + " = " + std::to_string(vj));
      } else {
        qj = rob_tag;
        LOG_DEBUG("Source operand 1 (rs1) is waiting for ROB tag: " +
                  std::to_string(qj));
        // check ROB
        auto rob_value = rob.get_value(rob_tag);
        if (rob_value.has_value()) {
          vj = rob_value.value();
          qj = std::numeric_limits<uint32_t>::max(); // Clear qj since we have
                                                     // the value
          LOG_DEBUG("Resolved ROB value for rs1: " + std::to_string(vj));
        }
      }
    }

    if (rs2.has_value()) {
      uint32_t rob_tag = reg_file.get_rob(rs2.value());
      if (rob_tag ==
          std::numeric_limits<uint32_t>::max()) { // Register is ready
        vk = reg_file.read(rs2.value());
        LOG_DEBUG("Source operand 2 (rs2) is ready: reg" +
                  std::to_string(rs2.value()) + " = " + std::to_string(vk));
      } else {
        qk = rob_tag;
        LOG_DEBUG("Source operand 2 (rs2) is waiting for ROB tag: " +
                  std::to_string(qk));
        auto rob_value = rob.get_value(rob_tag);
        if (rob_value.has_value()) {
          vk = rob_value.value();
          qk = std::numeric_limits<uint32_t>::max();
          LOG_DEBUG("Resolved ROB value for rs2: " + std::to_string(vk));
        }
      }
    } else {
      vk = imm.value_or(0);
      LOG_DEBUG("Source operand 2 is an immediate value: " +
                std::to_string(vk));
    }

    rs.add_entry(instr, vj, vk, qj, qk, imm, id, pc - 4);
    LOG_DEBUG("Added entry to Reservation Station");

    // check prediction instruction
    // B
    if (std::holds_alternative<riscv::B_Instruction>(instr)) {
      pc += std::get<riscv::B_Instruction>(instr).imm;
      pc -= 4; // because we already incremented PC in fetch
      LOG_DEBUG("Branch instruction, updated PC to: " + to_hex(pc));
    } else if (std::holds_alternative<riscv::J_Instruction>(instr)) {
      // JAL
      pc += std::get<riscv::J_Instruction>(instr).imm;
      pc -= 4; // because we already incremented PC in fetch
      LOG_DEBUG("JAL instruction, updated PC to: " + to_hex(pc));
    } else if (std::holds_alternative<riscv::I_Instruction>(instr) &&
               std::holds_alternative<riscv::I_JumpOp>(
                   std::get<riscv::I_Instruction>(instr).op)) {
      // JALR
      // do nothing
      LOG_DEBUG("JALR instruction, updated PC to: " + to_hex(pc));
    }

    if (rd.has_value()) {
      reg_file.receive_rob(rd.value(), id);
      LOG_DEBUG("Marked register " + std::to_string(rd.value()) +
                " as busy with ROB ID: " + std::to_string(id));
    }
  } else {
    LOG_WARN("ROB is full, instruction not issued, rolling back PC");
    pc -= 4;
  }
}

inline void CPU::dispatch() {
  LOG_DEBUG("Scanning reservation stations for ready instructions");
  int dispatched_count = 0;

  for (int i = 0; i < rs.rs.size(); i++) {
    ReservationStationEntry &ent = rs.rs.get(i);

    // Skip if operands not ready
    if (ent.qj != std::numeric_limits<uint32_t>::max() ||
        ent.qk != std::numeric_limits<uint32_t>::max()) {
      LOG_DEBUG("RS entry " + std::to_string(i) + " waiting for operands (qj=" +
                std::to_string(ent.qj) + ", qk=" + std::to_string(ent.qk) +
                ") with instruction: " + riscv::to_string(ent.op));
      // I_LoadOp, S_instruction
      if (std::holds_alternative<riscv::S_Instruction>(ent.op)) {
        LSBInstruction instruction;
        instruction.op_type = std::get<riscv::S_Instruction>(ent.op).op;
        instruction.address = ent.vj;
        instruction.data = ent.vk;
        instruction.imm = ent.imm;
        instruction.can_execute = false;
        instruction.dest_tag = ent.dest_tag;
        instruction.rob_id = ent.dest_tag;
        mem.add_instruction(instruction);
      } else if (auto *i_instr = std::get_if<riscv::I_Instruction>(&ent.op)) {
        if (std::holds_alternative<riscv::I_LoadOp>(i_instr->op)) {
          LSBInstruction instruction;
          instruction.op_type = std::get<riscv::I_LoadOp>(i_instr->op);
          instruction.address = ent.vj;
          instruction.imm = ent.imm;
          instruction.can_execute = false;
          instruction.dest_tag = ent.dest_tag;
          instruction.rob_id = ent.dest_tag;
          mem.add_instruction(instruction);
        }
      }
      continue;
    }

    bool dispatched = false;

    if (std::holds_alternative<riscv::R_Instruction>(ent.op)) {
      // R-type -> ALU
      if (alu.is_available()) {
        LOG_DEBUG("Dispatching R-type instruction to ALU (tag=" +
                  std::to_string(ent.dest_tag) + ")");
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
        LOG_DEBUG("Dispatching I-type load instruction to memory unit (tag=" +
                  std::to_string(ent.dest_tag) +
                  ", rob_id=" + std::to_string(ent.dest_tag) + ")");
        LSBInstruction instruction;
        instruction.op_type = std::get<riscv::I_LoadOp>(i_instr->op);
        instruction.address = ent.vj;
        instruction.imm = ent.imm;
        instruction.can_execute = true;
        instruction.dest_tag = ent.dest_tag;
        instruction.rob_id = ent.dest_tag;
        mem.add_instruction(instruction);
        dispatched = true;
      } else if (std::holds_alternative<riscv::I_ArithmeticOp>(i_instr->op)) {
        // Arithmetic -> ALU
        if (alu.is_available()) {
          LOG_DEBUG("Dispatching I-type arithmetic instruction to ALU (tag=" +
                    std::to_string(ent.dest_tag) + ")");
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
          LOG_DEBUG("Dispatching I-type jump instruction to predictor (tag=" +
                    std::to_string(ent.dest_tag) + ")");
          PredictorInstruction instruction;
          instruction.pc = ent.pc;
          instruction.rs1 = ent.vj;
          instruction.rs2 = 0; // JALR doesn't use rs2
          instruction.dest_tag = ent.dest_tag;
          instruction.imm = ent.imm;
          instruction.rob_id = ent.dest_tag;
          instruction.branch_type = std::get<riscv::I_JumpOp>(i_instr->op);
          LOG_DEBUG("JALR: rs1_val=" + std::to_string(instruction.rs1) +
                    ", imm=" + std::to_string(instruction.imm));
          pred.set_instruction(instruction);
          dispatched = true;
        } else {
          LOG_DEBUG("Predictor busy, cannot dispatch jump instruction");
        }
      }
    } else if (std::holds_alternative<riscv::S_Instruction>(ent.op)) {
      // Store -> Memory unit
      LOG_DEBUG("Dispatching S-type store instruction to memory unit (tag=" +
                std::to_string(ent.dest_tag) + ")");
      LSBInstruction instruction;
      instruction.op_type = std::get<riscv::S_Instruction>(ent.op).op;
      instruction.address = ent.vj;
      instruction.data = ent.vk;
      instruction.imm = ent.imm;
      instruction.can_execute = true;
      instruction.dest_tag = ent.dest_tag;
      instruction.rob_id = ent.dest_tag;
      mem.add_instruction(instruction);
      dispatched = true;
    } else if (std::holds_alternative<riscv::B_Instruction>(ent.op)) {
      // Branch -> Predictor
      if (pred.is_available()) {
        LOG_DEBUG("Dispatching B-type branch instruction to predictor (tag=" +
                  std::to_string(ent.dest_tag) + ")");
        PredictorInstruction instruction;
        instruction.pc = ent.pc;
        instruction.rs1 = ent.vj;
        instruction.rs2 = ent.vk;
        instruction.dest_tag = std::nullopt;
        instruction.imm = ent.imm;
        instruction.rob_id = ent.dest_tag;
        instruction.branch_type = std::get<riscv::B_Instruction>(ent.op).op;
        pred.set_instruction(instruction);
        dispatched = true;
      } else {
        LOG_DEBUG("Predictor busy, cannot dispatch branch instruction");
      }
    } else if (std::holds_alternative<riscv::U_Instruction>(ent.op)) {
      // U-type -> ALU
      if (alu.is_available()) {
        LOG_DEBUG("Dispatching U-type instruction to ALU (tag=" +
                  std::to_string(ent.dest_tag) + ")");
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
        LOG_DEBUG("Dispatching J-type jump instruction to predictor (tag=" +
                  std::to_string(ent.dest_tag) + ")");
        PredictorInstruction instruction;
        instruction.pc = ent.pc;
        instruction.rs1 = 0;
        instruction.rs2 = 0;
        instruction.dest_tag = ent.dest_tag;
        instruction.rob_id = ent.dest_tag;
        instruction.imm = ent.imm;
        instruction.branch_type = std::get<riscv::J_Instruction>(ent.op).op;
        LOG_DEBUG("JAL: pc=" + std::to_string(instruction.pc) +
                  ", imm=" + std::to_string(instruction.imm));
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

  LOG_DEBUG("Dispatched " + std::to_string(dispatched_count) +
            " instructions in this cycle");
}

inline void CPU::commit() {
  LOG_DEBUG("Committing completed instructions");
  // reg_file.print_debug_info();
  // rs.print_debug_info();

  bool mispredicted = rob.commit(pc);
  if (mispredicted) {
    LOG_DEBUG("Branch misprediction detected, stalling fetch for next cycle");
    stall_fetch = true;
    fetched_instruction = std::nullopt;
  }

  // rob.print_debug_info();
}

#endif // CORE_CPU_HPP