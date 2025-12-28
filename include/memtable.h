#pragma once

#include <map>
#include <vector>
#include <string>
#include <shared_mutex>
#include <iostream>
#include <mutex>
#include <fstream>
#include "types.h"

/**
 * Thread-safe memory storage
 */
class MemTable
{
private:
    table_t table;
    mutable std::shared_mutex mutex;
    std::atomic<size_t> total_count {0};

public:
    /**
     * Insert data into the MemTable
     */
    void insert (const std::string& tag, time_t time_ms, data_t val)
    {
        // Single writer
        std::unique_lock lock (mutex);
        table[tag].push_back (Data {time_ms, val});
        ++total_count;
    }

    /**
     * Get total number of data points in the MemTable
     */
    size_t get_total_count () const
    {
        return total_count.load ();
    }

    /**
     * Get number of datapoints for tag
     */
    size_t get_count (const std::string& tag) const
    {
        // Multi reader
        std::shared_lock lock (mutex);
        if (table.find (tag) != table.end ())
            return table.at (tag).size ();
        
        return 0;
    }

    /**
     * Move MemTable into a snapshot, clear MemTable
     */
    table_t extract ()
    {
        std::unique_lock lock (mutex);
        table_t snapshot = std::move (table);
        table.clear ();
        
        total_count.store (0);

        return snapshot;
    }

    /**
     * Flush table to disk (Sorted String Table), timestamp sorted
     */
    void flush (const table_t& d_table, id_t batch_id) const
    {
        std::string path = "../disk/sstables/sstable_" + std::to_string (batch_id) + ".db";
        std::ofstream out (path, std::ios::binary);

        for (const auto& [tag, data] : d_table)
        {
            // Write tag
            size_t tag_len = tag.size ();
            out.write (reinterpret_cast <const char*> (&tag_len), sizeof (tag_len));
            out.write (tag.c_str (), tag_len);

            // Write number of data points for this tag
            size_t num_points = data.size ();
            out.write (reinterpret_cast<const char*> (&num_points), sizeof (num_points));

            // Write data
            for (const Data& d : data)
            {
                out.write (reinterpret_cast<const char*> (&d.time_ms), sizeof (d.time_ms));
                out.write (reinterpret_cast<const char*> (&d.value), sizeof (d.value));
            }
        }

        out.close ();
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

        float estimated_kb = get_total_count () * sizeof (Data) >> 10;
        out << "Estimated KB: " << estimated_kb << std::endl;
    }
};