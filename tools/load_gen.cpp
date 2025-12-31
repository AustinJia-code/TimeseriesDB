#include <cmath>
#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <functional>
#include <cstdio>
#include <httplib.h>
#include "types.h"

/**
 * Class for device gen functions
 */
class LoadGenerator
{
private:
    std::string address;
    int port;

public:
    /**
     * Default (0.0.0.0:8080)
     */
    LoadGenerator () : address ("0.0.0.0"), port (9090) {} 

    /**
     * Custom endpoint (address:port)
     */
    LoadGenerator (const std::string& address, id_t port) :
        address (address), port (port) {}

    /**
     * Get sine wave value based on current time and sine wave
     * base: vertical offset
     * amplitude: amplitude
     * period_ms: period
     * time_ms: current time ms
     */
    data_t get_sine_wave (data_t base, data_t amplitude, time_t period_ms, time_t time_ms)
    {
        return base + amplitude * std::sin ((2.0 * M_PI * time_ms) / period_ms);
    }

    /**
     * Get sawtooth value based on current time and sawtooth range
     * min: min sawtooth value
     * max: max sawtooth value
     * period_ms: time from min to max
     * time_ms: current time ms
     */
    data_t get_sawtooth (data_t min, data_t max, time_t period_ms, time_t time_ms)
    {
        return min + (std::fmod (time_ms, period_ms) * (max - min) / period_ms);
    }

    /**
     * Simulated threaded device output
     * device_id: device id for current device
     * total_count: shared counter for number of data points output by devices
     */
    void start_worker (id_t device_id, time_t rate_ms, std::atomic<size_t>& total_count,
                       std::function<data_t (time_t)> generator_func)
    {
        while (true)
        {
            // Get data
            const auto now = std::chrono::system_clock::now ();
            const time_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds> 
                                  (now.time_since_epoch ()).count ();

            data_t data = generator_func (now_ms);
            
            // Post
            std::string payload = std::to_string (device_id) + "," +
                                  std::to_string (now_ms) + "," +
                                  std::to_string (data);
            httplib::Client client (address, port);
            httplib::Result res = client.Post ("/write", payload, "text/plain");

            if (res && res->status == httplib::StatusCode::OK_200)
                ++total_count;
            else
                auto error = res.error ();

            // Sleep
            std::this_thread::sleep_for (std::chrono::milliseconds (rate_ms));
        }
    }
};

/**
 * Runner
 */
int main ()
{
    LoadGenerator lg;
    std::atomic<size_t> total_count (0);
    std::vector<std::thread> threads;

    // Sine data (temperature, sound)
    threads.emplace_back ([&] () {
        lg.start_worker (id_t {1},
                        time_t {1},
                        total_count,
                        [&](time_t t) {
                            return lg.get_sine_wave (data_t {25.0},
                                                     data_t {5.0},
                                                     time_t {60000},
                                                     t);});});
    // Sawtooth data (counter, encoder)
    threads.emplace_back ([&] () {
        lg.start_worker (id_t {2},
                        time_t {2},
                        total_count,
                        [&](time_t t) {
                            return lg.get_sawtooth (data_t {2.0},
                                                    data_t {19.0},
                                                    time_t {4000},
                                                    t);});});
    // Random noise
    threads.emplace_back ([&] () {
        lg.start_worker (id_t {3},
                        time_t {3},
                        total_count,
                        [&](time_t unused) {
                            return static_cast<double> (rand () % 10);});});

    // Reporter
    std::thread reporter ([&] ()
    {
        while (true)
        {
            std::this_thread::sleep_for (std::chrono::seconds (1));
            std::printf ("PPS: %ld\n", total_count.exchange (0));
        }
    });

    for (std::thread& t : threads)
        t.join ();
    
    return EXIT_SUCCESS;
}