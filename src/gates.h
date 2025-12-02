#ifndef GATES_H
#define GATES_H

#include "sim.h"

// -----------------------------------------------------------------------------
// Two-Input AND Gate
// -----------------------------------------------------------------------------
class AndGate : public Primitive {
    Simulator* sim;
    uint64_t delay;
    Signal *in1, *in2, *out;

public:
    AndGate(Simulator* s, uint64_t d, Signal* i1, Signal* i2, Signal* o)
        : sim(s), delay(d), in1(i1), in2(i2), out(o) {
        // Register sensitivity: Wake up if either input changes
        in1->connect(this);
        in2->connect(this);

        sim->registerPrimitive(this);
    }

    void eval() override {
        // 1. Calculate the logic
        int result = (in1->value & in2->value) & 0x1; // Mask to 1 bit
        
        // 2. Schedule the output update in the future
        // Note: We don't change 'out' immediately. We tell the kernel to do it later.
        sim->schedule(delay, 0, out, result);
    }
};

// -----------------------------------------------------------------------------
// Two-Input OR Gate
// -----------------------------------------------------------------------------
class OrGate : public Primitive {
    Simulator* sim;
    uint64_t delay;
    Signal *in1, *in2, *out;

public:
    OrGate(Simulator* s, uint64_t d, Signal* i1, Signal* i2, Signal* o)
        : sim(s), delay(d), in1(i1), in2(i2), out(o) {
        in1->connect(this);
        in2->connect(this);

        sim->registerPrimitive(this);
    }

    void eval() override {
        int result = (in1->value | in2->value) & 0x1;
        sim->schedule(delay, 0, out, result);
    }
};

// -----------------------------------------------------------------------------
// Inverter (NOT Gate)
// -----------------------------------------------------------------------------
class Inverter : public Primitive {
    Simulator* sim;
    uint64_t delay;
    Signal *in, *out;

public:
    Inverter(Simulator* s, uint64_t d, Signal* i, Signal* o)
        : sim(s), delay(d), in(i), out(o) {
        in->connect(this);

        sim->registerPrimitive(this);
    }

    void eval() override {
        int result = (!in->value) & 0x1;
        sim->schedule(delay, 0, out, result);
    }
};

// -----------------------------------------------------------------------------
// Two-Input XOR Gate
// -----------------------------------------------------------------------------
class XorGate : public Primitive {
    Simulator* sim;
    uint64_t delay;
    Signal *in1, *in2, *out;

public:
    XorGate(Simulator* s, uint64_t d, Signal* i1, Signal* i2, Signal* o)
        : sim(s), delay(d), in1(i1), in2(i2), out(o) {
        in1->connect(this);
        in2->connect(this);

        sim->registerPrimitive(this);
    }

    void eval() override {
        int result = (in1->value ^ in2->value) & 0x1;
        sim->schedule(delay, 0, out, result);
    }
};

// -----------------------------------------------------------------------------
// D Flip-Flop
// -----------------------------------------------------------------------------
class Dff : public Primitive {
    Simulator* sim;
    uint64_t delay;
    Signal *clk, *d, *q;
    int last_clk; // To detect edges

public:
    Dff(Simulator* s, uint64_t prop_delay, Signal* _clk, Signal* _d, Signal* _q)
        : sim(s), delay(prop_delay), clk(_clk), d(_d), q(_q), last_clk(0) {
        // Only wake up on Clock edges!
        clk->connect(this);
        // Initialize last_clk to whatever the signal is currently
        last_clk = clk->value;

        sim->registerPrimitive(this);
    }

    void eval() override {
        int current_clk = clk->value;

        // Detect Posedge: Was 0, Now 1
        if (last_clk == 0 && current_clk == 1) {
            // NBA: Update Q at the SAME simulation time, but Next DELTA step.
            // This ensures all other DFFs read the current 'q' before we change it.
            sim->schedule(delay, 1, q, d->value);
        }

        // Update history
        last_clk = current_clk;
    }
};

// -----------------------------------------------------------------------------
// D Flip-Flop with Asynchronous Reset (Active High)
// -----------------------------------------------------------------------------
class DffAsync : public Primitive {
    Simulator* sim;
    uint64_t delay;
    Signal *clk, *rst, *d, *q;
    int last_clk; 

public:
    DffAsync(Simulator* s, uint64_t prop_delay, Signal* _clk, Signal* _rst, Signal* _d, Signal* _q)
        : sim(s), delay(prop_delay), clk(_clk), rst(_rst), d(_d), q(_q), last_clk(0) {
        
        // Sensitivity: Must wake up on Clock OR Reset changes
        clk->connect(this);
        rst->connect(this);
        
        last_clk = clk->value;

        sim->registerPrimitive(this);
    }

    void eval() override {
        int current_clk = clk->value;
        int current_rst = rst->value;

        // Async Reset happens immediately (Logic behavior) -> Delta 0
        if (rst->value == 1) {
             sim->schedule(delay, 0, q, 0);
        }
        // Clocked Update happens later (Register behavior) -> Delta 1
        else if (last_clk == 0 && clk->value == 1) {
             sim->schedule(delay, 1, q, d->value);
        }

        last_clk = current_clk;
    }
};

// -----------------------------------------------------------------------------
// Clock Generator (Behavioral Model)
// -----------------------------------------------------------------------------
class Clock : public Primitive {
    Simulator* sim;
    uint64_t half_period;
    Signal* clk_out;

public:
    Clock(Simulator* s, uint64_t period, Signal* out) 
        : sim(s), half_period(period / 2), clk_out(out) {
            // Kickoff: Use Delta 0
            sim->schedule(0, 0, clk_out, 0); 
            sim->schedule(half_period, 0, clk_out, 1);
            
            clk_out->connect(this);
            
            // Register myself (good practice since we added this system earlier)
            sim->registerPrimitive(this);
    }

    void eval() override {
        int next_val = !clk_out->value;
        
        // Schedule NEXT toggle: Delay = half_period, Delta = 0
        sim->schedule(half_period, 0, clk_out, next_val);
    }
};

#endif
