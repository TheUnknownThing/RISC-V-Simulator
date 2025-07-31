// dump.hpp
#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace norb {
inline std::string hex(uint32_t x) {
  std::ostringstream oss;
  oss << "0x" << std::setfill('0') << std::setw(8) << std::hex << x;
  return oss.str();
}

inline std::string bin(uint32_t x) {
  return "0b" + std::bitset<32>(x).to_string();
}

inline std::string dump_repr(uint32_t x) {
  return std::to_string(x) + " (" + hex(x) + ", " + bin(x) + ")";
}

inline std::string pad_with_zero(uint32_t x, int width = 4) {
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(width) << x;
  return oss.str();
}

template <size_t reg_count_, typename RegType_ = uint32_t>
class RegisterDumper {
private:
  std::ofstream file_;
  int line_number_;

public:
  RegisterDumper(const std::string &filename) : line_number_(0) {
    // Clear the file at bootup and keep it open
    file_.open(filename, std::ios::trunc);
    if (!file_.is_open()) {
      throw std::runtime_error("Failed to open file for register dumping: " +
                               filename);
    }
  }

  ~RegisterDumper() {
    if (file_.is_open()) {
      file_.close();
    }
  }

  void dump(uint32_t pc_at_commit,
            const std::array<RegType_, reg_count_> &reg_snapshot) {
    dump_impl(pc_at_commit, reg_snapshot);
  }

private:
  void dump_impl(uint32_t pc_at_commit,
                 const std::array<RegType_, reg_count_> &reg_snapshot) {
    std::ostringstream oss;
    oss << "[" << norb::pad_with_zero(++line_number_, 4)
        << "] ";                             // line number for each line
    oss << norb::hex(pc_at_commit) << " | "; // PC at commit
    for (size_t i = 0; i < reg_count_; ++i) {
      const auto reg_value = reg_snapshot[i];
      if (reg_value == 0)
        oss << "R" << i << "(" << 0 << ")";
      else
        oss << "R" << i << "(" << reg_value << "=" << norb::hex(reg_value)
            << ")";
      if (i < reg_count_ - 1)
        oss << " ";
    }
    oss << "\n";

    // Write to the persistent file stream
    file_ << oss.str();
    file_.flush(); // Ensure data is written immediately
  }
};
} // namespace norb
