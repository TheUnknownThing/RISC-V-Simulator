#ifndef CORE_REGISTER_FILE_HPP
#define CORE_REGISTER_FILE_HPP

#include <array>
#include <cstdint>

class RegisterFile {
public:
  RegisterFile();
  void write(uint32_t rd, uint32_t value);
  uint32_t read(uint32_t rd) const;
  void flush();

private:
  std::array<uint32_t, 32> registers;
};

inline RegisterFile::RegisterFile() { registers.fill(0); }

inline void RegisterFile::write(uint32_t rd, uint32_t value) {
  registers[rd] = value;
}

inline uint32_t RegisterFile::read(uint32_t rd) const { return registers[rd]; }

inline void RegisterFile::flush() { registers.fill(0); }

#endif // CORE_REGISTER_FILE_HPP