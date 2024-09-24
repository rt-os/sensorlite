#include "lib/serialib.h"
#include <unistd.h>
#include <stdio.h>
#include <regex>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

#if defined (_WIN32) || defined( _WIN64)
    #define WINDOWS_OS
#elif defined (__linux__) || defined(__APPLE__)
    #define UNIX_OS
#endif

serialib serial;

int main() {
    char device_name[32];
    std::string activePort;

    // Scan serial ports
    for (int i = 1; i < 99; i++) {
        #ifdef WINDOWS_OS
            sprintf(device_name, "\\\\.\\COM%d", i);
        #elif defined(UNIX_OS)
            sprintf(device_name, "/dev/ttyACM%d", i - 1);
        #endif
        if (serial.openDevice(device_name, 115200) == 1) {
            activePort = device_name;
            serial.closeDevice();
            break;
        }
    }
    if (activePort.empty()) {
        printf("No active serial ports found.\n");
        return -1;
    }
    printf("Connecting to %s...\n", activePort.c_str());
    if (serial.openDevice(activePort.c_str(), 115200) != 1) {
        printf("Failed to connect to %s\n", activePort.c_str());
        return -1;
    }

    // Create filename based on timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_time_t);
    std::ostringstream oss;
    oss << std::put_time(now_tm, "%Y%m%d_%H%M%S") << ".csv";
    std::string filename = oss.str();
    printf("file: %s\n", filename.c_str());

    std::ofstream csvFile(filename);
    csvFile << "Timestamp (PST),Sensor Number,Measurement" << std::endl;

    char buffer[256];
    int bytesRead;
    std::regex regexPattern("D(\\d)\\s*\\(mm\\):\\s*(\\d+)");
    std::smatch match;

    while (true) {
        bytesRead = serial.readString(buffer, '\r', sizeof(buffer) - 1, 5000);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string input(buffer);
            for (auto i = std::sregex_iterator(input.begin(), input.end(), regexPattern); i != std::sregex_iterator(); ++i) {
                match = *i;
                long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::system_clock::now().time_since_epoch()).count();
                csvFile << timestamp << "," << match.str(1) << "," << match.str(2) << std::endl;
                printf("%lld,%s,%s\n", timestamp, match.str(1).c_str(), match.str(2).c_str());
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
