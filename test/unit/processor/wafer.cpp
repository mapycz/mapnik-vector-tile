#include "catch.hpp"

// mapnik-vector-tile
#include "vector_tile_datasource_pbf.hpp"
#include "vector_tile_processor.hpp"

// mapnik
#include <mapnik/agg_renderer.hpp>
#include <mapnik/feature_factory.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/image_util.hpp>
#include <mapnik/util/fs.hpp>

// test utils
#include "test_utils.hpp"

// libprotobuf
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "vector_tile.pb.h"
#pragma GCC diagnostic pop

// boost
#include <boost/optional/optional_io.hpp>

// std
#include <set>

TEST_CASE("vector wafer output")
{
    const std::string style(R"xxx(
        <Map srs="+init=epsg:3857">
            <Layer name="polygon" srs="+init=epsg:4326">
                <Datasource>
                    <Parameter name="type">geojson</Parameter>
                    <Parameter name="inline">
                        {"type":"Polygon","coordinates":[[
                            [ 10,  10],
                            [-10,  10],
                            [-10, -10],
                            [ 10, -10],
                            [ 10,  10]
                        ]]}
                    </Parameter>
                </Datasource>
            </Layer>
        </Map>)xxx");

    mapnik::Map map(256, 256);
    mapnik::load_map_string(map, style);
    
    mapnik::vector_tile_impl::processor ren(map);
    
    mapnik::vector_tile_impl::merc_wafer wafer = ren.create_wafer(0, 0, 3, 8);
    CHECK(wafer.span() == 8);
    CHECK(wafer.tiles().size() == 64);


    /*
    // Now check that the tile is correct.
    vector_tile::Tile tile;
    tile.ParseFromString(out_tile.get_buffer());
    REQUIRE(1 == tile.layers_size());
    vector_tile::Tile_Layer const& layer = tile.layers(0);
    CHECK(std::string("layer") == layer.name());
    REQUIRE(2 == layer.features_size());
    vector_tile::Tile_Feature const& f = layer.features(0);
    CHECK(static_cast<mapnik::value_integer>(1) == static_cast<mapnik::value_integer>(f.id()));
    REQUIRE(3 == f.geometry_size());
    CHECK(9 == f.geometry(0));
    CHECK(4096 == f.geometry(1));
    CHECK(4096 == f.geometry(2));
    CHECK(190 == tile.ByteSize());
    std::string buffer;
    CHECK(tile.SerializeToString(&buffer));
    CHECK(190 == buffer.size());
    */
}


