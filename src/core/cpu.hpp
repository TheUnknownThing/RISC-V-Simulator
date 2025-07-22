#ifndef CORE_CPU_HPP
#define CORE_CPU_HPP

#include "../tomasulo/reorder_buffer.hpp"
#include "../tomasulo/reservation_station.hpp"
#include "../utils/binary_loader.hpp"
#include "../riscv/decoder.hpp"
#include "register_file.hpp"

class CPU {
  RegisterFile reg_file;
  ReorderBuffer rob;
  ReservationStation rs;
  BinaryLoader loader;
  uint32_t pc;
public:
  CPU(std::string filename);
  void Tick();

private:
  riscv::DecodedInstruction fetch();
  void issue(riscv::DecodedInstruction instr);
  void execute();
  void broadcast();
};

inline CPU::CPU(std::string filename) : reg_file(), rob(reg_file), rs(reg_file), loader(filename), pc(0) {}

inline void CPU::Tick() {
  riscv::DecodedInstruction instr = fetch();
  issue(instr);
  execute();
  broadcast();
}

inline riscv::DecodedInstruction CPU::fetch() {
  uint32_t instr = loader.fetchInstruction(pc);
  riscv::DecodedInstruction decoded_instr = riscv::decode(instr);
  pc += 4;
  return decoded_instr;
}

inline void CPU::issue(riscv::DecodedInstruction instr) {
  rs.add_entry(instr, instr.rs1, instr.rs2, instr.imm, instr.rd);
}

inline void CPU::execute() {

}

inline void CPU::broadcast() {
  rob.receive_broadcast();
}

#endif // CORE_CPU_HPP