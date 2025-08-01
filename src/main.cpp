#include <cstdio>
// #define LOGGING_LEVEL_INFO
// #define LOGGING_LEVEL_DEBUG
#define LOGGING_LEVEL_NONE
#include "core/cpu.hpp"
#include "utils/logger.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  std::cout.tie(nullptr);

  LOG_INFO("RISC-V Simulator starting...");

  int result;
  if (argc > 1) {
    CPU cpu(argv[1]);
    LOG_INFO("Starting CPU execution");
    result = cpu.run();
  } else {
    // use stdin
    CPU cpu;
    LOG_INFO("Starting CPU execution");
    result = cpu.run();
  }

  LOG_INFO("CPU execution completed with result: " + std::to_string(result));
  std::cout << (result & 0xFF) << std::endl;

  return EXIT_SUCCESS;
}