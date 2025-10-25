#ifndef _TRACKER_H_
#define _TRACKER_H_

#include <string>
#include <sstream>
#include <thread>
#include <iomanip>
#include <chrono>


#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

namespace Tracker {
    /*class ProgressTracker {
        private:
            std::chrono::steady_clock::time_point startTime;
            std::string currentOperation;
            int barWidth = 50;

        public:
            void StartOperation(const std::string& operation) {
                startTime = std::chrono::steady_clock::now();
                currentOperation = operation;
                std::cout << COLOR_CYAN << "[START] " << operation << COLOR_RESET << std::endl;
            }

            void UpdateProgress(float progress, const std::string& status = "") {
                auto currentTime = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

                std::cout << "\r" << COLOR_BLUE << "[" << std::fixed << std::setprecision(1) << (progress * 100) << "%] " << COLOR_RESET;

                // Progress bar
                int pos = barWidth * progress;
                std::cout << "[";
                for (int i = 0; i < barWidth; ++i) {
                    if (i < pos) std::cout << COLOR_GREEN << "=" << COLOR_RESET;
                    else if (i == pos) std::cout << COLOR_GREEN << ">" << COLOR_RESET;
                    else std::cout << " ";
                }
                std::cout << "] ";

                // Time estimation
                if (progress > 0) {
                    int totalEstimated = elapsed / progress;
                    int remaining = totalEstimated - elapsed;
                    std::cout << "Elapsed: " << FormatTime(elapsed) << " | ETA: " << FormatTime(remaining);
                }

                if (!status.empty()) {
                    std::cout << " | " << status;
                }

                std::cout << std::flush;
            }

            void CompleteOperation(const std::string& message = "") {
                auto endTime = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();

                std::cout << "\r" << COLOR_GREEN << "[COMPLETE] " << currentOperation << " - " << FormatTime(duration);
                if (!message.empty()) {
                    std::cout << " - " << message;
                }
                std::cout << COLOR_RESET << std::endl;
            }

            void FailOperation(const std::string& error = "") {
                auto endTime = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();

                std::cout << "\r" << COLOR_RED << "[FAILED] " << currentOperation << " - " << FormatTime(duration);
                if (!error.empty()) {
                    std::cout << " - " << error;
                }
                std::cout << COLOR_RESET << std::endl;
            }

        private:
            std::string FormatTime(int seconds) {
                int hours = seconds / 3600;
                int minutes = (seconds % 3600) / 60;
                int secs = seconds % 60;

                std::stringstream ss;
                if (hours > 0) ss << hours << "h ";
                if (minutes > 0) ss << minutes << "m ";
                ss << secs << "s";
                return ss.str();
            }
    };*/

};

#endif
