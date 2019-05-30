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

TEST_CASE("vector wafer output - polygon")
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
            using Geom = mapnik::geometry::polygon<double>;
            CHECK(geom.is<Geom>());

            switch (index)
            {
                case 3 * 8 + 3:
                {
                    Geom const & g = geom.get<Geom>();
                    auto const & exterior = g.exterior_ring;
                    REQUIRE(exterior.size() == 5);

                    CHECK(exterior[0].x == Approx(4160.0));
                    CHECK(exterior[0].y == Approx(3181.0));

                    CHECK(exterior[1].x == Approx(4160.0));
                    CHECK(exterior[1].y == Approx(4160.0));

                    CHECK(exterior[2].x == Approx(3186.0));
                    CHECK(exterior[2].y == Approx(4160.0));

                    CHECK(exterior[3].x == Approx(3186.0));
                    CHECK(exterior[3].y == Approx(3181.0));

                    CHECK(exterior[4].x == Approx(4160.0));
                    CHECK(exterior[4].y == Approx(3181.0));
                    break;
                }
                case 3 * 8 + 4:
                {
                    Geom const & g = geom.get<Geom>();
                    auto const & exterior = g.exterior_ring;
                    REQUIRE(exterior.size() == 5);

                    CHECK(exterior[0].x == Approx(910.0));
                    CHECK(exterior[0].y == Approx(3181.0));

                    CHECK(exterior[1].x == Approx(910.0));
                    CHECK(exterior[1].y == Approx(4160.0));

                    CHECK(exterior[2].x == Approx(-64.0));
                    CHECK(exterior[2].y == Approx(4160.0));

                    CHECK(exterior[3].x == Approx(-64.0));
                    CHECK(exterior[3].y == Approx(3181.0));

                    CHECK(exterior[4].x == Approx(910.0));
                    CHECK(exterior[4].y == Approx(3181.0));
                    break;
                }
                case 4 * 8 + 3:
                {
                    Geom const & g = geom.get<Geom>();
                    auto const & exterior = g.exterior_ring;
                    REQUIRE(exterior.size() == 5);

                    CHECK(exterior[0].x == Approx(4160.0));
                    CHECK(exterior[0].y == Approx(-64.0));

                    CHECK(exterior[1].x == Approx(4160.0));
                    CHECK(exterior[1].y == Approx(915.0));

                    CHECK(exterior[2].x == Approx(3186.0));
                    CHECK(exterior[2].y == Approx(915.0));

                    CHECK(exterior[3].x == Approx(3186.0));
                    CHECK(exterior[3].y == Approx(-64.0));

                    CHECK(exterior[4].x == Approx(4160.0));
                    CHECK(exterior[4].y == Approx(-64.0));
                    break;
                }
                case 4 * 8 + 4:
                {
                    Geom const & g = geom.get<Geom>();
                    auto const & exterior = g.exterior_ring;
                    REQUIRE(exterior.size() == 5);

                    CHECK(exterior[0].x == Approx(910.0));
                    CHECK(exterior[0].y == Approx(-64.0));

                    CHECK(exterior[1].x == Approx(910.0));
                    CHECK(exterior[1].y == Approx(915.0));

                    CHECK(exterior[2].x == Approx(-64.0));
                    CHECK(exterior[2].y == Approx(915.0));

                    CHECK(exterior[3].x == Approx(-64.0));
                    CHECK(exterior[3].y == Approx(-64.0));

                    CHECK(exterior[4].x == Approx(910.0));
                    CHECK(exterior[4].y == Approx(-64.0));
                    break;
                }
            }
        }
        else
        {
            CHECK(tile.has_layer("polygon") == false);
        }
        ++index;
    }
}


TEST_CASE("vector wafer output - point")
{
    const std::string style(R"xxx(
        <Map srs="+init=epsg:3857">
            <Layer name="point" srs="+init=epsg:4326">
                <Datasource>
                    <Parameter name="type">csv</Parameter>
                    <Parameter name="inline">
                        x, y
                        0, 0
                    </Parameter>
                </Datasource>
            </Layer>
        </Map>)xxx");

    mapnik::Map map(256, 256);
    mapnik::load_map_string(map, style);

    mapnik::vector_tile_impl::processor ren(map);

    mapnik::vector_tile_impl::merc_wafer wafer = ren.create_wafer(0, 0, 2, 4, 1024, 64);
    CHECK(wafer.span() == 4);
    REQUIRE(wafer.tiles().size() == 16);

    const mapnik::box2d<double> expected_extent(
        -20037508.3427892439, -20037508.3427892439,
         20037508.3427892439,  20037508.3427892439);
    CHECK(wafer.extent().minx() == Approx(expected_extent.minx()));
    CHECK(wafer.extent().miny() == Approx(expected_extent.miny()));
    CHECK(wafer.extent().maxx() == Approx(expected_extent.maxx()));
    CHECK(wafer.extent().maxy() == Approx(expected_extent.maxy()));

    CHECK(wafer.tile_size() == 1024 * 4);
    CHECK(wafer.buffer_size() == 64);
    CHECK(wafer.has_layer("point") == true);

    const std::set<std::size_t> non_empty_tile_indices {
        1 * 4 + 1, 1 * 4 + 2,
        2 * 4 + 1, 2 * 4 + 2 };
    std::size_t index = 0;

    for (auto const & tile : wafer.tiles())
    {
        if (non_empty_tile_indices.count(index))
        {
            CHECK(tile.has_layer("point") == true);
            vector_tile::Tile mvt;
            mvt.ParseFromString(tile.get_buffer());
            REQUIRE(1 == mvt.layers_size());
            vector_tile::Tile_Layer const& layer = mvt.layers(0);
            CHECK(std::string("point") == layer.name());
            REQUIRE(1 == layer.features_size());
            vector_tile::Tile_Feature const& feature = layer.features(0);
            std::string feature_string = feature.SerializeAsString();
            mapnik::vector_tile_impl::GeometryPBF geoms = feature_to_pbf_geometry(feature_string);
            auto geom = mapnik::vector_tile_impl::decode_geometry<double>(
                geoms, feature.type(), 2, 0.0, 0.0, 1.0, 1.0);
            using Geom = mapnik::geometry::point<double>;
            CHECK(geom.is<Geom>());

            switch (index)
            {
                case 1 * 4 + 1:
                {
                    Geom const & g = geom.get<Geom>();
                    CHECK(g.x == Approx(1024.0));
                    CHECK(g.y == Approx(1024.0));
                    break;
                }
                case 1 * 4 + 2:
                {
                    Geom const & g = geom.get<Geom>();
                    CHECK(g.x == Approx(0.0));
                    CHECK(g.y == Approx(1024.0));
                    break;
                }
                case 2 * 4 + 1:
                {
                    Geom const & g = geom.get<Geom>();
                    CHECK(g.x == Approx(1024.0));
                    CHECK(g.y == Approx(0.0));
                    break;
                }
                case 2 * 4 + 2:
                {
                    Geom const & g = geom.get<Geom>();
                    CHECK(g.x == Approx(0.0));
                    CHECK(g.y == Approx(0.0));
                    break;
                }
            }
        }
        else
        {
            CHECK(tile.has_layer("point") == false);
        }
        ++index;
    }
}

TEST_CASE("vector wafer output - multipoint")
{
    const std::string style(R"xxx(
        <Map srs="+init=epsg:3857">
            <Layer name="multipoint" srs="+init=epsg:4326">
                <Datasource>
                    <Parameter name="type">csv</Parameter>
                    <Parameter name="inline">
                        wkt
                        "MULTIPOINT(0 0, 10 10)"
                    </Parameter>
                </Datasource>
            </Layer>
        </Map>)xxx");

    mapnik::Map map(256, 256);
    mapnik::load_map_string(map, style);

    mapnik::vector_tile_impl::processor ren(map);

    mapnik::vector_tile_impl::merc_wafer wafer = ren.create_wafer(0, 0, 2, 4, 1024, 64);
    CHECK(wafer.span() == 4);
    REQUIRE(wafer.tiles().size() == 16);

    const mapnik::box2d<double> expected_extent(
        -20037508.3427892439, -20037508.3427892439,
         20037508.3427892439,  20037508.3427892439);
    CHECK(wafer.extent().minx() == Approx(expected_extent.minx()));
    CHECK(wafer.extent().miny() == Approx(expected_extent.miny()));
    CHECK(wafer.extent().maxx() == Approx(expected_extent.maxx()));
    CHECK(wafer.extent().maxy() == Approx(expected_extent.maxy()));

    CHECK(wafer.tile_size() == 1024 * 4);
    CHECK(wafer.buffer_size() == 64);
    CHECK(wafer.has_layer("multipoint") == true);

    const std::set<std::size_t> non_empty_tile_indices {
        1 * 4 + 1, 1 * 4 + 2,
        2 * 4 + 1, 2 * 4 + 2 };
    const std::size_t multipoint_index = 1 * 4 + 2;
    std::size_t index = 0;

    for (auto const & tile : wafer.tiles())
    {
        if (non_empty_tile_indices.count(index))
        {
            CHECK(tile.has_layer("multipoint") == true);
            vector_tile::Tile mvt;
            mvt.ParseFromString(tile.get_buffer());
            REQUIRE(1 == mvt.layers_size());
            vector_tile::Tile_Layer const& layer = mvt.layers(0);
            CHECK(std::string("multipoint") == layer.name());
            REQUIRE(1 == layer.features_size());
            vector_tile::Tile_Feature const& feature = layer.features(0);
            std::string feature_string = feature.SerializeAsString();
            mapnik::vector_tile_impl::GeometryPBF geoms = feature_to_pbf_geometry(feature_string);
            auto geom = mapnik::vector_tile_impl::decode_geometry<double>(
                geoms, feature.type(), 2, 0.0, 0.0, 1.0, 1.0);

            if (index == multipoint_index)
            {
                using Geom = mapnik::geometry::multi_point<double>;
                CHECK(geom.is<Geom>());
                Geom const & g = geom.get<Geom>();
                CHECK(g.size() == 2);
                CHECK(g[0].x == Approx(0.0));
                CHECK(g[0].y == Approx(1024.0));
                CHECK(g[1].x == Approx(114.0));
                CHECK(g[1].y == Approx(910.0));
            }
            else
            {
                using Geom = mapnik::geometry::point<double>;
                CHECK(geom.is<Geom>());

                switch (index)
                {
                    case 1 * 4 + 1:
                    {
                        Geom const & g = geom.get<Geom>();
                        CHECK(g.x == Approx(1024.0));
                        CHECK(g.y == Approx(1024.0));
                        break;
                    }
                    case 1 * 4 + 2:
                    {
                        CHECK(false); // Should not get here
                        break;
                    }
                    case 2 * 4 + 1:
                    {
                        Geom const & g = geom.get<Geom>();
                        CHECK(g.x == Approx(1024.0));
                        CHECK(g.y == Approx(0.0));
                        break;
                    }
                    case 2 * 4 + 2:
                    {
                        Geom const & g = geom.get<Geom>();
                        CHECK(g.x == Approx(0.0));
                        CHECK(g.y == Approx(0.0));
                        break;
                    }
                }
            }
        }
        else
        {
            CHECK(tile.has_layer("multipoint") == false);
        }
        ++index;
    }
}


TEST_CASE("vector wafer output - multipolygon")
{
    const std::string style(R"xxx(
        <Map srs="+init=epsg:3857">
            <Layer name="multipolygon" srs="+init=epsg:4326">
                <Datasource>
                    <Parameter name="type">geojson</Parameter>
                    <Parameter name="inline">
                        {"type":"MultiPolygon","coordinates":[
                            [[
                                [ 10,  10],
                                [-10,  10],
                                [-10, -10],
                                [ 10, -10],
                                [ 10,  10]
                            ]],
                            [[
                                [30, 30],
                                [10, 30],
                                [10, 10],
                                [30, 10],
                                [30, 30]
                            ]]
                        ]}
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
    CHECK(wafer.has_layer("multipolygon") == true);

    const std::set<std::size_t> non_empty_tile_indices {
        3 * 8 + 3, 3 * 8 + 4,
        4 * 8 + 3, 4 * 8 + 4 };
    const std::size_t multipolygon_index = 3 * 8 + 4;
    std::size_t index = 0;

    for (auto const & tile : wafer.tiles())
    {
        if (non_empty_tile_indices.count(index))
        {
            CHECK(tile.has_layer("multipolygon") == true);
            vector_tile::Tile mvt;
            mvt.ParseFromString(tile.get_buffer());
            REQUIRE(1 == mvt.layers_size());
            vector_tile::Tile_Layer const& layer = mvt.layers(0);
            CHECK(std::string("multipolygon") == layer.name());
            REQUIRE(1 == layer.features_size());
            vector_tile::Tile_Feature const& feature = layer.features(0);
            std::string feature_string = feature.SerializeAsString();
            mapnik::vector_tile_impl::GeometryPBF geoms = feature_to_pbf_geometry(feature_string);
            auto geom = mapnik::vector_tile_impl::decode_geometry<double>(
                geoms, feature.type(), 2, 0.0, 0.0, 1.0, 1.0);

            if (index == multipolygon_index)
            {
                using Geom = mapnik::geometry::multi_polygon<double>;
                CHECK(geom.is<Geom>());
                Geom const & g = geom.get<Geom>();
                REQUIRE(g.size() == 2);
            }
            else
            {
                using Geom = mapnik::geometry::polygon<double>;
                CHECK(geom.is<Geom>());

                switch (index)
                {
                    case 3 * 8 + 3:
                    {
                        Geom const & g = geom.get<Geom>();
                        auto const & exterior = g.exterior_ring;
                        CHECK(exterior.size() == 5);
                        break;
                    }
                    case 3 * 8 + 4:
                    {
                        CHECK(false);
                        break;
                    }
                    case 4 * 8 + 3:
                    {
                        Geom const & g = geom.get<Geom>();
                        auto const & exterior = g.exterior_ring;
                        CHECK(exterior.size() == 5);
                        break;
                    }
                    case 4 * 8 + 4:
                    {
                        Geom const & g = geom.get<Geom>();
                        auto const & exterior = g.exterior_ring;
                        CHECK(exterior.size() == 5);
                        break;
                    }
                }
            }
        }
        else
        {
            CHECK(tile.has_layer("multipolygon") == false);
        }
        ++index;
    }
}

