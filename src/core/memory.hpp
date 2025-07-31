#ifndef CORE_MEMORY_HPP
#define CORE_MEMORY_HPP

#include "utils/logger.hpp"
#include <cstdint>
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
  std::unordered_map<uint32_t, int32_t> memory_data;

public:
  int32_t read(uint32_t address) const;
  void write(uint32_t address, int32_t data);
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
};

// Memory implementation
inline int32_t Memory::read(uint32_t address) const {
  auto it = memory_data.find(address);
  return (it != memory_data.end()) ? it->second : 0;
}

inline void Memory::write(uint32_t address, int32_t data) {
  memory_data[address] = data;
}

// LSB implementation
inline LSB::LSB()
    : broadcast_result(std::nullopt), next_broadcast_result(std::nullopt),
      busy(false) {}

constexpr size_t LSB_SIZE = 32;

inline bool LSB::is_full() const { return lsb_entries.size() >= LSB_SIZE; }

inline void LSB::add_instruction(LSBInstruction instruction) {
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

  // Process the first entry that can execute
  for (auto it = lsb_entries.begin(); it != lsb_entries.end(); ++it) {
    LSBEntry &entry = *it;
    uint32_t effective_address = entry.instruction.address + entry.instruction.imm;

    LOG_DEBUG(
        "Processing Instruction: rob_id=" +
        std::to_string(entry.instruction.rob_id) +
        ", effective_addr=" + std::to_string(effective_address) + ", op_type=" +
        (entry.instruction.op_type == LSBOpType::LOAD ? "LOAD" : "STORE") +
        ", committed=" + std::to_string(entry.committed) +
        ", executing=" + std::to_string(entry.executing));

    // Start executing if:
    // 1. It's a LOAD (can execute during regular tick)
    // 2. It's a STORE that has been committed
    if (!entry.executing) {
      bool can_execute = (entry.instruction.op_type == LSBOpType::LOAD) ||
                         (entry.instruction.op_type == LSBOpType::STORE &&
                          entry.committed);

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
        lsb_entries.erase(it); // Remove completed instruction
        break; // Only process one instruction per tick
      }
    }
  }

  busy = !lsb_entries.empty();
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