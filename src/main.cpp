#include <httplib.h>
#include <iostream>
#include "memtable.h"
#include "wal.h"
#include <sstream>

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
bool start_server (httplib::Server& server, MemTable& mem_db)
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

    if (!start_server (server, mem_db) == EXIT_FAILURE)
    {
        std::cerr << "Failed to start server" << std::endl;
        return EXIT_FAILURE;
    }
        
    return EXIT_SUCCESS;
}