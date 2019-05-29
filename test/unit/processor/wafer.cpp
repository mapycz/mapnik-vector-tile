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
#include "decoding_util.hpp"
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
#include <fstream>

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
    
    mapnik::vector_tile_impl::merc_wafer wafer = ren.create_wafer(0, 0, 3, 8, 4096, 64);
    CHECK(wafer.span() == 8);
    REQUIRE(wafer.tiles().size() == 64);

    const mapnik::box2d<double> expected_extent(
        -20037508.3427892439, -20037508.3427892439,
         20037508.3427892439,  20037508.3427892439);
    CHECK(wafer.extent().minx() == Approx(expected_extent.minx()));
    CHECK(wafer.extent().miny() == Approx(expected_extent.miny()));
    CHECK(wafer.extent().maxx() == Approx(expected_extent.maxx()));
    CHECK(wafer.extent().maxy() == Approx(expected_extent.maxy()));

    CHECK(wafer.tile_size() == 4096 * 8);
    CHECK(wafer.buffer_size() == 64);
    CHECK(wafer.has_layer("polygon") == true);

    const std::set<std::size_t> non_empty_tile_indices {
        3 * 8 + 3, 3 * 8 + 4,
        4 * 8 + 3, 4 * 8 + 4 };
    std::size_t index = 0;

    for (auto const & tile : wafer.tiles())
    {
        if (non_empty_tile_indices.count(index))
        {
            CHECK(tile.has_layer("polygon") == true);
            vector_tile::Tile mvt;
            mvt.ParseFromString(tile.get_buffer());
            REQUIRE(1 == mvt.layers_size());
            vector_tile::Tile_Layer const& layer = mvt.layers(0);
            CHECK(std::string("polygon") == layer.name());
            REQUIRE(1 == layer.features_size());
            vector_tile::Tile_Feature const& feature = layer.features(0);
            std::string feature_string = feature.SerializeAsString();
            mapnik::vector_tile_impl::GeometryPBF geoms = feature_to_pbf_geometry(feature_string);
            auto geom = mapnik::vector_tile_impl::decode_geometry<double>(
                geoms, feature.type(), 2, 0.0, 0.0, 1.0, 1.0);
            CHECK(geom.is<mapnik::geometry::polygon<double>>());
        }
        else
        {
            CHECK(tile.has_layer("polygon") == false);
        }
        ++index;
    }

    mapnik::vector_tile_impl::merc_tile const & wafer_tile = wafer.tile(3, 3);
    std::ofstream of("/tmp/test.mvt", std::ios::binary);
    of << wafer_tile.get_buffer();
    vector_tile::Tile tile;
    tile.ParseFromString(wafer_tile.get_buffer());
    REQUIRE(1 == tile.layers_size());
    vector_tile::Tile_Layer const& layer = tile.layers(0);
    CHECK(std::string("polygon") == layer.name());
    REQUIRE(1 == layer.features_size());
    vector_tile::Tile_Feature const& f = layer.features(0);
    CHECK(static_cast<mapnik::value_integer>(1) == static_cast<mapnik::value_integer>(f.id()));
    //REQUIRE(3 == f.geometry_size());

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


