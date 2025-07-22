#ifndef TOMASULO_RESERVATION_STATION_HPP
#define TOMASULO_RESERVATION_STATION_HPP

#include <cstdint>
#include "../riscv/instruction.hpp"

class ReservationStationEntry {
    public:
        bool busy;
        riscv::DecodedInstruction op;
        double vj, vk;
        int qj, qk; // Tags, e.g., ROB entry indices
        int dest_tag;
        int cycles_remaining;
    };

#endif // TOMASULO_RESERVATION_STATION_HPP