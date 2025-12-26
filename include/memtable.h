#include <map>
#include <vector>
#include <string>
#include <shared_mutex>
#include <iostream>
#include <mutex>

/**
 * Single unit of timeseries data
 */
struct Data
{
    time_t time_ms;
    double value;
};

/**
 * Thread-safe memory storage
 */
class MemTable
{
private:
    std::map<std::string, std::vector<Data>> table;
    mutable std::shared_mutex mutex;

public:
    /**
     * Insert data into the MemTable
     */
    void insert (const std::string& metric, time_t time_ms, double val)
    {
        // Single writer
        std::unique_lock lock (mutex);
        table[metric].push_back (Data {time_ms, val});
    }

    /**
     * Get number of datapoints for metric
     */
    size_t get_count (const std::string& metric)
    {
        // Multi reader
        std::shared_lock lock (mutex);
        if (table.find (metric) != table.end ())
            return table[metric].size ();
        
        return 0;
    }

    /**
     * Get content overview
     */
    void print (std::ostream& out = std::cout)
    {
        std::stringstream ss;
        std::shared_lock lock (mutex);
        for (const auto& [metric, data] : table)
        {
            out << "Metric:" << metric 
                << " | Count: " << data.size ()
                << std::endl;
        }
    }
};