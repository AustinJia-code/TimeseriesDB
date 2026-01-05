#pragma once

#include <string>
#include <format>

// Turns on print debugging
static constexpr bool debug                 (true);

static std::string wal_path                 ("../disk/wal.wal");

static std::string sstable_dir              ("../disk/sstables/");
static std::string sstable_path             (sstable_dir + "sstable_");

// # bytes before WAL flush
static constexpr size_t memtable_bytes =    1 * (1 << 20);

const std::string get_sstable_path (const std::string id)
{
    return sstable_path + id + ".db";
}