cmake_minimum_required(VERSION 3.15)
project(ToySim)

set(CMAKE_CXX_STANDARD 20)

# Find Slang
get_filename_component(SLANG_ABS_PATH 
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../slang_install/lib/cmake/slang" 
    ABSOLUTE
)
set(slang_DIR "${SLANG_ABS_PATH}" CACHE PATH "Path to Slang config" FORCE)
find_package(slang REQUIRED)

# Our Source Files
add_executable(toysim 
    main.cpp 
    ../../sim.cpp 
    ../../builder.cpp
    ../../vcd.cpp
)

include_directories(
    ../..
)

# Link against Slang
target_link_libraries(toysim PRIVATE slang::slang)

# Copy Verilog Source File
set(VERILOG_INPUT "sync_half_adder.v")
add_custom_command(
    TARGET toysim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/${VERILOG_INPUT}"
        "$<TARGET_FILE_DIR:toysim>/${VERILOG_INPUT}"
    COMMENT "Copying ${VERILOG_INPUT} to executable directory..."
    VERBATIM
)