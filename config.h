#pragma once
#include <string>
#include <cstdint>

namespace config {
    inline std::string ipAddress = "192.168.33.40";
    inline uint16_t    port      = 0xAF12;
    inline int         pollMs    = 5000;
    inline std::string recTopic = "hach/sc4500/rec";
    inline std::string controlTopic = "hach/sc4500/control"
}
