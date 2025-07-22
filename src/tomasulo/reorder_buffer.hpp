#ifndef TOMASULO_REORDER_BUFFER_HPP
#define TOMASULO_REORDER_BUFFER_HPP

#include <cstdint>
#include "../riscv/instruction.hpp"

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

#endif // TOMASULO_REORDER_BUFFER_HPP