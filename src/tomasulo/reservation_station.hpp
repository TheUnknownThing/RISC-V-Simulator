#ifndef TOMASULO_RESERVATION_STATION_HPP
#define TOMASULO_RESERVATION_STATION_HPP

#include "../riscv/instruction.hpp"
#include "../utils/queue.hpp"
#include "../utils/logger.hpp"
#include "../core/register_file.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <functional>

struct ReservationStationEntry {
  ReservationStationEntry() = default;
  ReservationStationEntry(riscv::DecodedInstruction op, uint32_t qj, uint32_t qk, int vj, int vk, int imm, uint32_t dest_tag) : op(op), vj(vj), vk(vk), qj(qj), qk(qk), imm(imm), dest_tag(dest_tag) {}
  riscv::DecodedInstruction op;
  int32_t vj, vk;
  uint32_t qj, qk;
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
  void flush();
  void print_debug_info();
};

inline ReservationStation::ReservationStation(RegisterFile &reg_file) : reg_file(reg_file), rs(32) {
  LOG_DEBUG("ReservationStation initialized with capacity: 32");
}

inline void ReservationStation::add_entry(const riscv::DecodedInstruction &op, std::optional<uint32_t> src1, std::optional<uint32_t> src2, std::optional<uint32_t> imm, int dest_tag) {
  if (!rs.isFull()) {
    LOG_DEBUG("Adding entry to Reservation Station with dest_tag: " + std::to_string(dest_tag));
    
    // fetch qj and qk from reg_file
    int vj = 0;
    int vk = 0;
    uint32_t qj = src1.has_value() ? reg_file.get_rob(src1.value()) : std::numeric_limits<uint32_t>::max();
    uint32_t qk = src2.has_value() ? reg_file.get_rob(src2.value()) : std::numeric_limits<uint32_t>::max();
    
    if (qj == std::numeric_limits<uint32_t>::max()) {
      qj = 0;
      vj = src1.has_value() ? reg_file.read(src1.value()) : 0;
      if (src1.has_value()) {
        LOG_DEBUG("Source operand 1 ready: reg" + std::to_string(src1.value()) + " = " + std::to_string(vj));
      }
    } else {
      LOG_DEBUG("Source operand 1 waiting for ROB entry: " + std::to_string(qj));
    }
    
    if (src2.has_value()) {
      qk = reg_file.get_rob(src2.value());
      if (qk == std::numeric_limits<uint32_t>::max()) {
        qk = 0;
        vk = reg_file.read(src2.value());
        LOG_DEBUG("Source operand 2 ready: reg" + std::to_string(src2.value()) + " = " + std::to_string(vk));
      } else {
        LOG_DEBUG("Source operand 2 waiting for ROB entry: " + std::to_string(qk));
      }
    } else {
      qk = 0;
      vk = imm.value_or(0);
      LOG_DEBUG("Using immediate value: " + std::to_string(vk));
    }
    ReservationStationEntry ent(op, qj, qk, vj, vk, imm.value_or(0), dest_tag);
    rs.enqueue(ent);
    LOG_DEBUG("Entry added to Reservation Station successfully");
  } else {
    LOG_WARN("Reservation Station is full, cannot add new entry");
  }
}

inline void ReservationStation::receive_broadcast(int32_t value, uint32_t dest_tag) {
  LOG_DEBUG("Receiving broadcast for tag: " + std::to_string(dest_tag) + ", value: " + std::to_string(value));
  int updated_entries = 0;
  
  for (int i = 0; i < rs.size(); i++) {
    ReservationStationEntry &ent = rs.get(i);
    bool updated = false;
    
    if (ent.qj == dest_tag) {
      ent.vj = value;
      ent.qj = 0;
      LOG_DEBUG("Updated operand vj for RS entry " + std::to_string(i));
      updated = true;
    }
    if (ent.qk == dest_tag) {
      ent.vk = value;
      ent.qk = 0;
      LOG_DEBUG("Updated operand vk for RS entry " + std::to_string(i));
      updated = true;
    }
    
    if (updated) {
      updated_entries++;
      if (ent.qj == 0 && ent.qk == 0) {
        LOG_DEBUG("RS entry " + std::to_string(i) + " now ready for execution");
      }
    }
  }
  
  LOG_DEBUG("Broadcast updated " + std::to_string(updated_entries) + " reservation station entries");
}

inline void ReservationStation::flush() {
  LOG_DEBUG("Flushing Reservation Station - clearing all entries");
  
  while (!rs.isEmpty()) {
    rs.dequeue();
  }
  
  LOG_DEBUG("Reservation Station flush completed");
}

inline void ReservationStation::print_debug_info() {
  LOG_DEBUG("Reservation Station Debug Info:");
  for (int i = 0; i < rs.size(); ++i) {
    const ReservationStationEntry &ent = rs.get(i);
    LOG_DEBUG("RS[" + std::to_string(i) + "] - op: " + riscv::to_string(ent.op) +
              ", vj: " + std::to_string(ent.vj) +
              ", vk: " + std::to_string(ent.vk) +
              ", qj: " + std::to_string(ent.qj) +
              ", qk: " + std::to_string(ent.qk) +
              ", imm: " + std::to_string(ent.imm) +
              ", dest_tag: " + std::to_string(ent.dest_tag));
  }
}

#endif // TOMASULO_RESERVATION_STATION_HPP