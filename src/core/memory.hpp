#ifndef CORE_MEMORY_HPP
#define CORE_MEMORY_HPP

#include <cstdint>
#include <optional>
#include <queue>
#include <stdexcept>
#include <unordered_map>

enum class LSBOpType { LOAD, STORE };

struct LSBInstruction {
  LSBOpType op_type;
  uint32_t address;
  int32_t data;
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
  std::queue<LSBEntry> lsb_queue;
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

private:
  bool can_execute_load(const LSBEntry &entry) const;
  bool has_dependency(uint32_t address, uint32_t rob_id) const;
  void execute_head();
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

constexpr size_t LSB_SIZE = 16;

inline bool LSB::is_full() const { return lsb_queue.size() >= LSB_SIZE; }

inline void LSB::add_instruction(LSBInstruction instruction) {
  if (is_full()) {
    throw std::runtime_error("LSB is full");
  }
  lsb_queue.emplace(instruction);
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
  std::queue<LSBEntry> temp_queue;
  bool found = false;
  while (!lsb_queue.empty()) {
    LSBEntry entry = lsb_queue.front();
    lsb_queue.pop();

    if (!found && entry.instruction.rob_id == rob_id &&
        entry.instruction.op_type == LSBOpType::STORE) {
      entry.committed = true;
      found = true;
    }
    temp_queue.push(entry);
  }
  lsb_queue = temp_queue;
}

inline bool LSB::has_dependency(uint32_t address, uint32_t rob_id) const {
  std::queue<LSBEntry> temp_queue = lsb_queue;
  while (!temp_queue.empty()) {
    const LSBEntry &entry = temp_queue.front();
    if (entry.instruction.rob_id < rob_id &&
        entry.instruction.op_type == LSBOpType::STORE &&
        entry.instruction.address == address && !entry.committed) {
      return true;
    }
    if (entry.instruction.rob_id >= rob_id) {
      break;
    }
    temp_queue.pop();
  }
  return false;
}

inline bool LSB::can_execute_load(const LSBEntry &entry) const {
  if (entry.instruction.op_type != LSBOpType::LOAD) {
    return false;
  }
  return !has_dependency(entry.instruction.address, entry.instruction.rob_id);
}

inline void LSB::execute_head() {
  if (lsb_queue.empty()) {
    return;
  }
  LSBEntry &head = lsb_queue.front();
  if (head.executing) {
    return;
  }

  bool can_execute = false;
  if (head.instruction.op_type == LSBOpType::LOAD) {
    can_execute = can_execute_load(head);
  } else { 
    can_execute = head.committed;
  }

  if (can_execute) {
    head.executing = true;
    head.cycles_remaining = 3;
  }
}

inline void LSB::tick() {
  broadcast_result = next_broadcast_result;
  next_broadcast_result = std::nullopt;

  execute_head();

  if (!lsb_queue.empty()) {
    std::queue<LSBEntry> temp_queue;
    while (!lsb_queue.empty()) {
      LSBEntry entry = lsb_queue.front();
      lsb_queue.pop();

      if (entry.executing && entry.cycles_remaining > 0) {
        entry.cycles_remaining--;

        if (entry.cycles_remaining == 0) {
          MemoryResult result;
          result.rob_id = entry.instruction.rob_id;
          result.op_type = entry.instruction.op_type;

          if (entry.instruction.op_type == LSBOpType::LOAD) {
            result.data = memory.read(entry.instruction.address);
            result.dest_tag = entry.instruction.dest_tag;
            next_broadcast_result = result;
          } else { // STORE
            memory.write(entry.instruction.address, entry.instruction.data);
            result.data = 0; // No data to broadcast
            result.dest_tag = 0; // No destination register
            next_broadcast_result = result;
          }
          continue;
        }
      }
      temp_queue.push(entry);
    }
    lsb_queue = temp_queue;
  }
  busy = !lsb_queue.empty();
}

#endif // CORE_MEMORY_HPP