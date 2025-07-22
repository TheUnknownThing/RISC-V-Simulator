#ifndef TOMASULO_REORDER_BUFFER_HPP
#define TOMASULO_REORDER_BUFFER_HPP

#include "../riscv/instruction.hpp"
#include <cstdint>

enum class State { ISSUE, EXECUTE, WRITE_RESULT, COMMIT };
class ReorderBufferEntry {
public:
  bool busy;
  riscv::DecodedInstruction instr;
  State state;
  int dest_reg;
  double value;
  bool ready;
  bool exception_flag;
};

class ReorderBuffer {
public:
  ReorderBuffer();
  void add_entry(const ReorderBufferEntry& entry);
  void execute();
  void commit();
};

#endif // TOMASULO_REORDER_BUFFER_HPP