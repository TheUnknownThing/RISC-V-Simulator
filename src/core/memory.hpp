#ifndef CORE_MEMORY_HPP
#define CORE_MEMORY_HPP

#include "utils/logger.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_map>

enum class LSBOpType { LOAD, STORE };

struct LSBInstruction {
  LSBOpType op_type;
  int32_t address;
  int32_t data;
  int32_t imm; // Immediate offset for address calculation
  uint32_t dest_tag;
  uint32_t rob_id;
  bool can_execute;
};

struct LSBEntry {
  LSBInstruction instruction;
  uint32_t cycles_remaining;
  bool committed;
  bool executing;

  LSBEntry(LSBInstruction inst)
      : instruction(inst), cycles_remaining(0), committed(false),
        executing(false) {}
};

struct MemoryResult {
  int32_t data;
  uint32_t dest_tag;
  uint32_t rob_id;
  LSBOpType op_type;
};

class Memory {
  std::unordered_map<uint32_t, uint8_t> memory_data;

public:
  int32_t read(uint32_t address) const;
  void write(uint32_t address, int32_t data);
  void write_byte(uint32_t address, uint8_t data);
  uint8_t read_byte(uint32_t address) const;
  void initialize_from_loader(const std::map<uint32_t, uint8_t>& loader_memory);
};

class LSB {
  std::vector<LSBEntry> lsb_entries;
  std::optional<MemoryResult> broadcast_result;
  std::optional<MemoryResult> next_broadcast_result;
  Memory memory;
  bool busy;

public:
  LSB();

  bool is_full() const;
  void add_instruction(LSBInstruction instruction);

  void tick();
  bool has_result_for_broadcast() const;
  MemoryResult get_result_for_broadcast() const;

  void commit_memory(uint32_t rob_id);

  bool is_available() const;
  void flush();
  Memory& get_memory();
};

// Memory implementation
inline int32_t Memory::read(uint32_t address) const {
  // Read 4 bytes and combine them into a 32-bit value (little-endian)
  uint8_t byte0 = read_byte(address);
  uint8_t byte1 = read_byte(address + 1);
  uint8_t byte2 = read_byte(address + 2);
  uint8_t byte3 = read_byte(address + 3);
  return static_cast<int32_t>(byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24));
}

inline void Memory::write(uint32_t address, int32_t data) {
  // Write 32-bit value as 4 bytes (little-endian)
  write_byte(address, static_cast<uint8_t>(data & 0xFF));
  write_byte(address + 1, static_cast<uint8_t>((data >> 8) & 0xFF));
  write_byte(address + 2, static_cast<uint8_t>((data >> 16) & 0xFF));
  write_byte(address + 3, static_cast<uint8_t>((data >> 24) & 0xFF));
}

inline uint8_t Memory::read_byte(uint32_t address) const {
  auto it = memory_data.find(address);
  return (it != memory_data.end()) ? it->second : 0;
}

inline void Memory::write_byte(uint32_t address, uint8_t data) {
  memory_data[address] = data;
}

inline void Memory::initialize_from_loader(const std::map<uint32_t, uint8_t>& loader_memory) {
  memory_data.clear();
  for (const auto& entry : loader_memory) {
    memory_data[entry.first] = entry.second;
  }
  LOG_INFO("Memory initialized with " + std::to_string(loader_memory.size()) + " bytes from binary loader");
}

// LSB implementation
inline LSB::LSB()
    : broadcast_result(std::nullopt), next_broadcast_result(std::nullopt),
      busy(false) {}

constexpr size_t LSB_SIZE = 32;

inline bool LSB::is_full() const { return lsb_entries.size() >= LSB_SIZE; }

inline void LSB::add_instruction(LSBInstruction instruction) {
  // Check if an entry with the same ROB ID already exists
  auto existing_it = std::find_if(
      lsb_entries.begin(), lsb_entries.end(),
      [&instruction](const LSBEntry &entry) {
        return entry.instruction.rob_id == instruction.rob_id;
      });
  
  if (existing_it != lsb_entries.end()) {
    // Update the can_execute field of the existing entry
    existing_it->instruction.can_execute = instruction.can_execute;
    existing_it->instruction.address = instruction.address;
    existing_it->instruction.data = instruction.data;
    existing_it->instruction.imm = instruction.imm;
    existing_it->instruction.dest_tag = instruction.dest_tag;
    LOG_DEBUG("Updated can_execute for existing LSB entry with ROB ID: " + 
              std::to_string(instruction.rob_id) + " to " + 
              std::to_string(instruction.can_execute));
    return;
  }
  
  // If no existing entry found, add new entry
  if (is_full()) {
    throw std::runtime_error("LSB is full");
  }
  
  LSBEntry new_entry(instruction);
  
  // Insert in order by ROB ID
  auto insert_pos = std::lower_bound(
      lsb_entries.begin(), lsb_entries.end(), new_entry,
      [](const LSBEntry &a, const LSBEntry &b) {
        return a.instruction.rob_id < b.instruction.rob_id;
      });
  
  lsb_entries.insert(insert_pos, new_entry);
  busy = true;
}

inline bool LSB::is_available() const { return !busy; }

inline bool LSB::has_result_for_broadcast() const {
  return broadcast_result.has_value();
}

inline MemoryResult LSB::get_result_for_broadcast() const {
  if (!broadcast_result.has_value()) {
    throw std::runtime_error("No memory result available for broadcast");
  }
  return broadcast_result.value();
}

inline void LSB::commit_memory(uint32_t rob_id) {
  for (auto &entry : lsb_entries) {
    if (entry.instruction.rob_id <= rob_id) {
      entry.committed = true;
      LOG_DEBUG("Committed STORE instruction for ROB ID: " + std::to_string(rob_id));
    }
  }
}

inline void LSB::tick() {
  // Move next_broadcast_result to broadcast_result
  broadcast_result = next_broadcast_result;
  next_broadcast_result = std::nullopt;

  if (lsb_entries.empty()) {
    busy = false;
    return;
  }

  LOG_DEBUG("Memory Unit Executing: " + std::to_string(lsb_entries.size()) +
            " entries in LSB");

  // Process entries in order, stopping at first non-executable instruction
    LSBEntry &entry = *lsb_entries.begin();
    uint32_t effective_address = entry.instruction.address + entry.instruction.imm;

    LOG_DEBUG(
        "Processing Instruction: rob_id=" +
        std::to_string(entry.instruction.rob_id) +
        ", effective_addr=" + std::to_string(effective_address) + ", op_type=" +
        (entry.instruction.op_type == LSBOpType::LOAD ? "LOAD" : "STORE") +
        ", committed=" + std::to_string(entry.committed) +
        ", executing=" + std::to_string(entry.executing) +
        ", can_execute=" + std::to_string(entry.instruction.can_execute));

    // If this instruction cannot execute and is not already executing, block all subsequent instructions
    if (!entry.instruction.can_execute && !entry.executing) {
      LOG_DEBUG("Instruction with ROB ID " + std::to_string(entry.instruction.rob_id) + 
                " cannot execute, blocking all subsequent instructions");
      return;
    }

    // Start executing if not already executing
    if (!entry.executing) {
      bool can_execute = ((entry.instruction.op_type == LSBOpType::LOAD) ||
                         (entry.instruction.op_type == LSBOpType::STORE &&
                          entry.committed)) && entry.instruction.can_execute;

      if (can_execute) {
        entry.executing = true;
        entry.cycles_remaining = 3; // Set latency for memory access
      }
    }

    // Continue execution if already started
    if (entry.executing) {
      entry.cycles_remaining--;

      if (entry.cycles_remaining == 0) {
        // Instruction finished, prepare result for broadcast
        MemoryResult result;
        result.rob_id = entry.instruction.rob_id;
        result.op_type = entry.instruction.op_type;

        if (entry.instruction.op_type == LSBOpType::LOAD) {
          result.data = memory.read(effective_address);
          result.dest_tag = entry.instruction.dest_tag;
        } else { // STORE
          memory.write(effective_address, entry.instruction.data);
          result.data = 0;     // Data is not broadcasted for stores
          result.dest_tag = 0; // No destination register for stores
        }

        next_broadcast_result = result;
        lsb_entries.erase(lsb_entries.begin()); // Remove completed instruction
        return; // Only process one instruction per tick
      }
    }
  

  busy = !lsb_entries.empty();
}

inline Memory& LSB::get_memory() {
  return memory;
}

inline void LSB::flush() {
  LOG_DEBUG("Flushing LSB - removing non-committed entries");
  
  // Remove only non-committed entries, keep committed ones that are executing
  auto it = lsb_entries.begin();
  while (it != lsb_entries.end()) {
    if (!it->committed) {
      it = lsb_entries.erase(it);
    } else {
      ++it;
    }
  }
  
  // Clear broadcast results only if no committed instructions remain
  if (lsb_entries.empty()) {
    broadcast_result = std::nullopt;
    next_broadcast_result = std::nullopt;
    busy = false;
  }
}
#endif // CORE_MEMORY_HPP