#include <iostream>
#include <vector>
#include "sim.h"
#include "builder.h"
#include "vcd.h"

int main(int argc, char** argv) {
    Simulator kernel;
    GraphBuilder builder(&kernel);

    if (!builder.loadDesign("sync_half_adder.v", "sync_half_adder")) return 1;

    // --- VCD SETUP ---
    VcdWriter vcd("sync_half_adder_waves.vcd");
    kernel.attachVcd(&vcd);
    for (auto const& [name, sig] : builder.getSignals()) vcd.registerSignal(sig);

    // --- SIGNAL LOOKUP ---
    Signal* clk = builder.getSignal("clk");
    Signal* rst = builder.getSignal("rst");
    
    // FETCH INDIVIDUAL BITS
    Signal* a     = builder.getSignal("a");
    Signal* b     = builder.getSignal("b");
    Signal* sum   = builder.getSignal("sum");
    Signal* carry = builder.getSignal("carry");

    // Check if we found them
    if (!clk || !rst) {
        std::cerr << "Error: Clock or Reset not found." << std::endl;
        return 1;
    }
    if (!a || !b || !sum || !carry) {
        std::cerr << "Error: Signals not found. (Looking for 'a', 'b', 'sum', 'carry')" << std::endl;
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
    // This ensures all wires calculate their correct values 
    // based on rst=1 before the clock starts ticking.
    kernel.initialize();

    // Start Clock
    Clock sys_clk(&kernel, 10, clk);

    // T=15: Release Reset
    kernel.schedule(25, 0, rst, 0);
    
    // T=30: Release Reset
    kernel.schedule(30, 0, a, 0);
    kernel.schedule(30, 0, b, 0);
    
    // T=40: Release Reset
    kernel.schedule(40, 0, a, 1);
    kernel.schedule(40, 0, b, 0);
    
    // T=50: Release Reset
    kernel.schedule(50, 0, a, 0);
    kernel.schedule(50, 0, b, 1);
    
    // T=60: Release Reset
    kernel.schedule(60, 0, a, 1);
    kernel.schedule(60, 0, b, 1);

    // Run long enough to see counter wrap around
    kernel.run_until(70);
    
    std::cout << "Done. Open sync_half_adder_waves.vcd" << std::endl;
    return 0;
}
