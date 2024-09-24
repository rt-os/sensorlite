#include "lib/serialib.h"
#include <unistd.h>
#include <stdio.h>
#include <regex>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <dirent.h>

serialib serial;
std::string findSerialPortUnix() {
    DIR *dir = opendir("/dev");
    struct dirent *entry;
    const std::regex serialPattern("/dev/(tty|cu)\\.(usbserial|usbmodem|SLAB_USBtoUART).*");
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            std::string device = "/dev/" + std::string(entry->d_name);
            if (std::regex_match(device, serialPattern) && serial.openDevice(device.c_str(), 115200) == 1) {
                serial.closeDevice();
                closedir(dir);
                return device;
            }
        }
        closedir(dir);
    }
    return "";
}

std::string findActiveSerialPort() {
    #if defined(_WIN32) || defined(_WIN64)
    char device[32];
    for (int i = 1; i < 99; i++) {
        sprintf(device, "\\\\.\\COM%d", i);
        if (serial.openDevice(device, 115200) == 1) {
            serial.closeDevice();
            return std::string(device);
        }
    }
    return "";
    #else
    return findSerialPortUnix();  // Scan /dev for Unix-like systems
    #endif
}

int main() {
    std::string activePort = findActiveSerialPort();
    if (activePort.empty()) {
        printf("No active serial ports found.\n");
        return -1;
    }
    if (serial.openDevice(activePort.c_str(), 115200) != 1) {
        printf("Failed to connect to %s\n", activePort.c_str());
        return -1;
    }

    auto now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S") << ".csv";
    std::ofstream csvFile(oss.str());
    csvFile << "timestamp,sensor,value\n";

    char buffer[256];
    std::regex regexPattern("D(\\d)\\s*\\(mm\\):\\s*(\\d+)");
    while (true) {
        int bytesRead = serial.readString(buffer, '\r', sizeof(buffer) - 1, 5000);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string input(buffer);
            for (std::sregex_iterator i(input.begin(), input.end(), regexPattern); i != std::sregex_iterator(); ++i) {
                auto match = *i;
                long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch()).count();
                csvFile << timestamp << "," << match.str(1) << "," << match.str(2) << "\n";
                printf("Logged: %lld,%s,%s\n", timestamp, match.str(1).c_str(), match.str(2).c_str());
            }
        } else if (bytesRead == -2) {
            printf("Timeout while waiting for data.\n");
        } else {
            printf("Error reading from serial port.\n");
            break;
        }
    }

    serial.closeDevice();
    csvFile.close();
    return 0;
}
