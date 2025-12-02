#include <iostream>
#include <vector>
#include "sim.h"
#include "builder.h"
#include "vcd.h"

int main(int argc, char** argv) {
    Simulator kernel;
    GraphBuilder builder(&kernel);

    if (!builder.loadDesign("synth_counter.v", "counter")) return 1;

    // --- VCD SETUP ---
    VcdWriter vcd("counter_waves.vcd");
    kernel.attachVcd(&vcd);
    for (auto const& [name, sig] : builder.getSignals()) vcd.registerSignal(sig);

    // --- SIGNAL LOOKUP ---
    Signal* clk = builder.getSignal("clk");
    Signal* rst = builder.getSignal("rst");
    
    // FETCH INDIVIDUAL BITS
    // Because Yosys flattened the array, we must look for "count[0]", "count[1]"...
    Signal* c0  = builder.getSignal("count_0");
    Signal* c1  = builder.getSignal("count_1");
    Signal* c2  = builder.getSignal("count_2");
    Signal* c3  = builder.getSignal("count_3");

    // Check if we found them
    if (!clk || !rst) {
        std::cerr << "Error: Clock or Reset not found." << std::endl;
        return 1;
    }
    if (!c0 || !c1 || !c2 || !c3) {
        std::cerr << "Error: Count bits not found. (Looking for 'count_0' etc)" << std::endl;
        return 1;
    }

    std::cout << "--- Counter Simulation ---" << std::endl;
    // Pre-start Setup
    clk->value = 0;
    rst->value = 1;

    // T=0: Init
    kernel.schedule(0, 0, clk, 0);
    kernel.schedule(0, 0, rst, 1); // Hold Reset

    // Force Logic Propagation at T=0
    // This ensures all _00_ wires calculate their correct values 
    // based on rst=1 before the clock starts ticking.
    kernel.initialize();

    // Start Clock
    Clock sys_clk(&kernel, 10, clk);

    // T=15: Release Reset
    kernel.schedule(25, 0, rst, 0);

    // Run long enough to see counter wrap around
    kernel.run_until(345);
    
    std::cout << "Done. Open counter_waves.vcd" << std::endl;
    return 0;
}
