#ifndef TOMASULO_REORDER_BUFFER_HPP
#define TOMASULO_REORDER_BUFFER_HPP

#include "../core/alu.hpp"
#include "../core/memory.hpp"
#include "../core/predictor.hpp"
#include "../core/register_file.hpp"
#include "../riscv/instruction.hpp"
#include "../utils/exceptions.hpp"
#include "../utils/logger.hpp"
#include "../utils/queue.hpp"
#include "../utils/dump.hpp"
#include "reservation_station.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <variant>

struct ReorderBufferEntry {
  ReorderBufferEntry() = default;
  ReorderBufferEntry(riscv::DecodedInstruction instr,
                     std::optional<uint32_t> dest_tag, uint32_t id)
      : instr(instr), dest_tag(dest_tag), value(-1), ready(false),
        exception_flag(false), id(id), pc(0) {
    if ((!dest_tag.has_value()) &&
        !std::holds_alternative<riscv::B_Instruction>(instr)) {
      ready = true;
    } else {
      ready = false;
    }
  }
  riscv::DecodedInstruction instr;
  std::optional<uint32_t> dest_tag;
  int32_t value;
  bool ready;
  bool exception_flag;
  uint32_t id;
  uint32_t pc;
  uint32_t instruction_pc; // PC where this instruction was fetched from, for debug
};

class ReorderBuffer {
  CircularQueue<ReorderBufferEntry> rob;
  uint32_t cur_id = 0;
  RegisterFile &reg_file;
  ALU &alu;
  Predictor &predictor;
  LSB &mem;
  ReservationStation &rs;
  norb::RegisterDumper<32> reg_dumper;

public:
  ReorderBuffer(RegisterFile &reg_file, ALU &alu, Predictor &predictor,
                LSB &mem, ReservationStation &rs);

  int add_entry(riscv::DecodedInstruction instr,
                std::optional<uint32_t> dest_tag, uint32_t instr_pc);
  void commit(uint32_t &pc);
  void receive_broadcast();
  void flush();
  std::optional<int32_t> get_value(std::optional<uint32_t> rob_id);
  void print_debug_info();
};

inline ReorderBuffer::ReorderBuffer(RegisterFile &reg_file, ALU &alu,
                                    Predictor &predictor, LSB &mem,
                                    ReservationStation &rs)
    : rob(32), reg_file(reg_file), alu(alu), predictor(predictor), mem(mem),
      rs(rs)
      , reg_dumper("register_dump.txt") 
      {
  LOG_DEBUG("ReorderBuffer initialized with capacity: 32");
}

inline int ReorderBuffer::add_entry(riscv::DecodedInstruction instr,
                                    std::optional<uint32_t> dest_tag, uint32_t instr_pc) {
  if (!rob.isFull()) {
    ReorderBufferEntry ent(instr, dest_tag, cur_id++);
    ent.instruction_pc = instr_pc;
    rob.enqueue(ent);
    LOG_DEBUG("Added entry to ROB with ID: " + std::to_string(ent.id) +
              (dest_tag.has_value()
                   ? ", dest_reg: " + std::to_string(dest_tag.value())
                   : ", no dest_reg"));
    return ent.id;
  }
  LOG_WARN("ROB is full, cannot add new entry");
  return -1;
}

inline void ReorderBuffer::commit(uint32_t &pc) {
  if (rob.isEmpty()) {
    LOG_DEBUG("ROB is empty, nothing to commit");
    return;
  }

  const auto &ent = rob.front();

  mem.commit_memory(ent.id);

  if (ent.ready) {
    LOG_DEBUG("Committing instruction with ROB ID: " + std::to_string(ent.id));

    // termination instruction: li a0, 255
    if (auto *i_instr = std::get_if<riscv::I_Instruction>(&ent.instr)) {
      if (std::holds_alternative<riscv::I_ArithmeticOp>(i_instr->op) &&
          std::get<riscv::I_ArithmeticOp>(i_instr->op) ==
              riscv::I_ArithmeticOp::ADDI &&
          i_instr->rd == 10 && i_instr->rs1 == 0 && i_instr->imm == 255) {
        LOG_INFO("Termination instruction detected: li a0, 255");

        // Get the original value of a0 before termination
        double original_a0_value = reg_file.read(10);
        LOG_INFO("Program terminating with exit code: " +
                 std::to_string(static_cast<int>(original_a0_value)));

        // Do not write to register a0, just mark it as available if needed
        if (ent.dest_tag.has_value() &&
            reg_file.get_rob(ent.dest_tag.value()) == ent.id) {
          reg_file.mark_available(ent.dest_tag.value());
          LOG_DEBUG("Marked register " + std::to_string(ent.dest_tag.value()) +
                    " as available without overwriting its value");
        }

        throw ProgramTerminationException(static_cast<int>(original_a0_value));
      }
    }

    if (ent.exception_flag) {
      LOG_WARN("Branch misprediction detected! Flushing pipeline and "
               "correcting PC");

      // Flush
      flush();
      rs.flush();
      mem.flush();
      predictor.flush();
      pc = ent.pc;
    } else {
      LOG_DEBUG("No predictor broadcast available");
    }

    if (ent.dest_tag.has_value()) {
      LOG_DEBUG("Writing value " + std::to_string(ent.value) + " to register " +
                std::to_string(ent.dest_tag.value()));
      reg_file.write(ent.dest_tag.value(), ent.value);
      if (reg_file.get_rob(ent.dest_tag.value()) == ent.id) {
        reg_file.mark_available(ent.dest_tag.value());
        LOG_DEBUG("Marked register " + std::to_string(ent.dest_tag.value()) +
                  " as available");
      }
    }
    
    // Dump register file contents after commit
    std::array<uint32_t, 32> reg_snapshot;
    for (size_t i = 0; i < 32; ++i) {
      reg_snapshot[i] = static_cast<uint32_t>(reg_file.read(i));
    }
    reg_dumper.dump(ent.instruction_pc, reg_snapshot);
    
    rob.dequeue();
    LOG_DEBUG("Instruction committed and removed from ROB");
  } else {
    LOG_DEBUG("Head instruction not ready for commit (ROB ID: " +
              std::to_string(ent.id) +
              "), instruction details: " + riscv::to_string(ent.instr));
  }
}

inline void ReorderBuffer::receive_broadcast() {
  LOG_DEBUG("Checking for broadcast results from execution units");
  int broadcasts_received = 0;

  // check ALU, Predictor, and LSB
  if (alu.has_result_for_broadcast()) {
    ALUResult result = alu.get_result_for_broadcast();
    LOG_DEBUG(
        "Received ALU broadcast for tag: " + std::to_string(result.dest_tag) +
        ", result: " + std::to_string(result.result));
    for (int i = 0; i < rob.size(); i++) {
      ReorderBufferEntry &ent = rob.get(i);
      if (ent.id == result.dest_tag) {
        ent.value = result.result;
        ent.ready = true;
        rs.receive_broadcast(result.result, result.dest_tag);
        LOG_DEBUG("Updated ROB entry ID: " + std::to_string(ent.id) +
                  " with ALU result");
        broadcasts_received++;
      }
    }
  }
  if (predictor.has_result_for_broadcast()) {
    PredictorResult result = predictor.get_result_for_broadcast();
    LOG_DEBUG("Received Predictor broadcast, PC: " + std::to_string(result.pc));
    for (int i = 0; i < rob.size(); i++) {
      ReorderBufferEntry &ent = rob.get(i);
      if (result.dest_tag.has_value() && ent.id == result.dest_tag.value()) {
        ent.value = result.pc;
        ent.pc = result.correct_target;
        ent.ready = true;
        rs.receive_broadcast(result.pc, result.dest_tag.value());
        ent.exception_flag = result.is_mispredicted;
        // ent.value = result.correct_target;
        LOG_DEBUG("Updated ROB entry ID: " + std::to_string(ent.id) +
                  " with Predictor result (return addr: " +
                  std::to_string(result.pc) + ")");
        broadcasts_received++;
      } else if (ent.id == result.rob_id) {
        // B type
        ent.ready = true;
        ent.exception_flag = result.is_mispredicted;
        ent.pc = result.correct_target;
        LOG_DEBUG("Updated ROB entry ID: " + std::to_string(ent.id) +
                  " as ready based on Predictor result");
        broadcasts_received++;
      }
    }
  }
  if (mem.has_result_for_broadcast()) {
    MemoryResult result = mem.get_result_for_broadcast();
    // Only LOAD operations update the ROB. A STORE completion broadcast is
    // informational and arrives after the instruction has already been
    // committed and removed from the ROB.
    if (result.op_type == LSBOpType::LOAD) {
      LOG_DEBUG("Received Memory broadcast for tag: " +
                std::to_string(result.dest_tag) +
                ", data: " + std::to_string(result.data));
      for (int i = 0; i < rob.size(); i++) {
        ReorderBufferEntry &ent = rob.get(i);
        if (ent.id == result.dest_tag) {
          ent.value = result.data;
          ent.ready = true;
          rs.receive_broadcast(result.data, result.dest_tag);
          LOG_DEBUG("Updated ROB entry ID: " + std::to_string(ent.id) +
                    " with Memory result");
          broadcasts_received++;
        }
      }
    }
  }

  if (broadcasts_received > 0) {
    LOG_DEBUG("Processed " + std::to_string(broadcasts_received) +
              " broadcasts this cycle");
  }
}

inline void ReorderBuffer::flush() {
  LOG_DEBUG("Flushing ROB - clearing all entries");

  while (!rob.isEmpty()) {
    const auto &ent = rob.front();
    if (ent.dest_tag.has_value()) {
      if (reg_file.get_rob(ent.dest_tag.value()) == ent.id) {
        reg_file.mark_available(ent.dest_tag.value());
        LOG_DEBUG("Cleared register dependency for reg" +
                  std::to_string(ent.dest_tag.value()));
      }
    }
    rob.dequeue();
  }

  // Reset ID counter
  // cur_id = 0;
  LOG_DEBUG("ROB flush completed");
}

inline std::optional<int32_t>
ReorderBuffer::get_value(std::optional<uint32_t> rob_id) {
  if (!rob_id.has_value()) {
    LOG_DEBUG("No rob_id provided, returning nullopt");
    return std::nullopt;
  }
  uint32_t id = rob_id.value();
  for (int i = 0; i < rob.size(); i++) {
    const auto &ent = rob.get(i);
    if (ent.id == id) {
      if (ent.ready) {
        return static_cast<int32_t>(ent.value);
      } else {
        LOG_DEBUG("ROB entry ID: " + std::to_string(id) +
                  " is not ready, cannot retrieve value");
        return std::nullopt;
      }
    }
  }
  LOG_DEBUG("ROB entry ID: " + std::to_string(id) +
            " not found, returning nullopt");
  return std::nullopt;
}

void ReorderBuffer::print_debug_info() {
  LOG_DEBUG("Reorder Buffer Debug Info:");
  for (int i = 0; i < rob.size(); i++) {
    const auto &ent = rob.get(i);
    LOG_DEBUG("  Entry " + std::to_string(i) + ": " + "ID: " +
              std::to_string(ent.id) + ", Value: " + std::to_string(ent.value) +
              ", Ready: " + (ent.ready ? "true" : "false"));
  }
}

#endif // TOMASULO_REORDER_BUFFER_HPP