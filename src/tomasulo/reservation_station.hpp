#ifndef TOMASULO_RESERVATION_STATION_HPP
#define TOMASULO_RESERVATION_STATION_HPP

#include "../riscv/instruction.hpp"
#include <cstdint>

class ReservationStationEntry {
public:
  bool busy;
  riscv::DecodedInstruction op;
  double vj, vk;
  int qj, qk;
  int dest_tag;
  int cycles_remaining;
};

class ReservationStation {
public:
  ReservationStation();
  void add_entry(const ReservationStationEntry& entry);
  void execute();
  void commit();
};

#endif // TOMASULO_RESERVATION_STATION_HPP