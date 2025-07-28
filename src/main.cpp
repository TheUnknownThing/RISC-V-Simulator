#define LOGGING_LEVEL_DEBUG

#include <iostream>
#include "core/cpu.hpp"
#include "utils/logger.hpp"

int main() {
  LOG_INFO("RISC-V Simulator starting...");
  
  try {
    CPU cpu("../testcases/naive.data");
    
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