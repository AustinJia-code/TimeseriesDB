#pragma once

#include <fstream>
#include <string>
#include <mutex>
#include "memtable.h"
#include "types.h"

/**
 * Write ahead log for memtable persistence
 */
class WAL
{
private:
    std::string path;
    std::ofstream file;
    std::mutex write_lock;

public:
    /**
     * Open path as binary append mode
     */
    WAL (const std::string& path) : path (path)
    {
        file.open (path, std::ios::binary | std::ios::app);
        
        if (!file.is_open ())
        {
            std::cerr << "Could not open WAL file at " << path << std::endl;
            perror ("Reason");
            return;
        }
    }

    /**
     * Write raw bytes to disk
     */
    void append (std::string tag, time_t time_ms, data_t val)
    {
        std::lock_guard<std::mutex> lock (write_lock);
        if (!file.is_open ())
            return;

        // Write
        size_t tag_len = tag.size ();
        file.write (reinterpret_cast<const char*> (&tag_len), sizeof (tag_len));
        file.write (tag.c_str (), tag_len);
        file.write (reinterpret_cast<const char*> (&time_ms), sizeof (time_ms));
        file.write (reinterpret_cast<const char*> (&val), sizeof (val));

        // Flush buffer
        file.flush ();
    }

    /**
     * Recover WAL into mem_db
     */
    void recover (MemTable& mem_db) const
    {
        std::ifstream reader (path);
        if (!reader)
        {
            std::cout << "No WAL found." << std::endl;
            return;
        }

        while (reader.peek () != EOF)
        {
            size_t tag_len;

            if (!reader.read (reinterpret_cast<char*> (&tag_len), sizeof (tag_len)))
                break;
            
            // Check tag_len before allocating massive memory
            if (tag_len > 1024)
            {
                std::cerr << "Unreasonable tag length detected: " << tag_len 
                          << std::endl;
                break; 
            }

            std::string tag (tag_len, ' ');
            reader.read (&tag[0], tag_len);

            time_t time_ms;
            if (!reader.read (reinterpret_cast<char*> (&time_ms), sizeof (time_ms)))
                break;

            data_t val;
            if (!reader.read (reinterpret_cast<char*> (&val), sizeof (val)))
                break;

            mem_db.insert (tag, time_ms, val);
        }

        std::cout << "Recovered data from " << path << std::endl;
    }

    /**
     * Reset WAL to empty
     */
    void reset ()
    {
        std::lock_guard<std::mutex> lock (write_lock);
        if (file.is_open ())
            file.close ();
        
        file.open (path, std::ios::binary | std::ios::out | std::ios::trunc);

        if (!file.is_open ())
            std::cerr << "Failed to reset WAL" << std::endl;
    }

    /**
     * Destructor
     */
    ~WAL ()
    {
        // No need for lock
        if (file.is_open ())
        {
            file.flush ();
            file.close ();
        }
    }
};