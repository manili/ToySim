#include "builder.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/ast/expressions/AssignmentExpressions.h"

/* 
 * -----------------------------------------------------------------------------
 * Function: unwrapSignalName
 * -----------------------------------------------------------------------------
 * Purpose:
 *   Traverses the Slang Abstract Syntax Tree (AST) recursively to find the 
 *   underlying string name of a wire or signal.
 *
 * Why this is necessary:
 *   Slang represents Verilog connections as complex Expressions, not just simple strings.
 *   A connection to a gate might be wrapped in:
 *     1. Implicit Type Conversions (e.g., converting a 4-state logic type to a primitive input).
 *     2. Assignment Expressions (e.g., an output port is effectively an assignment to a wire).
 *   
 *   We must peel back these layers to find the 'NamedValue', which holds the actual
 *   symbol name (e.g., "clk", "reset", "sum").
 *
 * Returns:
 *   The string name of the signal if found, otherwise an empty string.
 */
std::string unwrapSignalName(const Expression* expr) {
    if (!expr) return "";

    // Base Case: The expression is a direct reference to a named signal (e.g., "a").
    // This is the leaf node we are looking for.
    if (expr->kind == ExpressionKind::NamedValue) {
        const NamedValueExpression& val = expr->as<NamedValueExpression>();
        return std::string(val.symbol.name);
    }

    // Indexing Case: Array Index (e.g., "count[0]")
    // Yosys accesses individual bits of wires.
    if (expr->kind == ExpressionKind::ElementSelect) {
        const ElementSelectExpression& sel = expr->as<ElementSelectExpression>();
        
        // Recursively get the name of the array (e.g., "count")
        std::string base = unwrapSignalName(&sel.value());
        
        // Get the index value
        // Note: In a full sim, we'd evaluate the constant. 
        // For Yosys output, it's usually a simple integer literal.
        if (sel.selector().kind == ExpressionKind::IntegerLiteral) {
            const IntegerLiteral& idx = sel.selector().as<IntegerLiteral>();
            // Construct the unique name "count[0]"
            return base + "_" + idx.getValue().toString();
        }
    }
    
    // Recursive Case 1: Implicit Conversions.
    // Slang inserts these when the port type doesn't exactly match the signal type.
    // We dig deeper into the 'operand' being converted.
    if (expr->kind == ExpressionKind::Conversion) {
        const ConversionExpression& conv = expr->as<ConversionExpression>();
        return unwrapSignalName(&conv.operand());
    }

    // Recursive Case 2: Assignment Expressions.
    // This typically occurs on the OUTPUT port of a primitive.
    // In the AST, 'and g1(out, in)' is represented as the primitive assigning a value to 'out'.
    // The wire being driven is on the Left-Hand Side (LHS) of the assignment.
    if (expr->kind == ExpressionKind::Assignment) {
        const AssignmentExpression& assign = expr->as<AssignmentExpression>();
        return unwrapSignalName(&assign.left());
    }

    // If the expression is too complex (e.g., concatenations {a,b} or math a+b),
    // we currently return empty to indicate it is unsupported in this ToySim.
    return ""; 
}

/* 
 * -----------------------------------------------------------------------------
 * Function: getOrCreateSignal
 * -----------------------------------------------------------------------------
 * Purpose:
 *   Implements the 'Interning' pattern for Signals. It ensures that every unique
 *   wire name in the Verilog corresponds to exactly one C++ Signal object instance.
 *
 * Logic:
 *   - Checks the 'net_map' registry.
 *   - If the signal exists, return the existing pointer.
 *   - If not, create a new Signal object, register it, and return the new pointer.
 */
Signal* GraphBuilder::getOrCreateSignal(std::string name) {
    if (net_map.find(name) == net_map.end()) {
        Signal* s = new Signal(name);
        net_map[name] = s;
        std::cout << "[Builder] Created Signal: " << name << std::endl;
    }
    return net_map[name];
}

/* 
 * -----------------------------------------------------------------------------
 * Function: loadDesign
 * -----------------------------------------------------------------------------
 * Purpose:
 *   The main entry point for the frontend. It orchestrates the entire parsing
 *   and graph generation process.
 *
 * Steps:
 *   1. Initialize the Slang Driver.
 *   2. Read the source Verilog file into a buffer.
 *   3. Parse the buffer into a Syntax Tree and create a Compilation.
 *   4. Extract the Root / Top-Level Module.
 *   5. Iterate through the module's members to identify Nets (Wires) and Primitives (Gates).
 *
 * Returns:
 *   true if the design was loaded and elaborated successfully, false otherwise.
 */
const std::string BUILTIN_PRIMITIVES = R"(
    module my_and       (output o, input in1, input in2);           endmodule
    module my_or        (output o, input in1, input in2);           endmodule
    module my_xor       (output o, input in1, input in2);           endmodule
    module my_not       (output o, input in1);                      endmodule
    module my_dff       (output q, input clk, input d);             endmodule
    module my_dff_async (output q, input clk, input rst, input d);  endmodule
)";

bool GraphBuilder::loadDesign(const std::string& filename, const std::string& top_module_name) {
    Driver driver;
    driver.addStandardArgs();

    // -------------------------------------------------------------------------
    // 1. Load the User's File (from Disk)
    //    Attempt to read the source file using Slang's SourceManager.
    //    The second argument 'nullptr' indicates no specific library association is needed.
    // -------------------------------------------------------------------------
    SourceManager::BufferOrError userBuffer = driver.sourceManager.readSource(filename, nullptr);
    if (!userBuffer) {
        std::cerr << "Error: Could not read file " << filename << std::endl;
        return false;
    }
    
    // -------------------------------------------------------------------------
    // 2. Inject the Built-in Primitives (from Memory)
    //    This creates a virtual file named "builtins.v"
    //    
    // -------------------------------------------------------------------------
    SourceBuffer builtinBuffer = driver.sourceManager.assignText("builtins.v", BUILTIN_PRIMITIVES);
    
    // -------------------------------------------------------------------------
    // 3. Create Syntax Trees for BOTH
    //    Create the compilation context and attach the syntax tree generated from the buffer.
    //    
    // -------------------------------------------------------------------------
    auto compilation = driver.createCompilation();

    // Add the user code
    compilation->addSyntaxTree(slang::syntax::SyntaxTree::fromBuffer(*userBuffer, driver.sourceManager));

    // Add the injected code
    compilation->addSyntaxTree(slang::syntax::SyntaxTree::fromBuffer(builtinBuffer, driver.sourceManager));

    // -------------------------------------------------------------------------
    // 4. Find Top Module
    //    Retrieve the root of the AST.
    //    
    // -------------------------------------------------------------------------
    const RootSymbol& root = compilation->getRoot();
    
    // Ensure we actually found a module to simulate.
    if (root.topInstances.empty()) {
        std::cerr << "Error: No top level module found!" << std::endl;
        return false;
    }
    
    const InstanceSymbol* top = nullptr;

    for (auto inst : root.topInstances) {
        std::string name = std::string(inst->name);
        if (name == top_module_name) {
            top = inst;
            break;
        }
    }

    if (!top) {
        std::cerr << "Error: No top module named " << top_module_name << " found!" << std::endl;
        return false;
    }

    std::cout << "[Builder] Elaborating module: " << top->name << std::endl;

    // -------------------------------------------------------------------------
    // 5. Elaborating and traversing the body of the module.
    //    wire (SymbolKind::Net) vs. gate (SymbolKind::PrimitiveInstance)
    //    vs. sub-module (SymbolKind::Instance).
    // -------------------------------------------------------------------------
    for (auto& member : top->body.members()) {
        if (member.kind == SymbolKind::Net) {
            const NetSymbol& net = member.as<NetSymbol>();
            getOrCreateSignal(std::string(net.name));
        }
        else if (member.kind == SymbolKind::PrimitiveInstance) {
            createGate(member.as<PrimitiveInstanceSymbol>());
        }
        else if (member.kind == SymbolKind::Instance) {
            createInstance(member.as<InstanceSymbol>());
        }
    }

    return true;
}

/* 
 * -----------------------------------------------------------------------------
 * Function: createGate
 * -----------------------------------------------------------------------------
 * Purpose:
 *   Converts a Slang Primitive Instance (e.g., 'and g1(y, a, b)') into a 
 *   ToySim C++ Primitive object that can be simulated.
 *
 * Logic:
 *   1. Identify the primitive type (and, or, xor).
 *   2. Resolve all port connections to Signal pointers using 'unwrapSignalName'.
 *   3. Validate that the gate has the minimum required connections (1 Output + 2 Inputs).
 *   4. Instantiate the specific C++ Gate subclass and register it with the Simulator Kernel.
 */
void GraphBuilder::createGate(const PrimitiveInstanceSymbol& gate) {
    std::string typeName = std::string(gate.primitiveType.name);
    std::vector<Signal*> connections;
    
    // Iterate over the ports defined in the Verilog instance.
    // We use the helper function to resolve complex AST expressions into simple wire names.
    for (auto port : gate.getPortConnections()) {
        std::string sigName = unwrapSignalName(port);
        
        if (!sigName.empty()) {
            connections.push_back(getOrCreateSignal(sigName));
        } else {
            // If unwrapping failed, log a warning for debugging purposes.
            if (port) {
               std::cerr << "[Builder] Warning: Could not resolve port expression kind: " 
                         << toString(port->kind) << " on gate " << typeName << std::endl;
            }
        }
    }

    // Safety Check: Ensure the binary gate has enough connections.
    if (typeName == "and" || typeName == "or" || typeName == "xor") {
        if (connections.size() < 3) {
            std::cerr << "[Builder] Error: Skipping '" << typeName 
                      << "' gate. Found " << connections.size() 
                      << " valid connections, expected 3." << std::endl;
            return;
        }
    }
    else if (typeName == "not") {
        if (connections.size() < 2) {
            std::cerr << "[Builder] Error: Skipping '" << typeName 
                      << "' gate. Found " << connections.size() 
                      << " valid connections, expected 2." << std::endl;
            return;
        }
    }

    // Map the connections to specific roles.
    // In Verilog primitives, the first argument is typically the Output.
    Signal* out = connections[0];
    Signal* in1 = connections[1];
    Signal* in2 = (connections.size() > 2) ? connections[2] : nullptr;

    // Hardcoded propagation delay (1 unit of time) for this phase of the project.
    // In a full implementation, we would extract this from the AST's delay field.
    uint64_t default_delay = 1; 

    // Instantiate the appropriate simulation primitive based on the type name.
    if (typeName == "and") {
        new AndGate(sim, default_delay, in1, in2, out);
        std::cout << "[Builder] Created AND Gate driving " << out->name << std::endl;
    } 
    else if (typeName == "or") {
        new OrGate(sim, default_delay, in1, in2, out);
        std::cout << "[Builder] Created OR Gate driving " << out->name << std::endl;
    }
    else if (typeName == "not") {
        new Inverter(sim, default_delay, in1, out);
        std::cout << "[Builder] Created NOT Gate driving " << out->name << std::endl;
    }
    else if (typeName == "xor") {
        new XorGate(sim, default_delay, in1, in2, out);
        std::cout << "[Builder] Created XOR Gate driving " << out->name << std::endl;
    }
}

/* 
 * -----------------------------------------------------------------------------
 * Function: createInstance
 * -----------------------------------------------------------------------------
 * Purpose:
 *   Handles the instantiation of user-defined modules (SymbolKind::Instance).
 *   Unlike built-in primitives (AND, OR, XOR) which are handled by createGate,
 *   these are distinct entities defined via "module ... endmodule".
 *
 * Strategy (Black Box / Leaf Node):
 *   In a fully featured simulator, this function would recursively traverse the 
 *   sub-module's body to "flatten" the hierarchy.
 *   
 *   However, for this phase of ToySim, we treat specific known modules (like "dff")
 *   as "Leaf Primitives" or "Black Boxes". Instead of looking inside the module,
 *   we map the Verilog module directly to a C++ Primitive class.
 *
 * Logic:
 *   1. Retrieve the definition name of the instance (e.g., "dff").
 *   2. If it matches a known type, resolve its port connections.
 *   3. Instantiate the corresponding C++ class (e.g., Dff) and map the signals
 *      based on the positional order of the ports defined in the Verilog module.
 */
void GraphBuilder::createInstance(const InstanceSymbol& inst) {
    std::string moduleName = std::string(inst.getDefinition().name);

    std::vector<Signal*> connections;
    
    // Slang connects ports differently for Instances than Primitives.
    // We iterate the port connections.
    for (auto port : inst.getPortConnections()) {
        if (!port) continue; // Safety check

        // Use getExpression() via arrow operator
        std::string sigName = unwrapSignalName(port->getExpression());
        
        if (!sigName.empty()) {
            connections.push_back(getOrCreateSignal(sigName));
        }
    }

    Signal* out = connections[0];
    Signal* in1 = connections[1];
    Signal* in2 = (connections.size() > 2) ? connections[2] : nullptr;
    Signal* in3 = (connections.size() > 3) ? connections[3] : nullptr;

    // Hardcoded propagation delay (1 unit of time) for this phase of the project.
    // In a full implementation, we would extract this from the AST's delay field.
    uint64_t default_delay = 0;

    if (moduleName == "my_and") {
        if (connections.size() < 3) {
            std::cerr << "[Builder] Error: " << moduleName << " requires 3 ports (out, in1, in2)" << std::endl;
            return;
        }
        new AndGate(sim, default_delay, in1, in2, out);
        std::cout << "[Builder] Created " << moduleName << " module driving " << out->name << std::endl;
    }
    else if (moduleName == "my_or") {
        if (connections.size() < 3) {
            std::cerr << "[Builder] Error: " << moduleName << " requires 3 ports (out, in1, in2)" << std::endl;
            return;
        }
        new OrGate(sim, default_delay, in1, in2, out);
        std::cout << "[Builder] Created " << moduleName << " module driving " << out->name << std::endl;
    }
    else if (moduleName == "my_xor") {
        if (connections.size() < 3) {
            std::cerr << "[Builder] Error: " << moduleName << " requires 3 ports (out, in1, in2)" << std::endl;
            return;
        }
        new XorGate(sim, default_delay, in1, in2, out);
        std::cout << "[Builder] Created " << moduleName << " module driving " << out->name << std::endl;
    }
    else if (moduleName == "my_not") {
        if (connections.size() < 2) {
            std::cerr << "[Builder] Error: " << moduleName << " requires 2 ports (out, in)" << std::endl;
            return;
        }
        new Inverter(sim, default_delay, in1, out);
        std::cout << "[Builder] Created " << moduleName << " module driving " << out->name << std::endl;
    }
    else if (moduleName == "my_dff") {
        if (connections.size() < 3) {
            std::cerr << "[Builder] Error: " << moduleName << " requires 3 ports (q, clk, d)" << std::endl;
            return;
        }
        new Dff(sim, default_delay, in1, in2, out);
        std::cout << "[Builder] Created " << moduleName << " module driving " << out->name << std::endl;
    }
    else if (moduleName == "my_dff_async") {
        if (connections.size() < 4) {
            std::cerr << "[Builder] Error: " << moduleName << " requires 4 ports (q, clk, rst, d)" << std::endl;
            return;
        }
        new DffAsync(sim, default_delay, in1, in2, in3, out);
        std::cout << "[Builder] Created " << moduleName << " module driving " << out->name << std::endl;
    }
    else {
        std::cout << "[Builder] Warning: Unknown module type '" << moduleName << "' ignored." << std::endl;
    }
}