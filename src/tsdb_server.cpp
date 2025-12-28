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
 * Wrapper for TSDB components
 */
class TSDBServer
{
private:
    httplib::Server server;
    MemTable mem_db;
    WAL wal;

    std::atomic<bool> running {true};
    std::thread debug_thread;
    std::thread flush_thread;

    /**
     * Debug thread
     */
    void start_debug_thread ()
    {
        debug_thread = std::thread ([this] ()
        {
            while (running.load ())
            {
                std::this_thread::sleep_for (std::chrono::seconds (1));
                // Clear terminal and print to your screen
                std::cout << "\033[H\033[J" << "=== LIVE CONSOLE ===\n";
                mem_db.print ();
            }
        });

        std::cout << "Debugging to console" << std::endl;
    }

    /**
     * WAL flusher thread
     */
    void start_flush_thread ()
    {
        flush_thread = std::thread ([this] ()
        {
            std::atomic<size_t> batch_id = get_next_batch_id ("../disk/sstables");
            while (running.load ())
            {
                // flush at ~1MB
                if ((mem_db.get_total_count () * sizeof (Data)) < (1 << 20))
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
    }

public:
    /**
     * Default constructor
     */
    TSDBServer () : server (), mem_db (), wal ("../disk/data.wal")
    {
        wal.recover (mem_db);
    }

    /**
     * Init db server endpoint
     */
    bool init_endpoint ()
    {
        // Build endpoint
        server.Post ("/write", [&] (const httplib::Request& req, 
                                          httplib::Response& res)
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
     * Starts server, blocks
     */
    bool start_server ()
    {
        start_debug_thread ();
        start_flush_thread ();

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
     * Destructor, clean up threads
     */
    ~TSDBServer ()
    {
        running.store (false);

        if (debug_thread.joinable ())
            debug_thread.join ();
        
        if (flush_thread.joinable ())
            flush_thread.join ();
    }
};


/**
 * Runner
 */
int main ()
{
    TSDBServer tsdb;

    if (tsdb.init_endpoint () == EXIT_FAILURE)
    {   
        std::cerr << "Failed to initialize endpoint" << std::endl;
        return EXIT_FAILURE;
    }

    if (tsdb.start_server () == EXIT_FAILURE)
    {
        std::cerr << "Failed to start server" << std::endl;
        return EXIT_FAILURE;
    }
        
    return EXIT_SUCCESS;
}