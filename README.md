# RISC-V Simulator with Tomasulo Algorithm

A toy RISC-V processor simulator implementing the Tomasulo algorithm for out-of-order execution.


## Project Structure

```
src/
├── core/           # Core CPU components
├── tomasulo/       # Tomasulo algorithm implementation
├── riscv/          # RISC-V ISA specific code
├── utils/          # Utility functions and helpers
└── main.cpp        # Application entry point
```

## Building

### Prerequisites

- CMake 3.16+
- C++17 compatible compiler (GCC 7+, Clang 6+)

### Build Commands

```bash
# Debug build
mkdir build && cd build
cmake ..
make

# Release build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
