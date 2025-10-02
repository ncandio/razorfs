#include "../src/razorfs_persistence.hpp"
#include "../src/razorfs_errors.hpp"
#include <cassert>
#include <iostream>
#include <cstring>

using namespace razorfs;

void test_basic_intern() {
    std::cout << "Testing basic string interning..." << std::endl;
    StringTable table;

    uint32_t offset1 = table.intern_string("hello");
    uint32_t offset2 = table.intern_string("hello");

    assert(offset1 == offset2 && "Same string should have same offset");
    assert(table.get_string(offset1) == "hello");

    std::cout << "  ✓ Basic intern works" << std::endl;
}

void test_multiple_strings() {
    std::cout << "Testing multiple strings..." << std::endl;
    StringTable table;

    uint32_t offset1 = table.intern_string("foo");
    uint32_t offset2 = table.intern_string("bar");
    uint32_t offset3 = table.intern_string("baz");

    assert(offset1 != offset2 && "Different strings should have different offsets");
    assert(offset2 != offset3);
    assert(table.get_string(offset1) == "foo");
    assert(table.get_string(offset2) == "bar");
    assert(table.get_string(offset3) == "baz");

    std::cout << "  ✓ Multiple strings work" << std::endl;
}

void test_invalid_offset() {
    std::cout << "Testing invalid offset..." << std::endl;
    StringTable table;
    table.intern_string("test");

    bool caught = false;
    try {
        table.get_string(9999);
    } catch (const StringTableException& e) {
        caught = true;
        std::cout << "  ✓ Caught exception: " << e.what() << std::endl;
    }

    assert(caught && "Should throw on invalid offset");
}

void test_empty_string() {
    std::cout << "Testing empty string..." << std::endl;
    StringTable table;

    bool caught = false;
    try {
        table.intern_string("");
    } catch (const StringTableException& e) {
        caught = true;
        std::cout << "  ✓ Caught exception: " << e.what() << std::endl;
    }

    assert(caught && "Should throw on empty string");
}

void test_save_and_load() {
    std::cout << "Testing save and load..." << std::endl;
    StringTable table1;

    table1.intern_string("first");
    table1.intern_string("second");
    table1.intern_string("third");

    const auto& data = table1.get_data();

    StringTable table2;
    table2.load_from_data(data.data(), data.size());

    assert(table2.get_string(table1.intern_string("first")) == "first");
    assert(table2.get_string(table1.intern_string("second")) == "second");
    assert(table2.get_string(table1.intern_string("third")) == "third");

    std::cout << "  ✓ Save and load works" << std::endl;
}

void test_corrupted_data() {
    std::cout << "Testing corrupted data..." << std::endl;
    StringTable table;

    // Data without null terminator
    const char bad_data[] = {'h', 'e', 'l', 'l', 'o'};

    bool caught = false;
    try {
        table.load_from_data(bad_data, sizeof(bad_data));
    } catch (const CorruptionError& e) {
        caught = true;
        std::cout << "  ✓ Caught corruption: " << e.what() << std::endl;
    }

    assert(caught && "Should throw on corrupted data");
}

void test_long_string() {
    std::cout << "Testing very long string..." << std::endl;
    StringTable table;

    std::string long_str(5000, 'x');  // Exceeds MAX_STRING_LENGTH

    bool caught = false;
    try {
        table.intern_string(long_str);
    } catch (const StringTableException& e) {
        caught = true;
        std::cout << "  ✓ Caught exception: " << e.what() << std::endl;
    }

    assert(caught && "Should throw on string too long");
}

int main() {
    std::cout << "==================================" << std::endl;
    std::cout << "String Table Tests" << std::endl;
    std::cout << "==================================" << std::endl;

    try {
        test_basic_intern();
        test_multiple_strings();
        test_invalid_offset();
        test_empty_string();
        test_save_and_load();
        test_corrupted_data();
        test_long_string();

        std::cout << "\n==================================" << std::endl;
        std::cout << "✅ ALL TESTS PASSED" << std::endl;
        std::cout << "==================================" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
