#ifndef TOMASULO_RESERVATION_STATION_HPP
#define TOMASULO_RESERVATION_STATION_HPP

#include "../riscv/instruction.hpp"
#include "../utils/queue.hpp"
#include <cstdint>

class ReservationStationEntry {
public:
  ReservationStationEntry();
  ReservationStationEntry(riscv::DecodedInstruction op, int qj, int qk, int imm, int dest_tag) : op(op), qj(qj), qk(qk), imm(imm), dest_tag(dest_tag) {}
  riscv::DecodedInstruction op;
  double vj, vk;
  int qj, qk;
  int imm;
  int dest_tag;
  int cycles_remaining;
};

class ReservationStation {
  CircularQueue<ReservationStationEntry> rs;
public:
  ReservationStation();
  void add_entry(riscv::DecodedInstruction op, int qj, int qk, int imm, int dest_tag);
  void execute();
};

ReservationStation::ReservationStation() : rs(32) {}

void ReservationStation::add_entry(riscv::DecodedInstruction op, int qj, int qk, int imm, int dest_tag) {
  if (!rs.isFull()) {
    // let cpu.hpp fetch the value of qj and qk from RegisterFile
    ReservationStationEntry ent(op, qj, qk, imm, dest_tag);
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