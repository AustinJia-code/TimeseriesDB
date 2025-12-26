#include <cmath>
#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <functional>
#include <cstdio>
#include <httplib.h>

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
    LoadGenerator (const std::string& address, int port) :
        address (address), port (port) {}

    /**
     * Get sine wave value based on current time and sine wave
     * base: vertical offset
     * amplitude: amplitude
     * period_ms: period
     * time_ms: current time ms
     */
    double getSineWave (double base, double amplitude, time_t period_ms, time_t time_ms)
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
    double getSawtooth (double min, double max, time_t period_ms, time_t time_ms)
    {
        return min + (std::fmod (time_ms, period_ms) * (max - min) / period_ms);
    }

    /**
     * Simulated threaded device output
     * device_id: device id for current device
     * total_count: shared counter for number of data points output by devices
     */
    void startWorker (int device_id, int rate_ms, std::atomic<int>& total_count,
                      std::function<double (time_t)> generatorFunc)
    {
        while (true)
        {
            // Get data
            const auto now = std::chrono::system_clock::now ();
            const time_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds> 
                                (now.time_since_epoch ()).count ();

            double data = generatorFunc (now_ms);
            
            // Post
            std::string payload = std::to_string (device_id) + "," +
                                  std::to_string (now_ms) + "," +
                                  std::to_string (data);
            httplib::Client client (address, port);
            httplib::Result res = client.Post("/write", payload, "text/plain");

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
    std::atomic<int> total_count (0);
    std::vector<std::thread> threads;

    // Sine data (temperature, sound)
    threads.emplace_back ([&] () {
        lg.startWorker (1, 100, total_count, [&](time_t t) {
            return lg.getSineWave (25.0, 5.0, 60000, t); }); });
    
    // Sawtooth data (counter, encoder)
    threads.emplace_back ([&] () {
        lg.startWorker (2, 100, total_count, [&](time_t t) {
            return lg.getSineWave (25.0, 5.0, 60000, t); }); });
    
    // Random noise
    threads.emplace_back ([&] () {
        lg.startWorker (3, 100, total_count, [&](time_t unused) {
            return static_cast<double> (rand () % 10); }); });

    // Reporter
    std::thread reporter ([&] ()
    {
        while (true)
        {
            std::this_thread::sleep_for (std::chrono::seconds (1));
            std::printf ("PPS: %d\n", total_count.exchange (0));
        }
    });

    for (std::thread& t : threads)
        t.join ();
    
    return EXIT_SUCCESS;
}