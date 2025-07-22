#ifndef TOMASULO_RESERVATION_STATION_HPP
#define TOMASULO_RESERVATION_STATION_HPP

#include "../riscv/instruction.hpp"
#include "../utils/queue.hpp"
#include "../core/register_file.hpp"
#include <cstdint>
#include <limits>

class ReservationStationEntry {
public:
  ReservationStationEntry();
  ReservationStationEntry(riscv::DecodedInstruction op, int qj, int qk, int vj, int vk, int imm, int dest_tag) : op(op), qj(qj), qk(qk), vj(vj), vk(vk), imm(imm), dest_tag(dest_tag) {}
  riscv::DecodedInstruction op;
  double vj, vk;
  int qj, qk;
  int imm;
  int dest_tag;
  int cycles_remaining;
};

class ReservationStation {
  CircularQueue<ReservationStationEntry> rs;
  RegisterFile &reg_file;
public:
  ReservationStation(RegisterFile &reg_file);
  void add_entry(riscv::DecodedInstruction op, int qj, int qk, int imm, int dest_tag);
  void execute();
};

ReservationStation::ReservationStation(RegisterFile &reg_file) : rs(32), reg_file(reg_file) {}

void ReservationStation::add_entry(riscv::DecodedInstruction op, int src1, int src2, int imm, int dest_tag) {
  if (!rs.isFull()) {
    // fetch qj and qk from reg_file
    int vj = 0;
    int vk = 0;
    int qj = reg_file.get_rob(src1);
    int qk = 0;
    if (qj == std::numeric_limits<int>::max()) {
      qj = 0;
      vj = reg_file.read(src1);
    }
    if (src2 != -1) {
      // if src2 == -1, then we do not have src2
      qk = reg_file.get_rob(src2);
      if (qk == std::numeric_limits<int>::max()) {
        qk = 0;
        vk = reg_file.read(src2);
      }
    }
    ReservationStationEntry ent(op, qj, qk, vj, vk, imm, dest_tag);
    rs.enqueue(ent);
  }
}

void ReservationStation::execute() {
  for (int i = 0; i < rs.size(); i++) {
    ReservationStationEntry &ent = rs.get(i);
    if (ent.qj == 0 && ent.qk == 0) {
      // execute the instruction
    }
  }
}

#endif // TOMASULO_RESERVATION_STATION_HPP