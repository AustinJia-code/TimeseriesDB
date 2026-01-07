#include "gorilla.h"
#include <iostream>
#include "types.h"

void test_gorilla_logic ()
{
    Gorilla gorilla;
    std::vector<Data> original =
    {
        {1000, 25.5}, {1100, 25.6}, {1200, 25.6}, {1300, 25.7}
    };

    // Encode
    BitWriter writer;
    gorilla.encode (original, writer);
    writer.flush ();

    // Decode
    std::vector<Data> decoded = gorilla.decode (writer.get_buffer (),
                                                original.size ());

    // Compare
    std::vector<size_t> mismatches;
    for (size_t i = 0; i < original.size (); ++i)
    {
        std::cout << "Original: " << std::endl;
        original[i].print ();
        std::cout << "Decoded: " << std::endl;
        decoded[i].print ();

        if (original[i].time_ms != decoded[i].time_ms || 
            original[i].value != decoded[i].value)
            mismatches.push_back (i);

        std::cout << std::endl;
    }

    if (mismatches.size () > 0)
        std::cerr << "FAIL: " << mismatches.size () << " mismatches" << std::endl;
    else
        std::cout << "SUCCESS: Gorilla round-trip perfect!" << std::endl;
}

void test_cold_store ()
{
    std::cout << "FAIL" << std::endl;
}

void test_mem_get ()
{
    std::cout << "FAIL" << std::endl;
}

void test_cold_get ()
{
    std::cout << "FAIL" << std::endl;
}

int main ()
{
    test_gorilla_logic ();
    test_cold_store ();
    test_mem_get ();
    test_cold_get ();

    return EXIT_SUCCESS;
}