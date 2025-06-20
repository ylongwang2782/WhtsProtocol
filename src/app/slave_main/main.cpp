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

    std::cout << "\n=== WhtsProtocol Slave Device Configuration ==="
              << std::endl;
    std::cout << "Please enter Device ID (hex format, e.g., 0x3732485B)"
              << std::endl;
    std::cout << "Press Enter for default (0x00000001): ";

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
            std::cout << "Using Device ID: 0x" << std::hex << std::uppercase
                      << deviceId << std::endl;
        } catch (const std::exception &e) {
            std::cout << "Invalid input, using default Device ID: 0x00000001"
                      << std::endl;
            deviceId = 0x00000001;
        }
    } else {
        std::cout << "Using default Device ID: 0x00000001" << std::endl;
    }

    return deviceId;
}

int main() {
    std::cout << "[INFO] Main: WhtsProtocol Slave Device" << std::endl;
    std::cout << "[INFO] Main: =========================" << std::endl;

    try {
        // Get Device ID from user input
        uint32_t deviceId = getDeviceIdFromUser();

        // Display configuration after getting Device ID
        std::cout << "[INFO] Main: \nPort Configuration (Wireless Broadcast "
                     "Simulation):"
                  << std::endl;
        std::cout << "[INFO] Main:   Backend: 8079" << std::endl;
        std::cout
            << "[INFO] Main:   Master:  8080 (receives responses from Slaves)"
            << std::endl;
        std::cout << "[INFO] Main:   Slaves:  8081 (listen for Master "
                     "broadcast commands)"
                  << std::endl;
        std::cout << "[INFO] Main: Wireless Communication Simulation:"
                  << std::endl;
        std::cout << "[INFO] Main:   Receives: Broadcast commands from Master"
                  << std::endl;
        std::cout << "[INFO] Main:   Sends: Unicast responses to Master"
                  << std::endl;
        std::cout << "[INFO] Main: Device Configuration:" << std::endl;
        std::cout << "[INFO] Main:   Device ID: 0x" << std::hex << deviceId
                  << std::dec << std::endl;

        std::cout << "\nStarting slave device..." << std::endl;

        // Create and initialize slave device
        SlaveDevice device(8081, deviceId);
        if (!device.initialize()) {
            std::cout << "[ERROR] Main: Failed to initialize slave device"
                      << std::endl;
            return 1;
        }

        // Run the device
        device.run();
    } catch (const std::exception &e) {
        std::cout << "[ERROR] Main: Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}