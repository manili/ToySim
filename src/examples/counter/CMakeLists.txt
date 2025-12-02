cmake_minimum_required(VERSION 3.15)
project(ToySim)

set(CMAKE_CXX_STANDARD 20)

# -----------------------------------------------------------------------------
# 1. Find Slang and Yosys
# -----------------------------------------------------------------------------
get_filename_component(SLANG_ABS_PATH 
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../slang_install/lib/cmake/slang" 
    ABSOLUTE
)
set(slang_DIR "${SLANG_ABS_PATH}" CACHE PATH "Path to Slang config" FORCE)
find_package(slang REQUIRED)
find_program(YOSYS_BIN yosys REQUIRED)

# -----------------------------------------------------------------------------
# 2. Define File Paths
# -----------------------------------------------------------------------------
# Inputs (In your source folder)
set(VERILOG_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/counter.v")
set(MAPPER_FILE   "${CMAKE_CURRENT_SOURCE_DIR}/mapper.v")

# Output (Goes into the build folder)
set(VERILOG_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/synth_counter.v")

# -----------------------------------------------------------------------------
# 3. Create the Synthesis Command
# -----------------------------------------------------------------------------
# We construct the Yosys script string.
# Note: We use absolute paths (${...}) so Yosys finds the files regardless of where it runs.
set(YOSYS_SCRIPT "read_verilog ${VERILOG_INPUT}; synth; abc -g AND,OR,XOR; techmap -map ${MAPPER_FILE}; opt; write_verilog -noattr -noexpr ${VERILOG_OUTPUT}")

add_custom_command(
    OUTPUT ${VERILOG_OUTPUT}
    COMMAND ${YOSYS_BIN} -p "${YOSYS_SCRIPT}"
    DEPENDS ${VERILOG_INPUT} ${MAPPER_FILE}
    COMMENT "Synthesizing counter.v to synth_counter.v using Yosys..."
    VERBATIM
)

# Create a high-level target that triggers the synthesis
add_custom_target(synthesize_all ALL DEPENDS ${VERILOG_OUTPUT})

# -----------------------------------------------------------------------------
# 4. Build ToySim
# -----------------------------------------------------------------------------
add_executable(toysim 
    main.cpp 
    ../../sim.cpp 
    ../../builder.cpp
    ../../vcd.cpp
)

include_directories(
    ../..
)

target_link_libraries(toysim PRIVATE slang::slang)

# Make sure ToySim builds *after* synthesis (optional, but good for ordering)
add_dependencies(toysim synthesize_all)

# -----------------------------------------------------------------------------
# 5. (Optional) Add a 'make run' command
# -----------------------------------------------------------------------------
# This allows you to type "make run_counter" to simulate the newly synthesized file
add_custom_target(run_counter
    COMMAND ./toysim
    DEPENDS toysim ${VERILOG_OUTPUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ToySim on synthesized counter..."
)
