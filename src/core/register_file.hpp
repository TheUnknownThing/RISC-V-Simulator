#ifndef CORE_REGISTER_FILE_HPP
#define CORE_REGISTER_FILE_HPP

#include <array>
#include <cstdint>
#include <limits>

class RegisterFile {
public:
  RegisterFile();
  void write(uint32_t rd, uint32_t value);
  uint32_t read(uint32_t rd) const;
  void flush();
  void receive_rob(uint32_t rd, uint32_t id);
  void mark_available(uint32_t rd);
  uint32_t get_rob(uint32_t rd) const;

private:
  std::array<uint32_t, 32> registers;
  std::array<uint32_t, 32> rob_id;
};

inline RegisterFile::RegisterFile() {
  registers.fill(0);
  rob_id.fill(std::numeric_limits<uint32_t>::max()); // All registers initially available
}

inline void RegisterFile::write(uint32_t rd, uint32_t value) {
  registers[rd] = value;
}

inline void RegisterFile::receive_rob(uint32_t rd, uint32_t id) { rob_id[rd] = id; }

inline void RegisterFile::mark_available(uint32_t rd) {
  rob_id[rd] = std::numeric_limits<uint32_t>::max();
}

inline uint32_t RegisterFile::get_rob(uint32_t rd) const {
  return rob_id[rd];
}

inline uint32_t RegisterFile::read(uint32_t rd) const { return registers[rd]; }

inline void RegisterFile::flush() { registers.fill(0); }

#endif // CORE_REGISTER_FILE_HPP