#ifndef TOMASULO_REORDER_BUFFER_HPP
#define TOMASULO_REORDER_BUFFER_HPP

#include "../riscv/instruction.hpp"
#include "../utils/queue.hpp"
#include <array>
#include <cstdint>

enum class State { ISSUE, EXECUTE, WRITE_RESULT, COMMIT };
class ReorderBufferEntry {
public:
  ReorderBufferEntry();
  ReorderBufferEntry(riscv::DecodedInstruction instr, int dest_reg)
      : busy(true), instr(instr), state(State::ISSUE), dest_reg(dest_reg),
        value(-1), ready(false), exception_flag(false) {}
  bool busy;
  riscv::DecodedInstruction instr;
  State state;
  int dest_reg;
  double value;
  bool ready;
  bool exception_flag;
};

class ReorderBuffer {
  CircularQueue<ReorderBufferEntry> rob;

public:
  ReorderBuffer();
  void add_entry(riscv::DecodedInstruction instr, int dest_reg);
  void execute();
  void commit();
  void receive_broadcast();
};

ReorderBuffer::ReorderBuffer() : rob(32) {}

void ReorderBuffer::add_entry(riscv::DecodedInstruction instr, int dest_reg) {
  if (!rob.isFull()) {
    ReorderBufferEntry ent(instr, dest_reg);
    rob.enqueue(ent);
  }
}

#endif // TOMASULO_REORDER_BUFFER_HPP