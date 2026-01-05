#pragma once
#include <cstdint>
#include <ctime>
#include <map>
#include <string>
#include <iostream>

using tag_t     = std::string;
using id_t      = uint32_t;
using data_t    = double;
using byte_t    = uint8_t;

/**
 * Single unit of timeseries data
 */
struct Data
{
    time_t time_ms;
    data_t value;

    void print (std::ostream& os = std::cout) const
    {
        os << "Time: " << time_ms << "\n"
           << "Value: " << value << std::endl;
    }
};

using table_t   = std::map<std::string, std::vector<Data>>;
