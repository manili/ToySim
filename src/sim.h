#ifndef SIM_H
#define SIM_H

#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <functional>

class Simulator;
class Primitive;
class VcdWriter;

// Represents a Wire
class Signal {
public:
    std::string name;
    int value;
    std::vector<Primitive*> fanout;

    Signal(std::string n) : name(n), value(0) {}
    void connect(Primitive* p);
};

// Represents an Action at a specific time
struct Event {
    uint64_t time;
    uint32_t delta;
    uint64_t sequence_id;
    Signal* target;
    int new_value;
    
    // Sort Order:
    // 1. Time (Smallest first)
    // 2. Delta (Smallest first)
    // 3. Sequence (Smallest first) -> This ensures FIFO
    bool operator>(const Event& other) const {
        if (time != other.time)
            return time > other.time;
        if (delta != other.delta)
            return delta > other.delta;
        return sequence_id > other.sequence_id;
    }
};

// Base class for logic gates
class Primitive {
public:
    virtual void eval() = 0;
};

// The Engine
class Simulator {
private:
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> event_queue;
    VcdWriter* vcd = nullptr;
    std::vector<Primitive*> primitives;

    uint64_t event_counter = 0;
public:
    uint64_t current_time;
    Simulator() : current_time(0) {}
    void initialize();
    void attachVcd(VcdWriter* writer) { vcd = writer; }
    void schedule(uint64_t delay, uint32_t delta, Signal* sig, int val);
    void run_until(uint64_t max_time);
    void registerPrimitive(Primitive* p) { primitives.push_back(p); }
};

#endif
