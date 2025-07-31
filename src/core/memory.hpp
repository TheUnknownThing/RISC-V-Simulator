#ifndef CORE_MEMORY_HPP
#define CORE_MEMORY_HPP

#include "utils/logger.hpp"
#include "riscv/instruction.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_map>

struct LSBInstruction {
  std::variant<riscv::I_LoadOp, riscv::S_StoreOp> op_type;
  int32_t address;
  int32_t data;
  int32_t imm; // Immediate offset for address calculation
  uint32_t dest_tag;
  uint32_t rob_id;
  bool can_execute;
  
  bool is_load() const {
    return std::holds_alternative<riscv::I_LoadOp>(op_type);
  }
  
  bool is_store() const {
    return std::holds_alternative<riscv::S_StoreOp>(op_type);
  }
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
  std::variant<riscv::I_LoadOp, riscv::S_StoreOp> op_type;
  bool is_load() const {
    return std::holds_alternative<riscv::I_LoadOp>(op_type);
  }
  
  bool is_store() const {
    return std::holds_alternative<riscv::S_StoreOp>(op_type);
  }
};

class Memory {
  std::unordered_map<uint32_t, uint8_t> memory_data;

public:
  int32_t read(uint32_t address) const;
  int16_t read_halfword(uint32_t address) const;
  int8_t read_byte_signed(uint32_t address) const;
  uint16_t read_halfword_unsigned(uint32_t address) const;
  uint8_t read_byte_unsigned(uint32_t address) const;
  
  void write(uint32_t address, int32_t data);
  void write_halfword(uint32_t address, int16_t data);
  void write_byte(uint32_t address, uint8_t data);
  
  int32_t load(uint32_t address, riscv::I_LoadOp op) const;
  void store(uint32_t address, int32_t data, riscv::S_StoreOp op);
  
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
  uint8_t byte0 = read_byte_unsigned(address);
  uint8_t byte1 = read_byte_unsigned(address + 1);
  uint8_t byte2 = read_byte_unsigned(address + 2);
  uint8_t byte3 = read_byte_unsigned(address + 3);
  return static_cast<int32_t>(byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24));
}

inline int16_t Memory::read_halfword(uint32_t address) const {
  uint8_t byte0 = read_byte_unsigned(address);
  uint8_t byte1 = read_byte_unsigned(address + 1);
  return static_cast<int16_t>(byte0 | (byte1 << 8));
}

inline int8_t Memory::read_byte_signed(uint32_t address) const {
  return static_cast<int8_t>(read_byte_unsigned(address));
}

inline uint16_t Memory::read_halfword_unsigned(uint32_t address) const {
  uint8_t byte0 = read_byte_unsigned(address);
  uint8_t byte1 = read_byte_unsigned(address + 1);
  return static_cast<uint16_t>(byte0 | (byte1 << 8));
}

inline uint8_t Memory::read_byte_unsigned(uint32_t address) const {
  auto it = memory_data.find(address);
  return (it != memory_data.end()) ? it->second : 0;
}

inline void Memory::write(uint32_t address, int32_t data) {
  write_byte(address, static_cast<uint8_t>(data & 0xFF));
  write_byte(address + 1, static_cast<uint8_t>((data >> 8) & 0xFF));
  write_byte(address + 2, static_cast<uint8_t>((data >> 16) & 0xFF));
  write_byte(address + 3, static_cast<uint8_t>((data >> 24) & 0xFF));
}

inline void Memory::write_halfword(uint32_t address, int16_t data) {
  write_byte(address, static_cast<uint8_t>(data & 0xFF));
  write_byte(address + 1, static_cast<uint8_t>((data >> 8) & 0xFF));
}

inline void Memory::write_byte(uint32_t address, uint8_t data) {
  memory_data[address] = data;
}

inline int32_t Memory::load(uint32_t address, riscv::I_LoadOp op) const {
  switch (op) {
    case riscv::I_LoadOp::LB:
      return static_cast<int32_t>(read_byte_signed(address));
    case riscv::I_LoadOp::LH:
      return static_cast<int32_t>(read_halfword(address));
    case riscv::I_LoadOp::LW:
      return read(address);
    case riscv::I_LoadOp::LBU:
      return static_cast<int32_t>(read_byte_unsigned(address));
    case riscv::I_LoadOp::LHU:
      return static_cast<int32_t>(read_halfword_unsigned(address));
    default:
      throw std::runtime_error("Invalid load operation");
  }
}

inline void Memory::store(uint32_t address, int32_t data, riscv::S_StoreOp op) {
  switch (op) {
    case riscv::S_StoreOp::SB:
      write_byte(address, static_cast<uint8_t>(data & 0xFF));
      break;
    case riscv::S_StoreOp::SH:
      write_halfword(address, static_cast<int16_t>(data & 0xFFFF));
      break;
    case riscv::S_StoreOp::SW:
      write(address, data);
      break;
    default:
      throw std::runtime_error("Invalid store operation");
  }
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
  auto existing_it = std::find_if(
      lsb_entries.begin(), lsb_entries.end(),
      [&instruction](const LSBEntry &entry) {
        return entry.instruction.rob_id == instruction.rob_id;
      });
  
  if (existing_it != lsb_entries.end()) {
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
  
  if (is_full()) {
    throw std::runtime_error("LSB is full");
  }
  
  LSBEntry new_entry(instruction);
  
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
  broadcast_result = next_broadcast_result;
  next_broadcast_result = std::nullopt;

  if (lsb_entries.empty()) {
    busy = false;
    return;
  }

  LOG_DEBUG("Memory Unit Executing: " + std::to_string(lsb_entries.size()) +
            " entries in LSB");

  LSBEntry &entry = *lsb_entries.begin();
  uint32_t effective_address = entry.instruction.address + entry.instruction.imm;

  LOG_DEBUG(
      "Processing Instruction: rob_id=" +
      std::to_string(entry.instruction.rob_id) +
      ", effective_addr=" + std::to_string(effective_address) + ", op_type=" +
      (entry.instruction.is_load() ? "LOAD" : "STORE") +
      ", committed=" + std::to_string(entry.committed) +
      ", executing=" + std::to_string(entry.executing) +
      ", can_execute=" + std::to_string(entry.instruction.can_execute));

  if (!entry.instruction.can_execute && !entry.executing) {
    LOG_DEBUG("Instruction with ROB ID " + std::to_string(entry.instruction.rob_id) + 
              " cannot execute, blocking all subsequent instructions");
    busy = !lsb_entries.empty();
    return;
  }

  if (!entry.executing) {
    bool can_execute = (entry.instruction.is_load() ||
                       (entry.instruction.is_store() && entry.committed)) && 
                       entry.instruction.can_execute;

    if (can_execute) {
      entry.executing = true;
      entry.cycles_remaining = 3;
    }
  }

  if (entry.executing) {
    entry.cycles_remaining--;

    if (entry.cycles_remaining == 0) {
      MemoryResult result;
      result.rob_id = entry.instruction.rob_id;
      result.op_type = entry.instruction.op_type;

      if (entry.instruction.is_load()) {
        auto load_op = std::get<riscv::I_LoadOp>(entry.instruction.op_type);
        result.data = memory.load(effective_address, load_op);
        result.dest_tag = entry.instruction.dest_tag;
      } else {
        auto store_op = std::get<riscv::S_StoreOp>(entry.instruction.op_type);
        memory.store(effective_address, entry.instruction.data, store_op);
        result.data = 0;
        result.dest_tag = 0;
      }

      next_broadcast_result = result;
      lsb_entries.erase(lsb_entries.begin());
    }
  }
  busy = !lsb_entries.empty();
}

inline Memory& LSB::get_memory() {
  return memory;
}

inline void LSB::flush() {
  LOG_DEBUG("Flushing LSB - removing non-committed entries");
  
  auto it = lsb_entries.begin();
  while (it != lsb_entries.end()) {
    if (!it->committed) {
      it = lsb_entries.erase(it);
    } else {
      ++it;
    }
  }
  
  if (lsb_entries.empty()) {
    broadcast_result = std::nullopt;
    next_broadcast_result = std::nullopt;
    busy = false;
  }
}
#endif // CORE_MEMORY_HPP