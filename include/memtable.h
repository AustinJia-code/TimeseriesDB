#pragma once

#include <map>
#include <vector>
#include <string>
#include <shared_mutex>
#include <iostream>
#include <mutex>
#include <fstream>
#include "types.h"
#include "bit_buffer.h"
#include "gorilla.h"
#include "tsdb_config.h"

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
     * Get data corresponding to tag
     */
    std::vector<Data> get_data (const std::string& tag) const
    {
        std::vector<Data> result {table.at (tag)};
        return result;
    }

    /**
     * Flush table to disk (Sorted String Table), timestamp sorted
     * Not thread-safe, pass in extracted MemTable!
     */
    void flush (const table_t& d_table, id_t batch_id) const
    {
        Gorilla gorilla;
        std::string path = get_sstable_path (std::to_string (batch_id));
        std::ofstream out (path, std::ios::binary);

        for (const auto& [tag, data] : d_table)
        {
            if (data.empty ())
                continue;
            
            BitWriter writer;
            gorilla.encode (data, writer);
            writer.flush ();

            // Write tag
            size_t tag_len = tag.size ();
            out.write (reinterpret_cast<const char*>(&tag_len), sizeof (tag_len));
            out.write (tag.data (), tag_len);

            // Write num points
            size_t num_pts = data.size ();
            out.write (reinterpret_cast<const char*> (&num_pts), sizeof (num_pts));

            // Write size
            const std::vector<byte_t>& compressed_buf = writer.get_buffer ();
            size_t compressed_bytes = compressed_buf.size ();
            out.write (reinterpret_cast<const char*> (&compressed_bytes), sizeof (compressed_bytes));

            // Write gorilla
            out.write (reinterpret_cast<const char*> (&compressed_bytes), sizeof (compressed_bytes));

            size_t raw_size = num_pts * sizeof (data);
            double ratio = (static_cast<double> (compressed_bytes) / raw_size) * 100.0;
            
            if (debug)
                std::cout << "[Flush] Tag: " << tag <<
                             " Ratio: " << ratio << "%" << std::endl;
        }

        if (debug)
            std::cout << std::endl;
            
        out.close ();
    }

    /**
     * Get content overview
     */
    void print (std::ostream& out = std::cout) const
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