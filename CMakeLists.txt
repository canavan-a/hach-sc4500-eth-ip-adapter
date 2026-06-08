cmake_minimum_required(VERSION 3.14)
project(hach)

set(CMAKE_CXX_STANDARD 23)

# ── EIPScanner ────────────────────────────────────────────────────────────────
add_subdirectory(third_party/EIPScanner)

# ── nlohmann/json ─────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(json)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(hach main.cpp)

target_include_directories(hach PRIVATE ${CMAKE_SOURCE_DIR}/third_party/EIPScanner/src)
target_link_libraries(hach
    EIPScanner
    nlohmann_json::nlohmann_json
)
