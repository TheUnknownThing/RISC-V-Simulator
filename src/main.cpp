#include <iostream>
#include "core/cpu.hpp"

int main() {
  try {
    CPU cpu("testcases/array_test1.data");
    std::cout << cpu.run() << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}