#include <catch2/catch_test_macros.hpp>
#include "model/process_graph.hpp"

using namespace nerv;

TEST_CASE("GraphFilter search matching and resource cutoffs", "[model][graph_filter]") {
    ProcessMetrics p1;
    p1.identity = {1234, 100ULL};
    p1.name = "chrome.exe";
    p1.cpu_percent = 15.0f;
    p1.resident_memory_bytes = 450ULL * 1024 * 1024; // 450 MB
    p1.is_accessible = true;
    p1.category = ProcessCategory::UserInteractive;

    ProcessMetrics p2;
    p2.identity = {5678, 200ULL};
    p2.name = "svchost.exe";
    p2.cpu_percent = 1.0f;
    p2.resident_memory_bytes = 25ULL * 1024 * 1024; // 25 MB
    p2.is_accessible = true;
    p2.category = ProcessCategory::SystemService;

    SECTION("Search by name substring (case-insensitive)") {
        GraphFilter f;
        f.search_query = "CHROME";
        REQUIRE(f.matches(p1));
        REQUIRE_FALSE(f.matches(p2));
    }

    SECTION("Search by PID") {
        GraphFilter f;
        f.search_query = "5678";
        REQUIRE_FALSE(f.matches(p1));
        REQUIRE(f.matches(p2));
    }

    SECTION("Filter by minimum CPU percentage") {
        GraphFilter f;
        f.min_cpu_percent = 10.0f;
        REQUIRE(f.matches(p1));
        REQUIRE_FALSE(f.matches(p2));
    }

    SECTION("Filter by minimum RAM in MB") {
        GraphFilter f;
        f.min_ram_mb = 100.0f;
        REQUIRE(f.matches(p1));
        REQUIRE_FALSE(f.matches(p2));
    }
}
