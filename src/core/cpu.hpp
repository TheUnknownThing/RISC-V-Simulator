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
  void fetch();
  void decode();
  void execute();
  void broadcast();
};

inline CPU::CPU(std::string filename) : reg_file(), rob(reg_file), rs(reg_file), loader(filename), pc(0) {}

inline void CPU::Tick() {
  fetch();
  decode();
  execute();
  broadcast();
}

inline void CPU::fetch() {
  uint32_t instr = loader.fetchInstruction(pc);
  riscv::decode(instr);
  pc += 4;
}



#endif // CORE_CPU_HPP