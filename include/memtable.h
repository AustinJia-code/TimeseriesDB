#pragma once

#include <map>
#include <vector>
#include <string>
#include <shared_mutex>
#include <iostream>
#include <mutex>
#include "types.h"

/**
 * Single unit of timeseries data
 */
struct Data
{
    time_t time_ms;
    data_t value;
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
    void insert (const std::string& tag, time_t time_ms, data_t val)
    {
        // Single writer
        std::unique_lock lock (mutex);
        table[tag].push_back (Data {time_ms, val});
    }

    /**
     * Get number of datapoints for tag
     */
    size_t get_count (const std::string& tag)
    {
        // Multi reader
        std::shared_lock lock (mutex);
        if (table.find (tag) != table.end ())
            return table[tag].size ();
        
        return 0;
    }

    /**
     * Get content overview
     */
    void print (std::ostream& out = std::cout)
    {
        std::stringstream ss;
        std::shared_lock lock (mutex);
        for (const auto& [tag, data] : table)
        {
            out << "tag:" << tag 
                << " | Count: " << data.size ()
                << std::endl;
        }
    }
};