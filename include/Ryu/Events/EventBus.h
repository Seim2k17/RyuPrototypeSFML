#pragma once

#include "Ryu/Events/EventEnums.h"

#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <any>
#include <functional>

#include <fmt/core.h>

// for the BETTER_ENUM EPhysicsEvent we need to define a hash_function t use it as a key
namespace std {
    template<>
    struct hash<Ryu::EPhysicsEvent> {
        size_t operator()(const Ryu::EPhysicsEvent& event) const {
            // Hash the underlying value of the Better Enum
            return hash<char>()(static_cast<char>(event));
        }
    };
}

class EventBus {
public:
    using Callback = std::function<void(std::any)>;

    static void emit(const Ryu::EPhysicsEvent& eventName, std::any data) {
        // TODO: when multithreading is activated
        // std::lock_guard<std::mutex> lock(sMutex);
        //fmt::print("Emit event {}. \n", eventName._to_string());
        for (auto& callback : sCallbacks[eventName]) {
            callback(data);
        }
    }

    static void subscribe(const Ryu::EPhysicsEvent& eventName, Callback callback) {
        // TODO: when multithreading is activated
        // std::lock_guard<std::mutex> lock(sMutex);
        sCallbacks[eventName].push_back(callback);
    }

private:
    static inline std::unordered_map<Ryu::EPhysicsEvent, std::vector<Callback>> sCallbacks;
    // TODO: use mutex when code will be multithreaded
    // TODO: need hashfunction
    // static inline std::mutex sMutex;  // Protects sCallbacks
};
