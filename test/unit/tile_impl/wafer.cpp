#include "catch.hpp"
#include <memory>

#include "vector_tile_wafer.hpp"

// mapnik
#include <mapnik/feature.hpp>
#include <mapnik/util/file_io.hpp>
#include <mapnik/json/feature_parser.hpp>

// libprotobuf
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "vector_tile.pb.h"
#pragma GCC diagnostic pop

TEST_CASE("Vector tile wafer class")
{
    mapnik::box2d<double> global_extent(-20037508.342789,-20037508.342789,20037508.342789,20037508.342789);

    SECTION("2x2")
    {
        mapnik::vector_tile_impl::merc_wafer wafer(0, 0, 1, 2, 1024, 20);

        CHECK(wafer.span() == 2);
        REQUIRE(wafer.tiles().size() == 4);

        {
            mapnik::vector_tile_impl::merc_tile const & tile = wafer.tiles()[0];
            CHECK(tile.x() == 0);
            CHECK(tile.y() == 0);
            CHECK(tile.z() == 1);
        }

        {
            mapnik::vector_tile_impl::merc_tile const & tile = wafer.tiles()[1];
            CHECK(tile.x() == 1);
            CHECK(tile.y() == 0);
            CHECK(tile.z() == 1);
        }

        {
            mapnik::vector_tile_impl::merc_tile const & tile = wafer.tiles()[2];
            CHECK(tile.x() == 0);
            CHECK(tile.y() == 1);
            CHECK(tile.z() == 1);
        }

        {
            mapnik::vector_tile_impl::merc_tile const & tile = wafer.tiles()[3];
            CHECK(tile.x() == 1);
            CHECK(tile.y() == 1);
            CHECK(tile.z() == 1);
        }
    }

}
