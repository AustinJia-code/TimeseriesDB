#pragma once

#include <vector>
#include "types.h"

/**
 * Handles writing bits
 */
class BitWriter
{
private:
    std::vector<byte_t> buf;
    byte_t current_byte = 0;
    size_t bit_count = 0;

public:
    /**
     * Write a single bit
     */
    void write_bit (bool bit)
    {
        // Little endian
        if (bit)
            current_byte |= (1 << (7 - bit_count));
        
        ++bit_count;
        
        if (bit_count == 8)
        {
            buf.push_back (current_byte);
            current_byte = 0x0;
            bit_count = 0;
        }
    }

    /**
     * Write many bits
     */
    void write_bits (uint64_t value, int count)
    {
        for (int i = count - 1; i >= 0; --i)
        {
            bool bit = (value >> i) & 1;
            write_bit (bit);
        }
    }

    /**
     * Pad and flush
     */
    void flush ()
    {
        if (bit_count == 0)
            return;
        
        buf.push_back (current_byte);
        
        current_byte = 0;
        bit_count = 0;
    }

    /**
     * buf getter
     */
    const std::vector<byte_t>& get_buffer () const
    {
        return buf;
    }
};


/**
 * Handles reading bits
 */
class BitReader
{
private:
    const std::vector<byte_t>& buf;
    size_t byte_pos = 0;
    int bit_pos = 0;

public:
    /**
     * Buffer constructor
     */
    BitReader (const std::vector<byte_t>& buf) : buf (buf) {}

    /**
     * Read bit
     */
    bool read_bit ()
    {
        if (byte_pos >= buf.size ())
            return false;
        
        bool bit = (buf[byte_pos] >> (7 - bit_pos++)) & 1;
        
        if (bit_pos == 8)
        {
            bit_pos = 0;
            ++byte_pos;
        }

        return bit;
    }

    /**
     * Read many bits
     */
    uint64_t read_bits (size_t count)
    {
        uint64_t value = 0;
        for (int i = 0; i < count; ++i)
            value = (value << 1) | read_bit ();
        
        return value;
    }
};