#include "httplib.h"
#include <iostream>
#include "memtable.h"
#include "wal.h"
#include <sstream>
#include <filesystem>
#include <regex>
#include <set>
#include "types.h"
#include "tsdb_config.h"

using namespace config;

/**
 * Get next batch id to write
 */
size_t get_next_batch_id (const std::string& dir = sstable_dir)
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
    
    std::atomic<size_t> batch_id;

    std::atomic<bool> running {true};
    std::thread debug_thread;
    std::thread flush_thread;

    /**
     * Debug thread
     */
    void start_debug_thread ()
    {
        std::cout << "\033[H\033[J" << "=== LIVE CONSOLE ===\n";

        debug_thread = std::thread ([this] ()
        {
            while (running.load ())
            {
                std::this_thread::sleep_for (std::chrono::seconds (1));
                // Clear terminal and print to screen
                mem_db.print ();
                std::cout << std::endl;
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
            while (running.load ())
            {
                // flush at ~1MB
                if ((mem_db.get_total_count () * sizeof (Data)) < memtable_bytes)
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds {100});
                    continue;
                }

                table_t data = mem_db.extract ();
                size_t cur_id = batch_id.fetch_add (1);

                if (debug)
                    std::cout << "Flushing batch " << cur_id << "..." << std::endl;

                mem_db.flush (data, cur_id);
                wal.reset ();
            }
        });

        if (debug)
            std::cout << "Flusher Initialized" << std::endl;
    }

    /**
     * Search an sstable for tag
     */
    std::vector<Data> search_sstable (const std::string& path, const tag_t& tag) const
    {
        std::ifstream in (path, std::ios::binary);

        while (in.peek () != EOF)
        {
            // Read tag
            size_t tag_len;
            in.read (reinterpret_cast<char*> (&tag_len), sizeof (tag_len));
            std::string cur_tag (tag_len, ' ');
            in.read (&cur_tag[0], tag_len);

            // Read # points
            size_t num_pts;
            in.read (reinterpret_cast<char*> (&num_pts), sizeof (num_pts));

            // Read size
            size_t comp_size;
            in.read (reinterpret_cast<char*> (&comp_size), sizeof (comp_size));

            // If found tag, read all points into result
            if (cur_tag == tag)
            {
                std::vector<byte_t> compressed_data (comp_size);
                in.read (reinterpret_cast<char*> (compressed_data.data ()), comp_size);

                Gorilla gorilla;
                return gorilla.decode (compressed_data, num_pts);
            }
            else
            {
                // Jump over this block to next tag
                in.seekg (comp_size, std::ios::cur);
            }
        }

        return {};
    }

public:
    /**
     * Default constructor
     */
    TSDBServer () : server (), mem_db (), wal (),
                    batch_id {get_next_batch_id ()}
    {
        wal.recover (mem_db);
    }

    /**
     * Init db server endpoint
     */
    bool init_endpoint ()
    {
        // Build write endpoint
        server.Post ("/write", [&] (const httplib::Request& req, 
                                          httplib::Response& res)
        {
            res.set_header("Access-Control-Allow-Origin", "*");
            
            std::stringstream package (req.body);
            std::string part;
            std::vector<std::string> parts;
            while (std::getline (package, part, ','))
                parts.push_back (part);

            // Expect device_tag, timestamp, value
            if (parts.size () != 3)
                res.status = httplib::StatusCode::BadRequest_400;
            else
            {
                // Parse package
                std::string tag = parts[0];
                time_t time_ms = std::stoll (parts[1]);
                data_t val = std::stod (parts[2]);

                // Write to disk for durability
                wal.append (tag, time_ms, val);
                
                // Write to memory for availability
                mem_db.insert (tag, time_ms, val);
                res.set_content ("OK", "text/plain");
            }
        });

        // Build read endpoint
        server.Get ("/read", [&] (const httplib::Request& req, 
                                        httplib::Response& res)
        {
            res.set_header("Access-Control-Allow-Origin", "*");

            // TODO: start and end time
            std::string tag = req.get_param_value ("tag");
            std::vector<Data> results = mem_db.get_data (tag);

            // Scan all files from db (TODO: optimize)
            // for (int i = 0; i < batch_id.load (); ++i)
            // {
            //     std::string path = get_sstable_path (std::to_string (i));
            //     std::vector<Data> disk_data = search_sstable (path, tag);
            //     results.insert (results.end (), disk_data.begin (), disk_data.end ());
            // }

            // Format as json
            std::ostringstream oss;
            oss << "[\n";
            
            for (size_t i = 0; i < results.size (); ++i)
            {
                oss << "    {\"ts\": " << results[i].time_ms << ",\n"
                    << "     \"val\": " << std::fixed << std::setprecision (2) << results[i].value << "\n"
                    << "    }";
                
                // Don't add a comma after the last element
                if (i < results.size() - 1)
                    oss << ",";

                oss << "\n";
            }
            
            oss << "]";
            
            res.set_content (oss.str (), "application/json");
        });

        // Build tags endpoint to list all available tags
        server.Get ("/tags", [&] (const httplib::Request& req, 
                                        httplib::Response& res)
        {
            res.set_header("Access-Control-Allow-Origin", "*");

            // Get tags from memtable
            std::set<std::string> tags;
            
            // Get tags from memory
            const std::set<std::string> mem_tags = mem_db.get_tags ();
            for (const auto& tag : mem_tags)
                tags.insert (tag);

            // Format as json array
            std::ostringstream oss;
            oss << "[";
            bool first = true;
            for (const auto& tag : tags)
            {
                if (!first) oss << ",";
                oss << "\"" << tag << "\"";
                first = false;
            }
            oss << "]";
            
            res.set_content (oss.str (), "application/json");
        });

        return EXIT_SUCCESS;
    }

    /**
     * Starts server, blocks
     */
    bool start_server ()
    {
        if (debug)
            start_debug_thread ();

        start_flush_thread ();

        // Listen
        if (!server.listen (host, port))
        {
            std::cerr << "Error: Could not bind to port " << std::to_string (port)
                      << std::endl;
            return EXIT_FAILURE;
        }
        std::cout << "TimeseriesDB started at http://" << host << ":"
                  << std::to_string (port) << std::endl;

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