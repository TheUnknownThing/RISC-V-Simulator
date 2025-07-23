#ifndef CORE_CPU_HPP
#define CORE_CPU_HPP

#include "../riscv/decoder.hpp"
#include "../tomasulo/reorder_buffer.hpp"
#include "../tomasulo/reservation_station.hpp"
#include "../utils/binary_loader.hpp"
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
  void commit();
};

inline CPU::CPU(std::string filename)
    : reg_file(), rob(reg_file), rs(reg_file), loader(filename), pc(0) {}

inline void CPU::Tick() {
  riscv::DecodedInstruction instr = fetch();
  issue(instr);
  execute();
  broadcast();
  commit();
}

inline riscv::DecodedInstruction CPU::fetch() {
  uint32_t instr = loader.fetchInstruction(pc);
  riscv::DecodedInstruction decoded_instr = riscv::decode(instr);
  pc += 4;
  return decoded_instr;
}

inline void CPU::issue(riscv::DecodedInstruction instr) {
  if (std::holds_alternative<std::monostate>(instr)) {
    throw std::runtime_error("Invalid instruction");
    return;
  }

  std::optional<uint32_t> rd, rs1, rs2;
  std::optional<int32_t> imm;

  if (auto *r_instr = std::get_if<riscv::R_Instruction>(&instr)) {
    rd = r_instr->rd;
    rs1 = r_instr->rs1;
    rs2 = r_instr->rs2;
  } else if (auto *i_instr = std::get_if<riscv::I_Instruction>(&instr)) {
    rd = i_instr->rd;
    rs1 = i_instr->rs1;
    imm = i_instr->imm;
  } else if (auto *s_instr = std::get_if<riscv::S_Instruction>(&instr)) {
    rs1 = s_instr->rs1;
    rs2 = s_instr->rs2;
    imm = s_instr->imm;
  } else if (auto *b_instr = std::get_if<riscv::B_Instruction>(&instr)) {
    rs1 = b_instr->rs1;
    rs2 = b_instr->rs2;
    imm = b_instr->imm;
  } else if (auto *u_instr = std::get_if<riscv::U_Instruction>(&instr)) {
    rd = u_instr->rd;
    imm = u_instr->imm;
  } else if (auto *j_instr = std::get_if<riscv::J_Instruction>(&instr)) {
    rd = j_instr->rd;
    imm = j_instr->imm;
  }

  int id = rob.add_entry(instr, rd);

  if (id != -1) {
    rs.add_entry(instr, rs1, rs2, imm, id);
  } else {
    pc -= 4;
  }
}

inline void CPU::execute() {}

inline void CPU::broadcast() { rob.receive_broadcast(); }

inline void CPU::commit() { rob.commit(); }

#endif // CORE_CPU_HPP