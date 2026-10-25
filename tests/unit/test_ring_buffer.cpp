#include <catch2/catch_test_macros.hpp>
#include "history/ring_buffer.hpp"

using namespace nerv;

TEST_CASE("RingBuffer basic operations", "[history][ring_buffer]") {
    RingBuffer<float, 5> buffer;

    REQUIRE(buffer.empty());
    REQUIRE(buffer.size() == 0);
    REQUIRE(buffer.capacity() == 5);

    buffer.push(10.0f);
    buffer.push(20.0f);
    buffer.push(30.0f);

    REQUIRE_FALSE(buffer.empty());
    REQUIRE(buffer.size() == 3);
    REQUIRE(buffer.oldest() == 10.0f);
    REQUIRE(buffer.newest() == 30.0f);

    REQUIRE(buffer[0] == 10.0f);
    REQUIRE(buffer[1] == 20.0f);
    REQUIRE(buffer[2] == 30.0f);

    SECTION("Buffer wrap-around beyond capacity") {
        buffer.push(40.0f);
        buffer.push(50.0f);
        REQUIRE(buffer.full());
        REQUIRE(buffer.size() == 5);

        // Pushing 6th element evicts 10.0f
        buffer.push(60.0f);
        REQUIRE(buffer.size() == 5);
        REQUIRE(buffer.oldest() == 20.0f);
        REQUIRE(buffer.newest() == 60.0f);

        REQUIRE(buffer[0] == 20.0f);
        REQUIRE(buffer[1] == 30.0f);
        REQUIRE(buffer[2] == 40.0f);
        REQUIRE(buffer[3] == 50.0f);
        REQUIRE(buffer[4] == 60.0f);
    }

    SECTION("Clear resets count") {
        buffer.clear();
        REQUIRE(buffer.empty());
        REQUIRE(buffer.size() == 0);
    }
}
