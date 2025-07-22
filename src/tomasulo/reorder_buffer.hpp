#ifndef TOMASULO_REORDER_BUFFER_HPP
#define TOMASULO_REORDER_BUFFER_HPP

#include "../riscv/instruction.hpp"
#include "../utils/queue.hpp"
#include "../core/register_file.hpp"
#include <array>
#include <cstdint>

enum class State { ISSUE, EXECUTE, WRITE_RESULT, COMMIT };
class ReorderBufferEntry {
public:
  ReorderBufferEntry();
  ReorderBufferEntry(riscv::DecodedInstruction instr, int dest_reg, uint32_t id)
      : busy(true), instr(instr), state(State::ISSUE), dest_reg(dest_reg),
        value(-1), ready(false), exception_flag(false), id(id) {}
  bool busy;
  riscv::DecodedInstruction instr;
  State state;
  int dest_reg;
  double value;
  bool ready;
  bool exception_flag;
  uint32_t id;
};

class ReorderBuffer {
  CircularQueue<ReorderBufferEntry> rob;
  uint32_t cur_id = 0;
  RegisterFile &reg_file;
public:
  ReorderBuffer(RegisterFile &reg_file);
  void add_entry(riscv::DecodedInstruction instr, int dest_reg);
  void commit();
  void receive_broadcast();
};

inline ReorderBuffer::ReorderBuffer(RegisterFile &reg_file) : rob(32), reg_file(reg_file) {}

inline void ReorderBuffer::add_entry(riscv::DecodedInstruction instr, int dest_reg) {
  if (!rob.isFull()) {
    ReorderBufferEntry ent(instr, dest_reg, cur_id++);
    rob.enqueue(ent);
  }
}

inline void ReorderBuffer::commit() {
  const auto &ent = rob.front();
  if (ent.state == State::COMMIT) {
    // commit the instruction
    reg_file.write(ent.dest_reg, ent.value);
    rob.dequeue();
  }
}

#endif // TOMASULO_REORDER_BUFFER_HPP