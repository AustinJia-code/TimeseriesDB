#pragma once
#include <cstdint>

using id_t      = uint32_t;
using data_t    = double;

/**
 * Single unit of timeseries data
 */
struct Data
{
    time_t time_ms;
    data_t value;
};

using table_t   = std::map<std::string, std::vector<Data>>;
