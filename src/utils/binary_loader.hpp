#ifndef UTILS_BINARY_LOADER_HPP
#define UTILS_BINARY_LOADER_HPP

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include "logger.hpp"

class BinaryLoader {
public:
  /**
   * @brief Constructs a BinaryLoader and loads the specified file.
   * @param filename The path to the file to load.
   */
  BinaryLoader(const std::string &filename) { 
    LOG_INFO("Loading binary file: " + filename);
    loadFile(filename); 
  }


  /**
   * @brief Constructs a BinaryLoader that will load data from stdin.
   */
  BinaryLoader() { 
    LOG_INFO("BinaryLoader created for stdin input");
  }


  /**
   * @brief Fetches a 32-bit instruction from the loaded memory.
   * @param address The memory address of the instruction.
   * @return The 32-bit instruction word.
   * @throws std::out_of_range if the address or the subsequent 3 bytes are not
   * in memory.
   */
  uint32_t fetchInstruction(uint32_t address) const {
    std::stringstream ss;
    ss << "0x" << std::hex << address;
    LOG_DEBUG("Fetching instruction from memory address: " + ss.str() + " (decimal: " + std::to_string(address) + ")");
    try {
      uint32_t byte0 = memory.at(address);
      uint32_t byte1 = memory.at(address + 1);
      uint32_t byte2 = memory.at(address + 2);
      uint32_t byte3 = memory.at(address + 3);
      uint32_t instruction = byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24);
      LOG_DEBUG("Fetched instruction: 0x" + std::to_string(instruction));
      return instruction;
    } catch (const std::out_of_range &e) {
      LOG_ERROR("Memory access violation at address 0x" + std::to_string(address));
      std::cerr << "Memory access violation at address 0x" << std::hex
                << address << std::dec << std::endl;
      throw;
    }
  }

  /**
   * @brief Gets a reference to the loaded memory data.
   * @return Const reference to the memory map.
   */
  const std::map<uint32_t, uint8_t>& get_memory() const {
    return memory;
  }

  /**
   * @brief Loads binary data from stdin.
   */
  void load_from_stdin() {
    LOG_INFO("Loading binary data from stdin");
    loadFromStdin();
  }

private:
  std::map<uint32_t, uint8_t> memory;

  void loadFile(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
      LOG_ERROR("Could not open file: " + filename);
      throw std::runtime_error("Could not open file: " + filename);
    }

    LOG_DEBUG("File opened successfully, parsing contents...");
    std::string line;
    uint32_t current_address = 0;
    int lines_processed = 0;
    int bytes_loaded = 0;

    while (std::getline(file, line)) {
      lines_processed++;
      if (line.empty())
        break;

      if (line[0] == '@') {
        current_address = std::stoul(line.substr(1), nullptr, 16);
        LOG_DEBUG("Setting address to: 0x" + std::to_string(current_address));
      } else {
        std::stringstream ss(line);
        unsigned int byte_val;
        while (ss >> std::hex >> byte_val) {
          memory[current_address++] = static_cast<uint8_t>(byte_val);
          bytes_loaded++;
        }
      }
    }
    
    LOG_INFO("Binary file loaded successfully");
    LOG_DEBUG("Processed " + std::to_string(lines_processed) + " lines, loaded " + 
              std::to_string(bytes_loaded) + " bytes");
    LOG_DEBUG("Memory ranges from 0x0 to 0x" + std::to_string(current_address - 1));
  }


  void loadFromStdin() {
    LOG_DEBUG("Reading binary data from stdin...");
    std::string line;
    uint32_t current_address = 0;
    int lines_processed = 0;
    int bytes_loaded = 0;

    while (std::getline(std::cin, line)) {
      lines_processed++;
      if (line.empty())
        break;

      if (line[0] == '@') {
        current_address = std::stoul(line.substr(1), nullptr, 16);
        LOG_DEBUG("Setting address to: 0x" + std::to_string(current_address));
      } else {
        std::stringstream ss(line);
        unsigned int byte_val;
        while (ss >> std::hex >> byte_val) {
          memory[current_address++] = static_cast<uint8_t>(byte_val);
          bytes_loaded++;
        }
      }
    }
    
    LOG_INFO("Binary data loaded successfully from stdin");
    LOG_DEBUG("Processed " + std::to_string(lines_processed) + " lines, loaded " + 
              std::to_string(bytes_loaded) + " bytes");
    LOG_DEBUG("Memory ranges from 0x0 to 0x" + std::to_string(current_address - 1));
  }

};

#endif // UTILS_BINARY_LOADER_HPP