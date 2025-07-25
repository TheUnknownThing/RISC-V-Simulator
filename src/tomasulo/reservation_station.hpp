#ifndef TOMASULO_RESERVATION_STATION_HPP
#define TOMASULO_RESERVATION_STATION_HPP

#include "../riscv/instruction.hpp"
#include "../utils/queue.hpp"
#include "../core/register_file.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <functional>

struct ReservationStationEntry {
  ReservationStationEntry();
  ReservationStationEntry(riscv::DecodedInstruction op, int qj, int qk, int vj, int vk, int imm, uint32_t dest_tag) : op(op), vj(vj), vk(vk), qj(qj), qk(qk), imm(imm), dest_tag(dest_tag) {}
  riscv::DecodedInstruction op;
  int32_t vj, vk;
  int qj, qk;
  int32_t imm;
  uint32_t dest_tag;
};

class ReservationStation {
  RegisterFile &reg_file;
  
public:
  CircularQueue<ReservationStationEntry> rs;
  ReservationStation(RegisterFile &reg_file);
  void add_entry(const riscv::DecodedInstruction &op, std::optional<uint32_t> qj, std::optional<uint32_t> qk, std::optional<uint32_t> imm, int dest_tag);
  void receive_broadcast(int32_t value, uint32_t dest_tag);
};

inline ReservationStation::ReservationStation(RegisterFile &reg_file) : reg_file(reg_file), rs(32) {}

inline void ReservationStation::add_entry(const riscv::DecodedInstruction &op, std::optional<uint32_t> src1, std::optional<uint32_t> src2, std::optional<uint32_t> imm, int dest_tag) {
  if (!rs.isFull()) {
    // fetch qj and qk from reg_file
    int vj = 0;
    int vk = 0;
    uint32_t qj = src1.has_value() ? reg_file.get_rob(src1.value()) : std::numeric_limits<uint32_t>::max();
    uint32_t qk = src2.has_value() ? reg_file.get_rob(src2.value()) : std::numeric_limits<uint32_t>::max();
    if (qj == std::numeric_limits<uint32_t>::max()) {
      qj = 0;
      vj = src1.has_value() ? reg_file.read(src1.value()) : 0;
    }
    if (src2.has_value()) {
      qk = reg_file.get_rob(src2.value());
      if (qk == std::numeric_limits<uint32_t>::max()) {
        qk = 0;
        vk = reg_file.read(src2.value());
      }
    }
    ReservationStationEntry ent(op, qj, qk, vj, vk, imm.value(), dest_tag);
    rs.enqueue(ent);
  }
}

inline void ReservationStation::receive_broadcast(int32_t value, uint32_t dest_tag) {
  for (int i = 0; i < rs.size(); i++) {
    ReservationStationEntry &ent = rs.get(i);
    if (ent.dest_tag == dest_tag) {
      if (ent.qj == 0) {
        ent.vj = value;
      }
      if (ent.qk == 0) {
        ent.vk = value;
      }
      break;
    }
  }
}

#endif // TOMASULO_RESERVATION_STATION_HPP