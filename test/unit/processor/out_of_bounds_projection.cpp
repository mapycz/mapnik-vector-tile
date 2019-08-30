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


TEST_CASE("the tile bbox is out of utm33 bounds")
{
    const std::string style(R"xxx(
        <Map srs="+init=epsg:3857">
            <Layer name="points" srs="+proj=utm +zone=33 +ellps=WGS84 +datum=WGS84 +units=m +no_defs">
                <Datasource>
                    <Parameter name="type">csv</Parameter>
                    <Parameter name="inline">
                        x, y
                        360318.4, 5529996.2
                    </Parameter>
                </Datasource>
            </Layer>
        </Map>)xxx");

    mapnik::Map map(256, 256);
    mapnik::load_map_string(map, style);

    mapnik::vector_tile_impl::processor ren(map);

    mapnik::vector_tile_impl::merc_tile tile = ren.create_tile(12, 7, 4);
    CHECK(tile.get_buffer().size() == 0);
}

TEST_CASE("the tile bbox is in the utm33 bounds")
{
    const std::string style(R"xxx(
        <Map srs="+init=epsg:3857">
            <Layer name="points" srs="+proj=utm +zone=33 +ellps=WGS84 +datum=WGS84 +units=m +no_defs">
                <Datasource>
                    <Parameter name="type">csv</Parameter>
                    <Parameter name="inline">
                        x, y
                        360318.4, 5529996.2
                    </Parameter>
                </Datasource>
            </Layer>
        </Map>)xxx");

    mapnik::Map map(256, 256);
    mapnik::load_map_string(map, style);

    mapnik::vector_tile_impl::processor ren(map);

    mapnik::vector_tile_impl::merc_tile tile = ren.create_tile(8, 5, 4);
    REQUIRE(tile.get_buffer().size() > 0);

    vector_tile::Tile mvt;
    mvt.ParseFromString(tile.get_buffer());
    REQUIRE(1 == mvt.layers_size());
    vector_tile::Tile_Layer const& layer = mvt.layers(0);
    CHECK(std::string("points") == layer.name());
    REQUIRE(1 == layer.features_size());
    vector_tile::Tile_Feature const& feature = layer.features(0);
    std::string feature_string = feature.SerializeAsString();
    mapnik::vector_tile_impl::GeometryPBF geoms = feature_to_pbf_geometry(feature_string);
    auto geom = mapnik::vector_tile_impl::decode_geometry<double>(
        geoms, feature.type(), 2, 0.0, 0.0, 1.0, 1.0);
    using Geom = mapnik::geometry::point<double>;
    REQUIRE(geom.is<Geom>());
    Geom const & g = geom.get<Geom>();
    CHECK(g.x == Approx(2377.0));
    CHECK(g.y == Approx(1773.0));
}

