#ifndef TOMASULO_REORDER_BUFFER_HPP
#define TOMASULO_REORDER_BUFFER_HPP

#include "../core/register_file.hpp"
#include "../riscv/instruction.hpp"
#include "reservation_station.hpp"
#include "../utils/queue.hpp"
#include "core/alu.hpp"
#include "core/lsb.hpp"
#include "core/predictor.hpp"
#include <array>
#include <cstdint>
#include <optional>


struct ReorderBufferEntry {
  ReorderBufferEntry();
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
  Memory &mem;
  ReservationStation &rs;

public:
  ReorderBuffer(RegisterFile &reg_file, ALU &alu, Predictor &predictor, Memory &mem, ReservationStation &rs);

  /**
   * @brief Add an entry to the reorder buffer
   * @param instr The instruction to add
   * @param dest_tag The destination register
   * @return The ID of the entry
   */
  int add_entry(riscv::DecodedInstruction instr, std::optional<uint32_t> dest_tag);
  void commit();
  void receive_broadcast();
};

inline ReorderBuffer::ReorderBuffer(RegisterFile &reg_file, ALU &alu, Predictor &predictor, Memory &mem, ReservationStation &rs)
    : rob(32), reg_file(reg_file), alu(alu), predictor(predictor), mem(mem), rs(rs) {}

inline int ReorderBuffer::add_entry(riscv::DecodedInstruction instr,
                                    std::optional<uint32_t> dest_tag) {
  if (!rob.isFull()) {
    ReorderBufferEntry ent(instr, dest_tag, cur_id++);
    rob.enqueue(ent);
    return ent.id;
  }
  return -1;
}

inline void ReorderBuffer::commit() {
  const auto &ent = rob.front();
  if (ent.ready) {
    // commit the instruction
    if (ent.dest_tag.has_value()) {
      reg_file.write(ent.dest_tag.value(), ent.value);
      if (reg_file.get_rob(ent.dest_tag.value()) == ent.id) {
        reg_file.mark_available(ent.dest_tag.value());
      }
    }
    rob.dequeue();
  }
}

inline void ReorderBuffer::receive_broadcast() {
  // check ALU, Predictor, and LSB
  if (alu.is_available()) {
    ALUResult result = alu.get_result();
    for (int i = 0; i < rob.size(); i++) {
      ReorderBufferEntry &ent = rob.get(i);
      if (ent.dest_tag == result.dest_tag) {
        ent.value = result.result;
        ent.ready = true;
        rs.receive_broadcast(result.result, result.dest_tag);
        break;
      }
    }
  }
  if (predictor.is_available()) {
    PredictorResult result = predictor.get_result();
    for (int i = 0; i < rob.size(); i++) {
      ReorderBufferEntry &ent = rob.get(i);
      if (ent.dest_tag == result.dest_tag) {
        ent.value = result.result;
        ent.ready = true;
        rs.receive_broadcast(result.result, result.dest_tag);
        break;
      }
    }
  }
  if (mem.is_available()) {
    MemResult result = mem.get_result();
    for (int i = 0; i < rob.size(); i++) {
      ReorderBufferEntry &ent = rob.get(i);
      if (ent.dest_tag == result.dest_tag) {
        ent.value = result.result;
        ent.ready = true;
        rs.receive_broadcast(result.result, result.dest_tag);
        break;
       }
    }
  }
}

#endif // TOMASULO_REORDER_BUFFER_HPP