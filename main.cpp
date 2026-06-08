#include <MessageRouter.h>
#include <utils/Buffer.h>

#include <mosquitto.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "config.h"

using eipScanner::MessageRouter;
using eipScanner::SessionInfo;
using namespace eipScanner::cip;
using namespace eipScanner::utils;
using json = nlohmann::json;

// ── Result type (C++17, replaces std::expected) ───────────────────────────────

struct CipError {
    std::string message;
    int         statusCode = -1;
};

template <typename T>
class Result {
public:
    Result(T val) : data_(std::move(val)) {}
    Result(CipError err) : data_(std::move(err)) {}

    explicit operator bool() const { return std::holds_alternative<T>(data_); }

    T&       operator*()       { return std::get<T>(data_); }
    const T& operator*() const { return std::get<T>(data_); }
    T*       operator->()       { return &std::get<T>(data_); }
    const T* operator->() const { return &std::get<T>(data_); }

    CipError&       error()       { return std::get<CipError>(data_); }
    const CipError& error() const { return std::get<CipError>(data_); }

private:
    std::variant<T, CipError> data_;
};

template <typename T>
Result<T> make_error(CipError err) { return Result<T>(std::move(err)); }

// ── Tag layout ───────────────────────────────────────────────────────────────

enum class TagType { Float, Integer };

struct TagDef {
    const char* name;
    TagType     type;
    size_t      offset;
};

static constexpr TagDef TAGS[] = {
    {"Raw pH",                  TagType::Float,   0},
    {"Raw Temp",                TagType::Float,   4},
    {"Raw Device Warning",      TagType::Integer, 8},
    {"Raw Device Error",        TagType::Integer, 10},
    {"Raw pH Heartbeat",        TagType::Integer, 12},
    {"Treated pH",              TagType::Float,   14},
    {"Treated Temp",            TagType::Float,   18},
    {"Treated Device Warning",  TagType::Integer, 22},
    {"Treated Device Error",    TagType::Integer, 24},
    {"Treated pH Heartbeat",    TagType::Integer, 26},
    {"sc4500 Measurement",      TagType::Integer, 28},
    {"HV Relay Position",       TagType::Integer, 30},
    {"HV Relay Input Ch2",      TagType::Float,   32},
};

// ── Decode helpers ────────────────────────────────────────────────────────────

template <typename T>
static T read_le(const std::vector<uint8_t>& data, size_t offset) {
    T val{};
    std::memcpy(&val, data.data() + offset, sizeof(T));
    return val;  // EtherNet/IP wire format is little-endian; x86 host matches
}

// ── CIP query ────────────────────────────────────────────────────────────────

static Result<std::vector<uint8_t>> fetchRawData(MessageRouter& router,
                                                  const std::shared_ptr<SessionInfo>& si) {
    auto response = router.sendRequest(
        si,
        ServiceCodes::GET_ATTRIBUTE_SINGLE,
        EPath(0x04, 100, 0x03),
        {});

    if (response.getGeneralStatusCode() != GeneralStatusCodes::SUCCESS) {
        return make_error<std::vector<uint8_t>>(CipError{
            "CIP request failed",
            static_cast<int>(response.getGeneralStatusCode())});
    }

    auto raw = response.getData();
    if (raw.empty()) {
        return make_error<std::vector<uint8_t>>(CipError{"Empty response from device"});
    }

    return std::vector<uint8_t>(raw.begin(), raw.end());
}

// ── Decode payload → JSON ────────────────────────────────────────────────────

static Result<json> decodePayload(const std::vector<uint8_t>& data) {
    json out;
    out["raw_hex"]    = [&] {
        std::string h;
        h.reserve(data.size() * 2);
        for (uint8_t b : data) {
            char buf[3];
            std::snprintf(buf, sizeof(buf), "%02x", b);
            h += buf;
        }
        return h;
    }();
    out["byte_count"] = data.size();

    json tags = json::object();
    for (const auto& tag : TAGS) {
        if (tag.type == TagType::Float) {
            if (data.size() < tag.offset + sizeof(float)) {
                tags[tag.name] = nullptr;
                continue;
            }
            float v = read_le<float>(data, tag.offset);
            tags[tag.name] = v;
        } else {
            if (data.size() < tag.offset + sizeof(uint16_t)) {
                tags[tag.name] = nullptr;
                continue;
            }
            uint16_t v = read_le<uint16_t>(data, tag.offset);
            tags[tag.name] = v;
        }
    }
    out["tags"] = std::move(tags);
    return out;
}

// ── Session helper ────────────────────────────────────────────────────────────

static Result<std::shared_ptr<SessionInfo>> openSession() {
    try {
        auto si = std::make_shared<SessionInfo>(config::ipAddress, config::port);
        return si;
    } catch (const std::exception& e) {
        return make_error<std::shared_ptr<SessionInfo>>(CipError{std::string("Session open failed: ") + e.what()});
    }
}

// ── MQTT publish ─────────────────────────────────────────────────────────────

static void mqttPublish(const std::string& payload) {
    mosquitto_lib_init();
    mosquitto* mosq = mosquitto_new(nullptr, true, nullptr);
    if (!mosq) {
        std::cerr << "mosquitto_new failed\n";
        mosquitto_lib_cleanup();
        return;
    }
    if (mosquitto_connect(mosq, "localhost", 1234, 60) != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT connect failed\n";
    } else {
        mosquitto_publish(mosq, nullptr, config::recTopic.c_str(),
                          static_cast<int>(payload.size()), payload.c_str(), 0, false);
    }
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

// ── Main loop ─────────────────────────────────────────────────────────────────

int main() {
    std::cout << "Polling " << config::ipAddress << " every "
              << config::pollMs << " ms\n";

    while (true) {
        auto sessionResult = openSession();
        if (!sessionResult) {
            std::cerr << "{\"error\":\"" << sessionResult.error().message << "\"}\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(config::pollMs));
            continue;
        }

        MessageRouter router;
        auto dataResult = fetchRawData(router, *sessionResult);

        if (!dataResult) {
            const auto& err = dataResult.error();
            json errJson;
            errJson["error"]  = err.message;
            errJson["status"] = err.statusCode;
            std::cerr << errJson.dump() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(config::pollMs));
            continue;
        }

        auto decodeResult = decodePayload(*dataResult);
        if (!decodeResult) {
            std::cerr << "{\"error\":\"" << decodeResult.error().message << "\"}\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(config::pollMs));
            continue;
        }

        std::string payload = decodeResult->dump();
        std::cout << decodeResult->dump(2) << "\n";
        mqttPublish(payload);
        std::this_thread::sleep_for(std::chrono::milliseconds(config::pollMs));
    }
}
