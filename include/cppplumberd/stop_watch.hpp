#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <numeric>
#include <iostream>

namespace cppplumberd {

    class StopWatch {
    private:
        using clock_type = std::chrono::high_resolution_clock;
        using time_point = clock_type::time_point;
        using duration = std::chrono::nanoseconds;

        time_point _startTime;
        time_point _stopTime;
        bool _isRunning = false;
        std::vector<duration> _laps;

    public:
        // Creates a new instance and starts timing
        static StopWatch StartNew() {
            StopWatch sw;
            sw.Start();
            return sw;
        }

        // Default constructor - doesn't start timing
        StopWatch() = default;

        // Start the stopwatch
        void Start() {
            if (!_isRunning) {
                _startTime = clock_type::now();
                _isRunning = true;
            }
        }

        // Stop the stopwatch
        void Stop() {
            if (_isRunning) {
                _stopTime = clock_type::now();
                _isRunning = false;
            }
        }

        // Reset the stopwatch
        void Reset() {
            _isRunning = false;
            _laps.clear();
        }

        // Restart the stopwatch
        void Restart() {
            Reset();
            Start();
        }

        // Record a lap time while continuing to run
        void Lap() {
            if (_isRunning) {
                auto now = clock_type::now();
                auto elapsed = now - _startTime;
                _laps.push_back(std::chrono::duration_cast<duration>(elapsed));
            }
        }

        // Elapsed time in nanoseconds
        int64_t ElapsedNanoseconds() const {
            auto elapsed = GetElapsedDuration();
            return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        }

        // Elapsed time in microseconds
        int64_t ElapsedMicroseconds() const {
            auto elapsed = GetElapsedDuration();
            return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        }

        // Elapsed time in milliseconds
        int64_t ElapsedMilliseconds() const {
            auto elapsed = GetElapsedDuration();
            return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        }

        // Elapsed time in seconds
        double ElapsedSeconds() const {
            auto elapsed = GetElapsedDuration();
            return std::chrono::duration<double>(elapsed).count();
        }

        // Get all lap times in milliseconds
        std::vector<int64_t> GetLapMilliseconds() const {
            std::vector<int64_t> result;
            result.reserve(_laps.size());

            for (const auto& lap : _laps) {
                result.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(lap).count());
            }

            return result;
        }

        // Get average lap time in milliseconds
        double GetAverageLapMilliseconds() const {
            if (_laps.empty()) return 0.0;

            int64_t sum = 0;
            for (const auto& lap : _laps) {
                sum += std::chrono::duration_cast<std::chrono::milliseconds>(lap).count();
            }

            return static_cast<double>(sum) / _laps.size();
        }

        // Print elapsed time with unit
        void PrintElapsed(const std::string& label = "Elapsed time") const {
            auto elapsed = ElapsedNanoseconds();

            if (elapsed < 1000) {
                std::cout << label << ": " << elapsed << " ns" << std::endl;
            }
            else if (elapsed < 1000000) {
                std::cout << label << ": " << ElapsedMicroseconds() << " us" << std::endl;
            }
            else if (elapsed < 1000000000) {
                std::cout << label << ": " << ElapsedMilliseconds() << " ms" << std::endl;
            }
            else {
                std::cout << label << ": " << ElapsedSeconds() << " s" << std::endl;
            }
        }

    private:
        // Get the elapsed duration
        std::chrono::nanoseconds GetElapsedDuration() const {
            if (_isRunning) {
                auto now = clock_type::now();
                return std::chrono::duration_cast<duration>(now - _startTime);
            }
            else {
                return std::chrono::duration_cast<duration>(_stopTime - _startTime);
            }
        }
    };

} // namespace cppplumberd