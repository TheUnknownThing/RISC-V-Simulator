#include <cstdio>
// #define LOGGING_LEVEL_INFO
// #define LOGGING_LEVEL_DEBUG
#define LOGGING_LEVEL_NONE
#include "core/cpu.hpp"
#include "utils/logger.hpp"
#include <iostream>

int main() {
  // freopen("../logs/simulator.log", "w", stderr);

  LOG_INFO("RISC-V Simulator starting...");

  CPU cpu();
  // CPU cpu("../testcases/hanoi.data");

  LOG_INFO("Starting CPU execution");
  int result = cpu.run();

  LOG_INFO("CPU execution completed with result: " + std::to_string(result));
  std::cout << (result & 0xFF) << std::endl;

  return EXIT_SUCCESS;
}