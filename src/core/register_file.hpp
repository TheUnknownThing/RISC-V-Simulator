#ifndef CORE_REGISTER_FILE_HPP
#define CORE_REGISTER_FILE_HPP

#include <array>
#include <cstdint>
#include <limits>
#include "../utils/logger.hpp"

class RegisterFile {
public:
  RegisterFile();
  void write(uint32_t rd, uint32_t value);
  uint32_t read(uint32_t rd) const;
  void flush();
  void receive_rob(uint32_t rd, uint32_t id);
  void mark_available(uint32_t rd);
  uint32_t get_rob(uint32_t rd) const;
  void print_debug_info() const;

private:
  std::array<uint32_t, 32> registers;
  std::array<uint32_t, 32> rob_id;
};

inline RegisterFile::RegisterFile() {
  registers.fill(0);
  rob_id.fill(std::numeric_limits<uint32_t>::max()); // All registers initially available
}

inline void RegisterFile::write(uint32_t rd, uint32_t value) {
  if (rd == 0) {
    LOG_WARN("Attempted to write to register zero, ignoring.");
    return; // Register zero is always zero
  }
  registers[rd] = value;
}

inline void RegisterFile::receive_rob(uint32_t rd, uint32_t id) {
  if (rd == 0) {
    LOG_WARN("Attempted to assign ROB ID to register zero, ignoring.");
    return;
  }
  rob_id[rd] = id;
}

inline void RegisterFile::mark_available(uint32_t rd) {
  if (rd == 0) {
    return; // Register zero is always available
  }
  rob_id[rd] = std::numeric_limits<uint32_t>::max();
}

inline uint32_t RegisterFile::get_rob(uint32_t rd) const {
  if (rd == 0) {
    return std::numeric_limits<uint32_t>::max();
  }
  return rob_id[rd];
}

inline uint32_t RegisterFile::read(uint32_t rd) const { return registers[rd]; }

inline void RegisterFile::flush() { 
  registers.fill(0);
  rob_id.fill(std::numeric_limits<uint32_t>::max());
}

inline void RegisterFile::print_debug_info() const {
  LOG_DEBUG("Register File Debug Info:");
  for (size_t i = 0; i < registers.size(); ++i) {
    LOG_DEBUG("reg[" + std::to_string(i) + "] = " + std::to_string(registers[i]) +
              ", ROB ID: " + std::to_string(rob_id[i]));
  }
}

#endif // CORE_REGISTER_FILE_HPP