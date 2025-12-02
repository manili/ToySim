#ifndef VCD_H
#define VCD_H

#include <string>
#include <fstream>
#include <map>
#include <vector>
#include "sim.h"

class VcdWriter {
private:
    std::ofstream file;
    std::map<Signal*, std::string> signal_to_id;
    int next_id_code; // Integer we convert to ASCII
    uint64_t last_time = 0; 

    // Helper to generate IDs like !, ", #, ... a, b, ...
    std::string generateCode(int id);

public:
    VcdWriter(const std::string& filename);
    ~VcdWriter();

    // 1. Register a signal before simulation starts
    void registerSignal(Signal* s);

    // 2. Write the header (Timescale, Definitions)
    void writeHeader();

    // 3. Record a value change (Called by Kernel)
    void recordChange(uint64_t time, Signal* s, int value);
};

#endif