#pragma once
#include <vector>
#include "types.h"
#include <cstring>
#include "bit_buffer.h"

/**
 * Handles compression
 * https://www.vldb.org/pvldb/vol8/p1816-teller.pdf 
 */
class Gorilla
{   
private:
    // Timestamp vars
    size_t delta_bits {14};
    size_t max_dod_bits {7};
    int64_t max_dod {64};
    int64_t min_dod {-63};

    // Value vars
    uint64_t last_val_bits = 0;
    uint32_t last_leading_zeros = 0xFFFFFFFF;
    uint32_t last_meaningful_len = 0;

public:
    /**
     * Reset the gorilla :D
     */
    void reset ()
    {
        last_val_bits = 0;
        last_leading_zeros = 0xFFFFFFFF;
        last_meaningful_len = 0;
    }

    /**
     * Write floating point values, compressed with xor
     */
    void write_value (data_t value, BitWriter& out)
    {
        uint64_t val_bits;
        std::memcpy (&val_bits, &value, sizeof (data_t));

        uint64_t xor_val = val_bits ^ last_val_bits;
        if (xor_val == 0)
            out.write_bit (0);
        else
        {
            out.write_bit (1);
            uint32_t leading_zeros = __builtin_clzll (xor_val);
            uint32_t trailing_zeros = __builtin_ctzll (xor_val);
            uint32_t meaningful_len = 64 - leading_zeros - trailing_zeros;

            // If meaningful len is same as last, shortcut
            if (leading_zeros >= last_leading_zeros &&
                trailing_zeros >= (64 - last_leading_zeros - last_meaningful_len) &&
                last_leading_zeros != 0xFFFFFFFF)
            {
                out.write_bit (0);
                uint32_t last_trailing_zeros = 64 - last_leading_zeros - last_meaningful_len;
                out.write_bits (xor_val >> last_trailing_zeros, last_meaningful_len);
            }
            else
            {
                out.write_bit (1);
                out.write_bits (leading_zeros, 5);
                out.write_bits (meaningful_len, 6);
                out.write_bits (xor_val >> trailing_zeros, meaningful_len);

                last_leading_zeros = leading_zeros;
                last_meaningful_len = meaningful_len;
            }

            last_val_bits = val_bits;
        }
    }

    /**
     * Encode w/ timestamp delta of delta compression
     */
    void encode (const std::vector<Data>& points, BitWriter& out)
    {
        if (points.empty ())
            return;

        reset ();

        // First point, write verbose
        out.write_bits (points[0].time_ms, 64);
        uint64_t first_val_bits;
        std::memcpy (&first_val_bits, &points[0].value, sizeof (data_t));
        out.write_bits (first_val_bits, 64);
        last_val_bits = first_val_bits;

        if (points.size () < 2)
            return;
        
        // Write second point
        int64_t last_delta = points[1].time_ms - points[0].time_ms;
        out.write_bits (static_cast<uint64_t> (last_delta), delta_bits);
        write_value (points[1].value, out);

        // Write rest
        for (size_t i = 2; i < points.size (); ++i)
        {
            int64_t delta = points[i].time_ms - points[ i -1].time_ms;
            int64_t dod = delta - last_delta;

            if (dod == 0)
                out.write_bit (0);
            else if (dod >= min_dod && dod <= max_dod)
            {
                out.write_bits (0b10, 2);
                out.write_bits (static_cast<uint64_t> (dod + 63), max_dod_bits);
            }
            else
            {
                out.write_bits (0b11, 2);
                out.write_bits (static_cast<uint64_t> (dod), 32);
            }

            write_value (points[i].value, out);
            last_delta = delta;
        }
    }

    /**
     * Decode
     */
    std::vector<Data> decode (const std::vector<byte_t>& compressed_data, size_t num_points)
    {
        if (num_points == 0)
            return {};
        
        BitReader reader (compressed_data);
        std::vector<Data> points;

        // Recover first full data point
        time_t last_ts = static_cast<size_t> (reader.read_bits (sizeof (size_t) * 8));
        uint64_t last_val_bits = reader.read_bits (64);

        data_t last_val;
        std::memcpy (&last_val, &last_val_bits, sizeof (data_t));
        points.push_back (Data {last_ts, last_val});

        if (num_points == 1)
            return points;
        
        // Recover second
        int64_t last_delta = static_cast<int64_t> (reader.read_bits (delta_bits));

        last_leading_zeros = 0xFFFFFFFF;
        last_meaningful_len = 0;

        for (size_t i = 1; i < num_points; ++i)
        {
            /* TIMESTAMP DECODING */
            // Control bit 1
            if (i > 1 && reader.read_bit ())
            {
                int64_t dod = 0;
                // '10', 7 bit DOD
                if (reader.read_bit () == 0)
                    dod = static_cast<int64_t> (reader.read_bits (7)) - 63;
                // '11', 32 bit DOd
                else
                    dod = static_cast<int64_t> (reader.read_bits (32));
                
                last_delta += dod;
            }

            last_ts += last_delta;

            /* VALUE DECODING */
            // Value changed
            if (reader.read_bit ())
            {
                // '11', New window
                if (reader.read_bit ())
                {
                    last_leading_zeros = static_cast<uint32_t> (reader.read_bits (5));
                    last_meaningful_len = static_cast<uint32_t> (reader.read_bits (6));
                }

                uint64_t bits = reader.read_bits (last_meaningful_len);
                uint64_t xor_val = bits << (64 - last_leading_zeros - last_meaningful_len);
                last_val_bits ^= xor_val;
            }

            std::memcpy (&last_val, &last_val_bits, sizeof (data_t));
            points.push_back (Data {last_ts, last_val});
        }

        return points;
    }
};