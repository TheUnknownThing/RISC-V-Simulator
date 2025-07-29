#ifndef TOMASULO_REORDER_BUFFER_HPP
#define TOMASULO_REORDER_BUFFER_HPP

#include "../core/register_file.hpp"
#include "../riscv/instruction.hpp"
#include "reservation_station.hpp"
#include "../utils/queue.hpp"
#include "../utils/logger.hpp"
#include "../utils/exceptions.hpp"
#include "core/alu.hpp"
#include "core/memory.hpp"
#include "core/predictor.hpp"
#include <array>
#include <cstdint>
#include <optional>


struct ReorderBufferEntry {
  ReorderBufferEntry() = default;
  ReorderBufferEntry(riscv::DecodedInstruction instr, std::optional<uint32_t> dest_tag, uint32_t id)
      : instr(instr), dest_tag(dest_tag),
        value(-1), ready(false), exception_flag(false), id(id) {}
  riscv::DecodedInstruction instr;
  std::optional<uint32_t> dest_tag;
  double value;
  bool ready;
  bool exception_flag;
  uint32_t id;
};

class ReorderBuffer {
  CircularQueue<ReorderBufferEntry> rob;
  uint32_t cur_id = 0;
  RegisterFile &reg_file;
  ALU &alu;
  Predictor &predictor;
  LSB &mem;
  ReservationStation &rs;

public:
  ReorderBuffer(RegisterFile &reg_file, ALU &alu, Predictor &predictor, LSB &mem, ReservationStation &rs);

  /**
   * @brief Add an entry to the reorder buffer
   * @param instr The instruction to add
   * @param dest_tag The destination register
   * @return The ID of the entry
   */
  int add_entry(riscv::DecodedInstruction instr, std::optional<uint32_t> dest_tag);
  void commit();
  void receive_broadcast();
  void flush();
};

inline ReorderBuffer::ReorderBuffer(RegisterFile &reg_file, ALU &alu, Predictor &predictor, LSB &mem, ReservationStation &rs)
    : rob(32), reg_file(reg_file), alu(alu), predictor(predictor), mem(mem), rs(rs) {
  LOG_DEBUG("ReorderBuffer initialized with capacity: 32");
}

inline int ReorderBuffer::add_entry(riscv::DecodedInstruction instr,
                                    std::optional<uint32_t> dest_tag) {
  if (!rob.isFull()) {
    ReorderBufferEntry ent(instr, dest_tag, cur_id++);
    rob.enqueue(ent);
    LOG_DEBUG("Added entry to ROB with ID: " + std::to_string(ent.id) + 
              (dest_tag.has_value() ? ", dest_reg: " + std::to_string(dest_tag.value()) : ", no dest_reg"));
    return ent.id;
  }
  LOG_WARN("ROB is full, cannot add new entry");
  return -1;
}

inline void ReorderBuffer::commit() {
  if (rob.isEmpty()) {
    LOG_DEBUG("ROB is empty, nothing to commit");
    return;
  }
  
  const auto &ent = rob.front();
  if (ent.ready) {
    LOG_DEBUG("Committing instruction with ROB ID: " + std::to_string(ent.id));
    
    // termination instruction: li a0, 255
    if (auto *i_instr = std::get_if<riscv::I_Instruction>(&ent.instr)) {
      if (std::holds_alternative<riscv::I_ArithmeticOp>(i_instr->op) &&
          std::get<riscv::I_ArithmeticOp>(i_instr->op) == riscv::I_ArithmeticOp::ADDI &&
          i_instr->rd == 10 &&
          i_instr->rs1 == 0 &&
          i_instr->imm == 255) {
        LOG_INFO("Termination instruction detected: li a0, 255");
        LOG_INFO("Program terminating with exit code: " + std::to_string(static_cast<int>(ent.value)));
        
        // Commit the instruction first before terminating
        if (ent.dest_tag.has_value()) {
          LOG_DEBUG("Writing termination value " + std::to_string(ent.value) + " to register " + std::to_string(ent.dest_tag.value()));
          reg_file.write(ent.dest_tag.value(), ent.value);
          if (reg_file.get_rob(ent.dest_tag.value()) == ent.id) {
            reg_file.mark_available(ent.dest_tag.value());
          }
        }
        
        // Throw termination exception with the value in a0
        throw ProgramTerminationException(static_cast<int>(ent.value));
      }
    }
    
    // commit the instruction
    if (ent.dest_tag.has_value()) {
      LOG_DEBUG("Writing value " + std::to_string(ent.value) + " to register " + std::to_string(ent.dest_tag.value()));
      reg_file.write(ent.dest_tag.value(), ent.value);
      if (reg_file.get_rob(ent.dest_tag.value()) == ent.id) {
        reg_file.mark_available(ent.dest_tag.value());
        LOG_DEBUG("Marked register " + std::to_string(ent.dest_tag.value()) + " as available");
      }
    }
    rob.dequeue();
    LOG_DEBUG("Instruction committed and removed from ROB");
  } else {
    LOG_DEBUG("Head instruction not ready for commit (ROB ID: " + std::to_string(ent.id) + "), instruction details: " + 
              riscv::to_string(ent.instr));
  }
}

inline void ReorderBuffer::receive_broadcast() {
  LOG_DEBUG("Checking for broadcast results from execution units");
  int broadcasts_received = 0;
  
  // check ALU, Predictor, and LSB
  if (alu.has_result_for_broadcast()) {
    ALUResult result = alu.get_result_for_broadcast();
    LOG_DEBUG("Received ALU broadcast for tag: " + std::to_string(result.dest_tag) + 
              ", result: " + std::to_string(result.result));
    for (int i = 0; i < rob.size(); i++) {
      ReorderBufferEntry &ent = rob.get(i);
      if (ent.id == result.dest_tag) {
        ent.value = result.result;
        ent.ready = true;
        rs.receive_broadcast(result.result, result.dest_tag);
        LOG_DEBUG("Updated ROB entry ID: " + std::to_string(ent.id) + " with ALU result");
        broadcasts_received++;
        break;
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
        ent.ready = true;
        rs.receive_broadcast(result.pc, result.dest_tag.value());
        LOG_DEBUG("Updated ROB entry ID: " + std::to_string(ent.id) + " with Predictor result");
        broadcasts_received++;
        break;
      }
    }
  }
  if (mem.has_result_for_broadcast()) {
    MemoryResult result = mem.get_result_for_broadcast();
    LOG_DEBUG("Received Memory broadcast for tag: " + std::to_string(result.dest_tag) + 
              ", data: " + std::to_string(result.data));
    for (int i = 0; i < rob.size(); i++) {
      ReorderBufferEntry &ent = rob.get(i);
      if (ent.id == result.dest_tag) {
        ent.value = result.data;
        ent.ready = true;
        rs.receive_broadcast(result.data, result.dest_tag);
        LOG_DEBUG("Updated ROB entry ID: " + std::to_string(ent.id) + " with Memory result");
        broadcasts_received++;
        break;
       }
    }
  }
  
  if (broadcasts_received > 0) {
    LOG_DEBUG("Processed " + std::to_string(broadcasts_received) + " broadcasts this cycle");
  }
}

inline void ReorderBuffer::flush() {
  LOG_DEBUG("Flushing ROB - clearing all entries");
  
  while (!rob.isEmpty()) {
    const auto& ent = rob.front();
    if (ent.dest_tag.has_value()) {
      if (reg_file.get_rob(ent.dest_tag.value()) == ent.id) {
        reg_file.mark_available(ent.dest_tag.value());
        LOG_DEBUG("Cleared register dependency for reg" + std::to_string(ent.dest_tag.value()));
      }
    }
    rob.dequeue();
  }
  
  // Reset ID counter
  cur_id = 0;
  LOG_DEBUG("ROB flush completed");
}

#endif // TOMASULO_REORDER_BUFFER_HPP