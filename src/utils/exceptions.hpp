#ifndef UTILS_EXCEPTIONS_HPP
#define UTILS_EXCEPTIONS_HPP

#include <string>

/**
 * @brief Exception thrown when the simulator should terminate normally
 * This is thrown when the termination instruction (li a0, 255) is committed
 */
class ProgramTerminationException : public std::exception {
private:
  std::string message;
  int exit_code;

public:
  ProgramTerminationException(int code) : exit_code(code) {
    message = "Program terminated with exit code: " + std::to_string(code);
  }

  const char *what() const noexcept override { return message.c_str(); }

  int get_exit_code() const { return exit_code; }
};

#endif // UTILS_EXCEPTIONS_HPP