#ifndef CORE_MEMORY_HPP
#define CORE_MEMORY_HPP

#include <cstdint>
#include <array>

struct LSBEntry {
    uint32_t address;
    uint8_t value;
    uint32_t cycle_remaining;
};

class Memory {
  std::array<uint8_t, 1024> memory;
  uint8_t cycle_remaining;

public:
  Memory();
  uint8_t read(uint32_t address);
  void write(uint32_t address, uint8_t value);
  bool is_available() const;
  void tick();
};

inline Memory::Memory() : cycle_remaining(0) {
  memory.fill(0);
}


inline uint8_t Memory::read(uint32_t address) {
  // read costs 3 cycles
  cycle_remaining = 3;
  return memory[address];
}

inline void Memory::write(uint32_t address, uint8_t value) {
  // write costs 3 cycles
  cycle_remaining = 3;
  memory[address] = value;
}

inline bool Memory::is_available() const {
  return cycle_remaining == 0;
}

inline void Memory::tick() {
  if (cycle_remaining > 0) {
    cycle_remaining--;
  }
}

#endif