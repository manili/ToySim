#ifndef BUILDER_H
#define BUILDER_H

#include <map>
#include <string>
#include <iostream>

// ToySim Includes
#include "sim.h"
#include "gates.h"

// Slang Includes
#include "slang/driver/Driver.h"
#include "slang/ast/Compilation.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/ast/symbols/MemberSymbols.h"
#include "slang/ast/ASTVisitor.h"

using namespace slang;
using namespace slang::driver;
using namespace slang::ast;

class GraphBuilder {
    Simulator* sim;
    // The Symbol Table: Maps "wire_name" -> Signal*
    std::map<std::string, Signal*> net_map;

public:
    GraphBuilder(Simulator* s) : sim(s) {}

    // Main entry point
    bool loadDesign(const std::string& filename, const std::string& top_module_name);
    
    Signal* getSignal(const std::string& name) {
        if (net_map.find(name) != net_map.end()) {
            return net_map[name];
        }
        return nullptr;
    }

    const std::map<std::string, Signal*>& getSignals() { return net_map; }

private:
    // Helpers
    Signal* getOrCreateSignal(std::string name);
    void createGate(const PrimitiveInstanceSymbol& gate);
    void createInstance(const InstanceSymbol& inst);
};

#endif
