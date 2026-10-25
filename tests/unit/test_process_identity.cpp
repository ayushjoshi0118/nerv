#include <catch2/catch_test_macros.hpp>
#include "telemetry/process_metrics.hpp"
#include <unordered_set>

using namespace nerv;

TEST_CASE("ProcessIdentity and PID reuse resolution", "[telemetry][identity]") {
    ProcessIdentity proc1{1024, 500000000ULL};
    ProcessIdentity proc1_same{1024, 500000000ULL};
    ProcessIdentity proc2_reused_pid{1024, 900000000ULL}; // Reused PID after exit
    ProcessIdentity proc3_diff_pid{2048, 500000000ULL};

    SECTION("Equality and uniqueness") {
        REQUIRE(proc1 == proc1_same);
        REQUIRE_FALSE(proc1 == proc2_reused_pid);
        REQUIRE(proc1 != proc2_reused_pid);
        REQUIRE(proc1 != proc3_diff_pid);
    }

    SECTION("Hash table discrimination with PID reuse") {
        std::unordered_set<ProcessIdentity> set;
        set.insert(proc1);
        set.insert(proc2_reused_pid);

        // Even though both have PID 1024, they represent two distinct process incarnations!
        REQUIRE(set.size() == 2);
        REQUIRE(set.find(proc1) != set.end());
        REQUIRE(set.find(proc2_reused_pid) != set.end());
    }
}
