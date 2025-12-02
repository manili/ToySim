#include "vcd.h"
#include <iostream>

VcdWriter::VcdWriter(const std::string& filename) : next_id_code(33) { // Start ASCII at 33 (!)
    file.open(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open VCD file " << filename << std::endl;
    }
}

VcdWriter::~VcdWriter() {
    if (file.is_open()) file.close();
}

std::string VcdWriter::generateCode(int id) {
    // Simple 1-char generation for ToySim. 
    // In a real sim, you'd handle multi-char IDs (aa, ab, etc) once you pass ASCII 126.
    char c = (char)id;
    return std::string(1, c);
}

void VcdWriter::registerSignal(Signal* s) {
    std::string code = generateCode(next_id_code++);
    signal_to_id[s] = code;
}

void VcdWriter::writeHeader() {
    file << "$date Today $end" << std::endl;
    file << "$version ToySim v1.0 $end" << std::endl;
    file << "$timescale 1ns $end" << std::endl;
    file << "$scope module top $end" << std::endl;

    // Define variables: $var wire 1 ! a $end
    for (auto const& [sig, id] : signal_to_id) {
        file << "$var wire 1 " << id << " " << sig->name << " $end" << std::endl;
    }

    file << "$upscope $end" << std::endl;
    file << "$enddefinitions $end" << std::endl;
    file << "$dumpvars" << std::endl;

    // Dump initial values
    for (auto const& [sig, id] : signal_to_id) {
        file << sig->value << id << std::endl;
    }
    file << "$end" << std::endl;
}

void VcdWriter::recordChange(uint64_t time, Signal* s, int value) {
    if (signal_to_id.find(s) != signal_to_id.end()) {
        
        // If time advanced, print the timestamp line
        if (time > last_time) {
            file << "#" << time << std::endl;
            last_time = time;
        }

        // Write value change
        file << value << signal_to_id[s] << std::endl;
    }
}