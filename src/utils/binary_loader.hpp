#ifndef UTILS_BINARY_LOADER_HPP
#define UTILS_BINARY_LOADER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <iomanip>

class BinaryLoader {
public:
    /**
     * @brief Constructs a BinaryLoader and loads the specified file.
     * @param filename The path to the file to load.
     */
    BinaryLoader(const std::string& filename) {
        loadFile(filename);
    }

    /**
     * @brief Fetches a 32-bit instruction from the loaded memory.
     * @param address The memory address of the instruction.
     * @return The 32-bit instruction word.
     * @throws std::out_of_range if the address or the subsequent 3 bytes are not in memory.
     */
    uint32_t fetchInstruction(uint32_t address) const {
        try {
            uint32_t byte0 = memory.at(address);
            uint32_t byte1 = memory.at(address + 1);
            uint32_t byte2 = memory.at(address + 2);
            uint32_t byte3 = memory.at(address + 3);
            return byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24);
        } catch (const std::out_of_range& e) {
            std::cerr << "Memory access violation at address 0x" << std::hex << address << std::dec << std::endl;
            throw;
        }
    }

private:
    std::map<uint32_t, uint8_t> memory;

    void loadFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + filename);
        }

        std::string line;
        uint32_t current_address = 0;

        while (std::getline(file, line)) {
            if (line.empty()) continue;

            if (line[0] == '@') {
                current_address = std::stoul(line.substr(1), nullptr, 16);
            } else {
                std::stringstream ss(line);
                unsigned int byte_val;
                while (ss >> std::hex >> byte_val) {
                    memory[current_address++] = static_cast<uint8_t>(byte_val);
                }
            }
        }
    }
};

#endif // UTILS_BINARY_LOADER_HPP