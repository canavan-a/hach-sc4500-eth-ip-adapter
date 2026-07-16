cmake_minimum_required(VERSION 3.13)
project(hach)

set(CMAKE_CXX_STANDARD 17)

# ── EIPScanner ────────────────────────────────────────────────────────────────
add_subdirectory(third_party/EIPScanner)

# ── nlohmann/json ─────────────────────────────────────────────────────────────
add_subdirectory(third_party/nlohmann_json)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(hach main.cpp)

target_include_directories(hach PRIVATE ${CMAKE_SOURCE_DIR}/third_party/EIPScanner/src)
target_link_libraries(hach
    EIPScannerS
    nlohmann_json::nlohmann_json
    mosquitto
)
