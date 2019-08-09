#include "catch.hpp"

// mapnik-vector-tile
#include "vector_tile_processor.hpp"

// mapnik
#include <mapnik/load_map.hpp>

// test utils
#include "decoding_util.hpp"
#include "test_utils.hpp"

// libprotobuf
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "vector_tile.pb.h"
#pragma GCC diagnostic pop

// std
#include <set>


TEST_CASE("get simplification distance value from the mapnik layer")
{
    const std::string style(R"xxx(
        <Map srs="+init=epsg:3857">
            <Layer name="line" buffer-size="64">
                <Datasource>
                    <Parameter name="type">geojson</Parameter>
                    <Parameter name="mvt_simplify_distance">10</Parameter>
                    <Parameter name="inline">
                        {"type":"Polygon","coordinates":[[
                            [0.0, 10000000.0],
                            [1564344.6504, 9876883.40595],
                            [3090169.94375, 9510565.16295],
                            [4539904.9974, 8910065.24188],
                            [5877852.52292, 8090169.94375],
                            [7071067.81187, 7071067.81187],
                            [8090169.94375, 5877852.52292],
                            [8910065.24188, 4539904.9974],
                            [9510565.16295, 3090169.94375],
                            [9876883.40595, 1564344.6504],
                            [10000000.0, 6.12323399574e-10],
                            [9876883.40595, -1564344.6504],
                            [9510565.16295, -3090169.94375],
                            [8910065.24188, -4539904.9974],
                            [8090169.94375, -5877852.52292],
                            [7071067.81187, -7071067.81187],
                            [5877852.52292, -8090169.94375],
                            [4539904.9974, -8910065.24188],
                            [3090169.94375, -9510565.16295],
                            [1564344.6504, -9876883.40595],
                            [1.22464679915e-09, -10000000.0],
                            [-1564344.6504, -9876883.40595],
                            [-3090169.94375, -9510565.16295],
                            [-4539904.9974, -8910065.24188],
                            [-5877852.52292, -8090169.94375],
                            [-7071067.81187, -7071067.81187],
                            [-8090169.94375, -5877852.52292],
                            [-8910065.24188, -4539904.9974],
                            [-9510565.16295, -3090169.94375],
                            [-9876883.40595, -1564344.6504],
                            [-10000000.0, -1.83697019872e-09],
                            [-9876883.40595, 1564344.6504],
                            [-9510565.16295, 3090169.94375],
                            [-8910065.24188, 4539904.9974],
                            [-8090169.94375, 5877852.52292],
                            [-7071067.81187, 7071067.81187],
                            [-5877852.52292, 8090169.94375],
                            [-4539904.9974, 8910065.24188],
                            [-3090169.94375, 9510565.16295],
                            [-1564344.6504, 9876883.40595],
                            [0, 10000000]
                            ]]}
                    </Parameter>
                </Datasource>
            </Layer>
        </Map>)xxx");

    mapnik::Map map(256, 256);
    mapnik::load_map_string(map, style);

    mapnik::vector_tile_impl::processor ren(map);

    mapnik::vector_tile_impl::merc_wafer wafer = ren.create_wafer(0, 0, 1, 2, 512);
    REQUIRE(wafer.span() == 2);
    REQUIRE(wafer.tiles().size() == 4);

    mapnik::vector_tile_impl::merc_tile const & tile = wafer.tile(0, 0);

    REQUIRE(tile.has_layer("line") == true);
    vector_tile::Tile mvt;
    mvt.ParseFromString(tile.get_buffer());
    REQUIRE(1 == mvt.layers_size());
    vector_tile::Tile_Layer const& layer = mvt.layers(0);
    CHECK(std::string("line") == layer.name());
    REQUIRE(1 == layer.features_size());
    vector_tile::Tile_Feature const& feature = layer.features(0);
    std::string feature_string = feature.SerializeAsString();
    mapnik::vector_tile_impl::GeometryPBF geoms = feature_to_pbf_geometry(feature_string);
    auto geom = mapnik::vector_tile_impl::decode_geometry<double>(
        geoms, feature.type(), 2, 0.0, 0.0, 1.0, 1.0);
    using Geom = mapnik::geometry::polygon<double>;
    REQUIRE(geom.is<Geom>());

    Geom const & g = geom.get<Geom>();
    REQUIRE(g.exterior_ring.size() == 11);
}

