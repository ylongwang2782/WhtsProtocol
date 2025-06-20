#include "../Logger.h"
#include "SlaveDevice.h"
#include <exception>
#include <iostream>
#include <string>

using namespace SlaveApp;

/**
 * Helper function to get Device ID from user input
 */
uint32_t getDeviceIdFromUser() {
    std::string input;
    uint32_t deviceId = 0x00000001; // Default value

    Log::i("Main", "=== WhtsProtocol Slave Device Configuration ===");
    Log::i("Main", "Please enter Device ID (hex format, e.g., 0x3732485B)");
    Log::i("Main", "Press Enter for default (0x00000001): ");

    std::getline(std::cin, input);

    // Trim whitespace
    input.erase(input.find_last_not_of(" \t\r\n") + 1);
    input.erase(0, input.find_first_not_of(" \t\r\n"));

    if (!input.empty()) {
        try {
            // Support both 0x prefix and without prefix
            if (input.substr(0, 2) == "0x" || input.substr(0, 2) == "0X") {
                deviceId = std::stoul(input, nullptr, 16);
            } else {
                deviceId = std::stoul(input, nullptr, 16);
            }
            Log::i("Main", "Using Device ID: 0x%08X", deviceId);
        } catch (const std::exception &e) {
            Log::e("Main",
                   "Invalid input, using default Device ID: 0x00000001");
            deviceId = 0x00000001;
        }
    } else {
        Log::i("Main", "Using default Device ID: 0x00000001");
    }

    return deviceId;
}

int main() {
    Log::i("Main", "WhtsProtocol Slave Device");
    Log::i("Main", "=========================");

    try {
        // Get Device ID from user input
        uint32_t deviceId = getDeviceIdFromUser();

        // Display configuration after getting Device ID
        Log::i("Main", "Port Configuration (Wireless Broadcast Simulation):");
        Log::i("Main", "  Backend: 8079");
        Log::i("Main", "  Master:  8080 (receives responses from Slaves)");
        Log::i("Main",
               "  Slaves:  8081 (listen for Master broadcast commands)");
        Log::i("Main", "Wireless Communication Simulation:");
        Log::i("Main", "  Receives: Broadcast commands from Master");
        Log::i("Main", "  Sends: Unicast responses to Master");
        Log::i("Main", "Device Configuration:");
        Log::i("Main", "  Device ID: 0x%08X", deviceId);

        Log::i("Main", "Starting slave device...");

        // Create and initialize slave device
        SlaveDevice device(8081, deviceId);
        if (!device.initialize()) {
            Log::e("Main", "Failed to initialize slave device");
            return 1;
        }

        // Run the device
        device.run();
    } catch (const std::exception &e) {
        Log::e("Main", "Error: %s", e.what());
        return 1;
    }

    return 0;
}