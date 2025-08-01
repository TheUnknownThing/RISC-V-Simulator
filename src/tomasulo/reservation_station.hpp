#ifndef TOMASULO_RESERVATION_STATION_HPP
#define TOMASULO_RESERVATION_STATION_HPP

#include "../riscv/instruction.hpp"
#include "../utils/logger.hpp"
#include "../utils/queue.hpp"
#include <cstdint>
#include <limits>
#include <optional>

struct ReservationStationEntry {
  ReservationStationEntry() = default;
  ReservationStationEntry(riscv::DecodedInstruction op, uint32_t qj,
                          uint32_t qk, int vj, int vk, int imm,
                          uint32_t dest_tag, uint32_t pc = 0)
      : op(op), vj(vj), vk(vk), qj(qj), qk(qk), imm(imm), dest_tag(dest_tag),
        pc(pc) {}
  riscv::DecodedInstruction op;
  int32_t vj, vk;
  uint32_t qj, qk;
  int32_t imm;
  uint32_t dest_tag;
  uint32_t pc;
};

class ReservationStation {
public:
  CircularQueue<ReservationStationEntry> rs;
  ReservationStation();
  void add_entry(const riscv::DecodedInstruction &op, int32_t vj, int32_t vk,
                 uint32_t qj, uint32_t qk, std::optional<int32_t> imm,
                 int dest_tag, uint32_t pc = 0);
  void receive_broadcast(int32_t value, uint32_t dest_tag);
  void flush();
  // void print_debug_info();
};

inline ReservationStation::ReservationStation() : rs(32) {
  LOG_DEBUG("ReservationStation initialized with capacity: 32");
}

inline void ReservationStation::add_entry(const riscv::DecodedInstruction &op,
                                          int32_t vj, int32_t vk, uint32_t qj,
                                          uint32_t qk,
                                          std::optional<int32_t> imm,
                                          int dest_tag, uint32_t pc) {
  if (!rs.isFull()) {
    LOG_DEBUG(
        "Adding pre-processed entry to Reservation Station with dest_tag: " +
        std::to_string(dest_tag));

    ReservationStationEntry ent(op, qj, qk, vj, vk, imm.value_or(0), dest_tag,
                                pc);

    rs.enqueue(ent);
    LOG_DEBUG("Entry added successfully. qj=" + std::to_string(qj) +
              ", qk=" + std::to_string(qk));
  } else {
    LOG_WARN("Reservation Station is full, cannot add new entry");
  }
}

inline void ReservationStation::receive_broadcast(int32_t value,
                                                  uint32_t dest_tag) {
  LOG_DEBUG("Receiving broadcast for tag: " + std::to_string(dest_tag) +
            ", value: " + std::to_string(value));
  int updated_entries = 0;

  for (int i = 0; i < rs.size(); i++) {
    ReservationStationEntry &ent = rs.get(i);
    bool updated = false;

    if (ent.qj == dest_tag) {
      ent.vj = value;
      ent.qj = std::numeric_limits<uint32_t>::max();
      LOG_DEBUG("Updated operand vj for RS entry " + std::to_string(i));
      updated = true;
    }
    if (ent.qk == dest_tag) {
      ent.vk = value;
      ent.qk = std::numeric_limits<uint32_t>::max();
      LOG_DEBUG("Updated operand vk for RS entry " + std::to_string(i));
      updated = true;
    }

    if (updated) {
      updated_entries++;
      if (ent.qj == std::numeric_limits<uint32_t>::max() &&
          ent.qk == std::numeric_limits<uint32_t>::max()) {
        LOG_DEBUG("RS entry " + std::to_string(i) + " now ready for execution");
      }
    }
  }

  LOG_DEBUG("Broadcast updated " + std::to_string(updated_entries) +
            " reservation station entries");
}

inline void ReservationStation::flush() {
  LOG_DEBUG("Flushing Reservation Station - clearing all entries");

  while (!rs.isEmpty()) {
    rs.dequeue();
  }

  LOG_DEBUG("Reservation Station flush completed");
}

// inline void ReservationStation::print_debug_info() {
//   LOG_DEBUG("Reservation Station Debug Info:");
//   for (int i = 0; i < rs.size(); ++i) {
//     const ReservationStationEntry &ent = rs.get(i);
//     LOG_DEBUG("RS[" + std::to_string(i) + "] - op: " +
//     riscv::to_string(ent.op) +
//               ", vj: " + std::to_string(ent.vj) +
//               ", vk: " + std::to_string(ent.vk) +
//               ", qj: " + std::to_string(ent.qj) +
//               ", qk: " + std::to_string(ent.qk) +
//               ", imm: " + std::to_string(ent.imm) +
//               ", dest_tag: " + std::to_string(ent.dest_tag));
//   }
// }

#endif // TOMASULO_RESERVATION_STATION_HPP