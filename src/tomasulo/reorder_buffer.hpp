#ifndef TOMASULO_REORDER_BUFFER_HPP
#define TOMASULO_REORDER_BUFFER_HPP

#include "../core/register_file.hpp"
#include "../riscv/instruction.hpp"
#include "../utils/queue.hpp"
#include <array>
#include <cstdint>
#include <optional>

enum class State { ISSUE, EXECUTE, WRITE_RESULT, COMMIT };

struct ReorderBufferEntry {
  ReorderBufferEntry();
  ReorderBufferEntry(riscv::DecodedInstruction instr, std::optional<uint32_t> dest_reg, uint32_t id)
      : busy(true), instr(instr), state(State::ISSUE), dest_reg(dest_reg),
        value(-1), ready(false), exception_flag(false), id(id) {}
  bool busy;
  riscv::DecodedInstruction instr;
  State state;
  std::optional<uint32_t> dest_reg;
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

  /**
   * @brief Add an entry to the reorder buffer
   * @param instr The instruction to add
   * @param dest_reg The destination register
   * @return The ID of the entry
   */
  int add_entry(riscv::DecodedInstruction instr, std::optional<uint32_t> dest_reg);
  void commit();
  void receive_broadcast();
};

inline ReorderBuffer::ReorderBuffer(RegisterFile &reg_file)
    : rob(32), reg_file(reg_file) {}

inline int ReorderBuffer::add_entry(riscv::DecodedInstruction instr,
                                    std::optional<uint32_t> dest_reg) {
  if (!rob.isFull()) {
    ReorderBufferEntry ent(instr, dest_reg, cur_id++);
    rob.enqueue(ent);
    return ent.id;
  }
  return -1;
}

inline void ReorderBuffer::commit() {
  const auto &ent = rob.front();
  if (ent.state == State::COMMIT) {
    // commit the instruction
    if (ent.dest_reg.has_value()) {
      reg_file.write(ent.dest_reg.value(), ent.value);
    }
    rob.dequeue();
  }
}

#endif // TOMASULO_REORDER_BUFFER_HPP