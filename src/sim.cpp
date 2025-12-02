#include "sim.h"
#include "vcd.h"

// -----------------------------------------------------------------------------
// Signal Implementation
// -----------------------------------------------------------------------------
void Signal::connect(Primitive* p) {
    fanout.push_back(p);
}

// -----------------------------------------------------------------------------
// Simulator Implementation
// -----------------------------------------------------------------------------
void Simulator::initialize() {
    std::cout << "[Kernel] Power-On Initialization (Eval all gates)..." << std::endl;
    for (Primitive* p : primitives) p->eval();
}

void Simulator::schedule(uint64_t delay, uint32_t delta, Signal* sig, int val) {
    uint64_t trigger_time = current_time + delay;
    Event e = {trigger_time, delta, event_counter++, sig, val};
    event_queue.push(e);
}

void Simulator::run_until(uint64_t max_time) {
    std::cout << "[Kernel] Starting Simulation at Time " << current_time << std::endl;

    // Dump initial state (Time 0)
    if (vcd) vcd->writeHeader();

    while (!event_queue.empty()) {
        // 1. Peek at the next event
        Event e = event_queue.top();

        // STOP condition
        if (e.time > max_time) {
            std::cout << "[Kernel] Max time reached. Stopping." << std::endl;
            break;
        }

        event_queue.pop();

        // 2. Advance Time
        // Note: In a real sim, we would check for Delta Cycles here.
        // For Phase 1, we just jump simulation time.
        if (e.time < current_time) {
            std::cerr << "Error: Cannot travel back in time!" << std::endl;
            return;
        }
        current_time = e.time;

        // 3. Execute Event (Update the signal)
        // Optimization: Only trigger updates if the value actually changes
        if (e.target->value != e.new_value) {
            e.target->value = e.new_value;

            if (vcd) {
                vcd->recordChange(current_time, e.target, e.new_value);
            }
            
            // Debug print
            std::cout << "  [@" << current_time << "ns] Signal '" 
                      << e.target->name << "' updated to " << e.new_value << std::endl;

            // 4. Propagate: Tell everyone connected to this wire to "Evaluate"
            for (Primitive* p : e.target->fanout) p->eval();
        }
    }

    std::cout << "[Kernel] Simulation Finished at Time " << current_time << std::endl;
}
