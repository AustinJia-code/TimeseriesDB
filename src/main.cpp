#include <httplib.h>
#include <iostream>
#include "memtable.h"
#include "wal.h"
#include <sstream>
#include <filesystem>
#include <regex>
#include "types.h"

/**
 * Get next batch id to write
 */
size_t get_next_batch_id (const std::string& dir)
{
    size_t max_id = 0;
    std::regex re ("sstable_(\\d+)\\.db");
    std::smatch match;

    for (const auto& entry : std::filesystem::directory_iterator (dir))
    {
        std::string filename = entry.path ().filename ().string ();
        
        if (std::regex_search (filename, match, re))
            max_id = std::max (max_id, static_cast<size_t> (std::stoi (match[1])));
    }

    return max_id + 1;
}

/**
 * Init db server endpoint
 */
bool init_endpoint (httplib::Server& server, MemTable& mem_db, WAL& wal)
{
    // Build endpoint
    server.Post ("/write", [&] (const httplib::Request& req, httplib::Response& res)
    {
        std::stringstream package (req.body);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline (package, part, ','))
            parts.push_back (part);

        // Expect device_id, timestamp, value
        if (parts.size () != 3)
            res.status = httplib::StatusCode::BadRequest_400;
        else
        {
            // Parse package
            std::string tag = "device_" + parts[0];
            time_t time_ms = std::stoll (parts[1]);
            data_t val = std::stod (parts[2]);

            // Write to disk for durability
            wal.append (tag, time_ms, val);
            
            // Write to memory for availability
            mem_db.insert (tag, time_ms, val);
            res.set_content ("OK", "text/plain");
        }
    });

    return EXIT_SUCCESS;
}

/**
 * Start server
 */
bool start_server (httplib::Server& server, MemTable& mem_db, WAL& wal)
{
    // Debug route
    std::thread debug_thread ([&mem_db, &server] ()
    {
        while (true)
        {
            std::this_thread::sleep_for (std::chrono::seconds (1));
            // Clear terminal and print to your screen
            std::cout << "\033[H\033[J" << "=== LIVE CONSOLE ===\n";
            mem_db.print ();
        }
    });
    std::cout << "Debugging to console" << std::endl;

    // Flusher
    std::thread flush_thread ([&mem_db, &wal] ()
    {
        std::atomic<size_t> batch_id = get_next_batch_id ("../disk/sstables");
        while (true)
        {
            // flush at ~1MB
            if ((mem_db.get_total_count () * sizeof (Data)) < (2 << 20))
            {
                std::this_thread::sleep_for (std::chrono::milliseconds {100});
                continue;
            }

            table_t data = mem_db.extract ();
            size_t cur_id = batch_id.fetch_add (1);

            std::cout << "Flushing batch " << cur_id << "..." << std::endl;

            mem_db.flush (data, cur_id);
            wal.reset ();
        }
    });
    std::cout << "Flusher Initialized" << std::endl;

    // Listen
    if (!server.listen ("0.0.0.0", 9090))
    {
        std::cerr << "Error: Could not bind to port 9090" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "TimeseriesDB started at http://0.0.0.0:9090" << std::endl;

    return EXIT_SUCCESS;
}

/**
 * Main server
 */
int main ()
{
    httplib::Server server;
    MemTable mem_db;
    WAL wal ("../disk/data.wal");
    wal.recover (mem_db);

    if (init_endpoint (server, mem_db, wal) == EXIT_FAILURE)
    {   
        std::cerr << "Failed to initialize endpoint" << std::endl;
        return EXIT_FAILURE;
    }

    if (start_server (server, mem_db, wal) == EXIT_FAILURE)
    {
        std::cerr << "Failed to start server" << std::endl;
        return EXIT_FAILURE;
    }
        
    return EXIT_SUCCESS;
}