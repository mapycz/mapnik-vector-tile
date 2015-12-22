#include "catch.hpp"

// test utils
#include "test_utils.hpp"
#include "vector_tile_projection.hpp"

// vector output api
#include "vector_tile_processor.hpp"
#include "vector_tile_backend_pbf.hpp"
#include "vector_tile_util.hpp"
#include "vector_tile_datasource.hpp"

#include <mapnik/util/fs.hpp>
#include <mapnik/datasource_cache.hpp>
#include <mapnik/agg_renderer.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/image_reader.hpp>
#include <mapnik/image_util.hpp>
#include <mapnik/feature_factory.hpp>
#include <mapnik/raster.hpp>

#include <sstream>
#include <fstream>

TEST_CASE( "raster tile output 1", "should create raster tile with one raster layer" ) {
    typedef mapnik::vector_tile_impl::backend_pbf backend_type;
    typedef mapnik::vector_tile_impl::processor<backend_type> renderer_type;
    typedef vector_tile::Tile tile_type;
    unsigned _x=0,_y=0,_z=1;
    double minx,miny,maxx,maxy;
    mapnik::vector_tile_impl::spherical_mercator merc(512);
    merc.xyz(_x,_y,_z,minx,miny,maxx,maxy);
    mapnik::box2d<double> bbox;
    bbox.init(minx,miny,maxx,maxy);
    unsigned tile_size = 512;
    tile_type tile;
    backend_type backend(tile,16);
    mapnik::Map map(tile_size,tile_size,"+init=epsg:3857");
    map.set_buffer_size(256);
    mapnik::layer lyr("layer",map.srs());
    mapnik::parameters params;
    params["type"] = "gdal";
    // created with:
    // wget http://www.nacis.org/naturalearth/50m/raster/NE2_50m_SR_W.zip
    // gdalwarp -t_srs EPSG:3857 -ts 1048 1048 -r bilinear NE2_50M_SR_W.tif natural_earth.tif
    params["file"] = "test/data/natural_earth.tif";
    std::shared_ptr<mapnik::datasource> ds =
        mapnik::datasource_cache::instance().create(params);
    lyr.set_datasource(ds);
    map.add_layer(lyr);
    mapnik::request m_req(tile_size,tile_size,bbox);
    m_req.set_buffer_size(map.buffer_size());
    renderer_type ren(backend,map,m_req,1.0,0,0,1,false,"jpeg",mapnik::SCALING_BILINEAR);
    ren.apply();
    std::string key("");
    CHECK(false == mapnik::vector_tile_impl::is_solid_extent(tile,key));
    CHECK("" == key);
    CHECK(1 == tile.layers_size());
    vector_tile::Tile_Layer const& layer = tile.layers(0);
    CHECK(std::string("layer") == layer.name());
    CHECK(1 == layer.features_size());
    vector_tile::Tile_Feature const& f = layer.features(0);
    CHECK(static_cast<mapnik::value_integer>(1) == static_cast<mapnik::value_integer>(f.id()));
    CHECK(0 == f.geometry_size());
    CHECK(f.has_raster());
    std::string const& ras_buffer = f.raster();
    CHECK(!ras_buffer.empty());
    // debug
    bool debug = false;

    if (!mapnik::util::exists("test/fixtures/expected-2.jpeg")) {
        std::ofstream file("test/fixtures/expected-2.jpeg", std::ios::out|std::ios::trunc|std::ios::binary);
        if (!file) {
            throw std::runtime_error("could not write image");
        }
        file << ras_buffer;
        file.close();
    }
    std::unique_ptr<mapnik::image_reader> reader(mapnik::get_image_reader(ras_buffer.data(),ras_buffer.size()));
    if (!reader.get()) {
        throw std::runtime_error("could not open image bytes");
    }
    mapnik::image_rgba8 im_data(reader->width(),reader->height());
    reader->read(0,0,im_data);
    unsigned diff = testing::compare_images(im_data,"test/fixtures/expected-2.jpeg");
    CHECK(0 == diff);
    if (diff > 0) {
        mapnik::save_to_file(im_data,"test/fixtures/actual-2.jpeg","jpeg");
    }

    std::size_t expected_image_size = 45660;
    int expected_vtile_size = expected_image_size + 26;
    if (!debug) {
        CHECK(expected_image_size == ras_buffer.size());
        CHECK(expected_vtile_size == tile.ByteSize());
    }
    std::string buffer;
    CHECK(tile.SerializeToString(&buffer));
    if (!debug) {
        CHECK(expected_vtile_size == buffer.size());
    }
    // now read back and render image
    mapnik::Map map2(tile_size,tile_size,"+init=epsg:3857");
    map2.set_buffer_size(256);
    tile_type tile2;
    CHECK(tile2.ParseFromString(buffer));
    std::string key2("");
    CHECK(false == mapnik::vector_tile_impl::is_solid_extent(tile,key2));
    CHECK("" == key2);
    CHECK(1 == tile2.layers_size());
    vector_tile::Tile_Layer const& layer2 = tile2.layers(0);
    CHECK(std::string("layer") == layer2.name());
    CHECK(1 == layer2.features_size());
    vector_tile::Tile_Feature const& f2 = layer2.features(0);
    CHECK(static_cast<mapnik::value_integer>(1) == static_cast<mapnik::value_integer>(f2.id()));
    CHECK(0 == f2.geometry_size());
    CHECK(f2.has_raster());
    CHECK(!f2.raster().empty());
    if (!debug) {
        CHECK(expected_image_size == f2.raster().size());
    }
    mapnik::layer lyr2("layer",map2.srs());
    std::shared_ptr<mapnik::vector_tile_impl::tile_datasource> ds2 = std::make_shared<
                                    mapnik::vector_tile_impl::tile_datasource>(
                                        layer2,_x,_y,_z,map2.width());
    lyr2.set_datasource(ds2);
    lyr2.add_style("style");
    map2.add_layer(lyr2);
    mapnik::load_map(map2,"test/data/raster_style.xml");
    map2.zoom_to_box(bbox);
    mapnik::image_rgba8 im(map2.width(),map2.height());
    mapnik::agg_renderer<mapnik::image_rgba8> ren2(map2,im);
    ren2.apply();
    if (!mapnik::util::exists("test/fixtures/expected-2.png")) {
        mapnik::save_to_file(im,"test/fixtures/expected-2.png","png32");
    }
    diff = testing::compare_images(im,"test/fixtures/expected-2.png");
    CHECK(0 == diff);
    if (diff > 0) {
        mapnik::save_to_file(im_data,"test/fixtures/actual-2.png","png32");
    }
}

TEST_CASE( "raster tile output 2", "should be able to overzoom raster" ) {
    typedef vector_tile::Tile tile_type;
    tile_type tile;
    {
        typedef mapnik::vector_tile_impl::backend_pbf backend_type;
        typedef mapnik::vector_tile_impl::processor<backend_type> renderer_type;
        double minx,miny,maxx,maxy;
        mapnik::vector_tile_impl::spherical_mercator merc(256);
        merc.xyz(0,0,0,minx,miny,maxx,maxy);
        mapnik::box2d<double> bbox(minx,miny,maxx,maxy);
        backend_type backend(tile,16);
        mapnik::Map map(256,256,"+init=epsg:3857");
        map.set_buffer_size(1024);
        mapnik::layer lyr("layer",map.srs());
        mapnik::parameters params;
        params["type"] = "gdal";
        std::ostringstream s;
        s << std::fixed << std::setprecision(16)
          << bbox.minx() << ',' << bbox.miny() << ','
          << bbox.maxx() << ',' << bbox.maxy();
        params["extent"] = s.str();
        params["file"] = "test/data/256x256.png";
        std::shared_ptr<mapnik::datasource> ds =
            mapnik::datasource_cache::instance().create(params);
        lyr.set_datasource(ds);
        map.add_layer(lyr);
        mapnik::request m_req(256,256,bbox);
        m_req.set_buffer_size(map.buffer_size());
        renderer_type ren(backend,map,m_req,1.0,0,0,1,false,"jpeg",mapnik::SCALING_BILINEAR);
        ren.apply();
    }
    // Done creating test data, now test created tile
    CHECK(1 == tile.layers_size());
    vector_tile::Tile_Layer const& layer = tile.layers(0);
    CHECK(std::string("layer") == layer.name());
    CHECK(1 == layer.features_size());
    vector_tile::Tile_Feature const& f = layer.features(0);
    CHECK(static_cast<mapnik::value_integer>(1) == static_cast<mapnik::value_integer>(f.id()));
    CHECK(0 == f.geometry_size());
    CHECK(f.has_raster());
    std::string const& ras_buffer = f.raster();
    CHECK(!ras_buffer.empty());
    // debug
    bool debug = false;
    if (debug) {
        std::ofstream file("out2.png", std::ios::out|std::ios::trunc|std::ios::binary);
        file << ras_buffer;
        file.close();
    }

    // confirm tile looks correct as encoded
    std::size_t expected_image_size = 1654;
    int expected_vtile_size = expected_image_size + 23;
    if (!debug) {
        CHECK(expected_image_size == ras_buffer.size());
        CHECK(expected_vtile_size == tile.ByteSize());
    }
    std::string buffer;
    CHECK(tile.SerializeToString(&buffer));
    if (!debug) {
        CHECK(expected_vtile_size == buffer.size());
    }
    tile_type tile2;
    CHECK(tile2.ParseFromString(buffer));
    CHECK(1 == tile2.layers_size());
    vector_tile::Tile_Layer const& layer2 = tile2.layers(0);
    CHECK(std::string("layer") == layer2.name());
    CHECK(1 == layer2.features_size());
    vector_tile::Tile_Feature const& f2 = layer2.features(0);
    CHECK(static_cast<mapnik::value_integer>(1) == static_cast<mapnik::value_integer>(f2.id()));
    CHECK(0 == f2.geometry_size());
    CHECK(f2.has_raster());
    CHECK(!f2.raster().empty());
    if (!debug) {
        CHECK(expected_image_size == f2.raster().size());
    }

    // now read back and render image at larger size
    // and zoomed in
    double minx,miny,maxx,maxy;
    mapnik::vector_tile_impl::spherical_mercator merc(256);
    // 2/0/1.png
    merc.xyz(0,1,2,minx,miny,maxx,maxy);
    mapnik::box2d<double> bbox(minx,miny,maxx,maxy);
    mapnik::Map map2(256,256,"+init=epsg:3857");
    map2.set_buffer_size(1024);
    mapnik::layer lyr2("layer",map2.srs());
    std::shared_ptr<mapnik::vector_tile_impl::tile_datasource> ds2 = std::make_shared<
                                    mapnik::vector_tile_impl::tile_datasource>(
                                        layer2,0,0,0,256);
    lyr2.set_datasource(ds2);
    lyr2.add_style("style");
    map2.add_layer(lyr2);
    mapnik::load_map(map2,"test/data/raster_style.xml");
    map2.zoom_to_box(bbox);
    mapnik::image_rgba8 im(map2.width(),map2.height());
    mapnik::agg_renderer<mapnik::image_rgba8> ren2(map2,im);
    ren2.apply();
    if (!mapnik::util::exists("test/fixtures/expected-3.png")) {
        mapnik::save_to_file(im,"test/fixtures/expected-3.png","png32");
    }
    unsigned diff = testing::compare_images(im,"test/fixtures/expected-3.png");
    CHECK(0 == diff);
    if (diff > 0) {
        mapnik::save_to_file(im,"test/fixtures/actual-3.png","png32");
    }
}

TEST_CASE( "raster tile output 3", "should be able to round trip image with alpha" ) {
    mapnik::box2d<double> bbox(-20037508.342789,-20037508.342789,20037508.342789,20037508.342789);

    // create a vtile from scratch with a raster

    vector_tile::Tile tile;
    std::string format("png32");
    {
        mapnik::vector_tile_impl::backend_pbf backend(tile,16);
        backend.start_tile_layer("layer");
        mapnik::feature_ptr feature(mapnik::feature_factory::create(std::make_shared<mapnik::context_type>(),1));
        backend.start_tile_feature(*feature);
        std::unique_ptr<mapnik::image_reader> reader(mapnik::get_image_reader("./test/fixtures/alpha-white-2.png"));
        if (!reader.get()) {
            throw std::runtime_error("could not open image");
        }
        mapnik::image_rgba8 im_data(reader->width(),reader->height());
        reader->read(0,0,im_data);
        backend.add_tile_feature_raster(mapnik::save_to_string(im_data,format));
        backend.stop_tile_feature();
        backend.stop_tile_layer();
    }
    {
        REQUIRE(1 == tile.layers_size());
        vector_tile::Tile_Layer const& layer = tile.layers(0);
        REQUIRE(1 == layer.features_size());
        vector_tile::Tile_Feature const& f = layer.features(0);
        REQUIRE(f.has_raster());
        std::string const& ras_buffer = f.raster();
        REQUIRE(!ras_buffer.empty());
        std::unique_ptr<mapnik::image_reader> reader(mapnik::get_image_reader(ras_buffer.data(),ras_buffer.size()));
        if (!reader.get()) {
            throw std::runtime_error("could not open image bytes");
        }
        mapnik::image_rgba8 im_data(reader->width(),reader->height());
        reader->read(0,0,im_data);
        unsigned diff = testing::compare_images(im_data,"./test/fixtures/alpha-white-2.png",0,true);
        CHECK(0 == diff);
        if (diff > 0) {
            mapnik::save_to_file(im_data,"test/fixtures/actual-4.png",format);
        }

    }

    // Now actually re-render to trigger the raster being passed through the processor
    // and confirm raster still looks correct
    {
        // create datasource wrapping raster
        unsigned tile_size = 256;
        std::shared_ptr<mapnik::vector_tile_impl::tile_datasource> ds = std::make_shared<
                                        mapnik::vector_tile_impl::tile_datasource>(
                                            tile.layers(0),0,0,0,tile_size);
        // before rendering let's query the raster directly to ensure
        // the datasource returns it correctly.
        mapnik::query q(bbox);
        mapnik::featureset_ptr fs = ds->features(q);
        mapnik::feature_ptr feat = fs->next();
        mapnik::raster_ptr const& source = feat->get_raster();
        CHECK(source->data_.is<mapnik::image_rgba8>());
        mapnik::image_rgba8 const& source_data = mapnik::util::get<mapnik::image_rgba8>(source->data_);
        unsigned diff = testing::compare_images(source_data,"./test/fixtures/alpha-white-2.png",0,true);
        CHECK(0 == diff);
        if (diff > 0) {
            mapnik::save_to_file(source_data,"test/fixtures/actual-5.png",format);
        }

        // okay, now we'll re-render another vector tile from original
        // which triggers the raster to be resampled
        mapnik::Map map(tile_size,tile_size,"+init=epsg:3857");
        mapnik::layer lyr("layer",map.srs());
        ds->set_envelope(bbox);
        lyr.set_datasource(ds);
        map.add_layer(lyr);
        map.zoom_to_box(bbox);

        typedef mapnik::vector_tile_impl::backend_pbf backend_type;
        vector_tile::Tile round_tipped_tile;
        backend_type backend(round_tipped_tile,16);
        typedef mapnik::vector_tile_impl::processor<backend_type> renderer_type;
        mapnik::request m_req(tile_size,tile_size,bbox);
        renderer_type ren(backend,
                          map,
                          m_req,
                          1.0,
                          0,
                          0,
                          1,
                          false,
                          format,
                          mapnik::SCALING_BILINEAR);
        ren.apply();
        REQUIRE(1 == round_tipped_tile.layers_size());
        vector_tile::Tile_Layer const& layer = round_tipped_tile.layers(0);
        REQUIRE(1 == layer.features_size());
        vector_tile::Tile_Feature const& f = layer.features(0);
        REQUIRE(f.has_raster());
        std::string const& ras_buffer = f.raster();
        REQUIRE(!ras_buffer.empty());
        std::unique_ptr<mapnik::image_reader> reader(mapnik::get_image_reader(ras_buffer.data(),ras_buffer.size()));
        if (!reader.get()) {
            throw std::runtime_error("could not open image bytes");
        }

        mapnik::image_rgba8 im_data(reader->width(),reader->height());
        reader->read(0,0,im_data);

        if (!mapnik::util::exists("test/fixtures/expected-4.png")) {
            mapnik::save_to_file(im_data,"test/fixtures/expected-4.png","png32");
        }

        diff = testing::compare_images(im_data,"test/fixtures/expected-4.png",0,true);
        CHECK(0 == diff);
        if (diff > 0) {
            mapnik::save_to_file(im_data,"test/fixtures/actual-7.png",format);
        }

        // visually identical but scaling causes some pixels shifts compared to original
        diff = testing::compare_images(im_data,"./test/fixtures/alpha-white-2.png",0,true);
        CHECK(13853 == diff);
        if (diff > 0) {
            mapnik::save_to_file(im_data,"test/fixtures/actual-7.png",format);
        }
    }

}
