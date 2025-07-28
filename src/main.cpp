#define LOGGING_LEVEL_DEBUG

#include <iostream>
#include "core/cpu.hpp"
#include "utils/logger.hpp"

int main() {
  LOG_INFO("RISC-V Simulator starting...");
  
  try {
    LOG_INFO("Initializing CPU with testcase: ../testcases/array_test1.data");
    CPU cpu("../testcases/array_test1.data");
    
    LOG_INFO("Starting CPU execution");
    int result = cpu.run();
    
    LOG_INFO("CPU execution completed with result: " + std::to_string(result));
    std::cout << result << std::endl;
    
  } catch (const std::exception &e) {
    LOG_ERROR("Execution failed: " + std::string(e.what()));
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  
  LOG_INFO("RISC-V Simulator terminated successfully");
  return EXIT_SUCCESS;
}