#ifndef __MAPNIK_VECTOR_TILE_STYLE_LEVEL_PROCESSOR__
#define __MAPNIK_VECTOR_TILE_STYLE_LEVEL_PROCESSOR__

// mapnik
#include <mapnik/config.hpp>            // for MAPNIK_DECL
#include <mapnik/feature_style_processor.hpp>
#include <mapnik/util/noncopyable.hpp>       // for noncopyable
#include <mapnik/rule.hpp>              // for rule, symbolizers
#include <mapnik/geometry/box2d.hpp>     // for box2d
#include <mapnik/view_transform.hpp>    // for view_transform
#include <mapnik/image_compositing.hpp>  // for composite_mode_e
#include <mapnik/pixel_position.hpp>
#include <mapnik/request.hpp>
#include <mapnik/symbolizer_enumerations.hpp>
#include <mapnik/renderer_common.hpp>
#include <mapnik/image_util.hpp>
// stl
#include <memory>

namespace mapnik
{

namespace vector_tile_impl
{

class MAPNIK_DECL style_level_processor :
    public feature_style_processor<style_level_processor>,
    private util::noncopyable
{

public:
    style_level_processor(mapnik::Map const& map, double scale_factor)
        : feature_style_processor<style_level_processor>(map, scale_factor),
          image_format_("webp"),
          area_threshold_(0.1),
          simplify_distance_(0.0),
          fill_type_(positive_fill),
          scaling_method_(SCALING_BILINEAR),
          strictly_simple_(true),
          multi_polygon_union_(false),
          process_all_rings_(false)
    {
    }

    ~style_level_processor() = default;

    tile create_tile(mapnik::box2d<double> const & extent,
                     std::uint32_t tile_size = 4096,
                     std::int32_t buffer_size = 0,
                     double scale_denom = 0.0,
                     int offset_x = 0,
                     int offset_y = 0)
    {
        tile t(extent, tile_size, buffer_size);
        update_tile(t, scale_denom, offset_x, offset_y);
        return t;
    }

    /*
    merc_tile create_tile(std::uint64_t x,
                          std::uint64_t y,
                          std::uint64_t z,
                          std::uint32_t tile_size = 4096,
                          std::int32_t buffer_size = 0,
                          double scale_denom = 0.0,
                          int offset_x = 0,
                          int offset_y = 0)
    {
        merc_tile t(x, y, z, tile_size, buffer_size);
        update_tile(t, scale_denom, offset_x, offset_y);
        return t;
    }
    */

    MAPNIK_VECTOR_INLINE void processor::update_tile(tile & t,
                                                     double scale_denom,
                                                     int offset_x,
                                                     int offset_y)
    {
        // Futures
        std::vector<tile_layer> tile_layers;

        for (mapnik::layer const& lay : m_.layers())
        {
            if (t.has_layer(lay.name()))
            {
                continue;
            }
            tile_layers.emplace_back(m_,
                                 lay,
                                 t.extent(),
                                 t.tile_size(),
                                 t.buffer_size(),
                                 scale_factor_,
                                 scale_denom,
                                 offset_x,
                                 offset_y);
            if (!tile_layers.back().is_valid())
            {
                t.add_empty_layer(lay.name());
                tile_layers.pop_back();
                continue;
            }
        }

        for (auto & layer_ref : tile_layers)
        {
            if (layer_ref.get_ds()->type() == datasource::Vector)
            {
                detail::create_geom_layer(layer_ref,
                                          simplify_distance_,
                                          area_threshold_,
                                          fill_type_,
                                          strictly_simple_,
                                          multi_polygon_union_,
                                          process_all_rings_
                                         );
            }
            else // Raster
            {
                detail::create_raster_layer(layer_ref,
                                            image_format_,
                                            scaling_method_
                                           );
            }
        }

        for (auto & layer_ref : tile_layers)
        {
            t.add_layer(layer_ref); 
        }
    }

    void start_map_processing(Map const& map);
    void end_map_processing(Map const& map);
    void start_layer_processing(layer const& lay, box2d<double> const& query_extent);
    void end_layer_processing(layer const& lay);

    void start_style_processing(feature_type_style const& st);
    void end_style_processing(feature_type_style const& st);

    template <typename Symbolizer>
    void process(Symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans)
    {
        MAPNIK_LOG_ERROR(style_level_processor) << "style_level_processor should not process individual symbolizer.";
    }

    inline bool process(rule::symbolizers const&,
                        mapnik::feature_impl& feature,
                        proj_transform const& )
    {
        using encoding_process = mapnik::vector_tile_impl::geometry_to_feature_pbf_visitor;
        using clipping_process = mapnik::vector_tile_impl::geometry_clipper<encoding_process>;

        mapnik::vector_tile_impl::vector_tile_strategy vs(layer.get_view_transform());
        mapnik::box2d<double> const& buffered_extent = layer.get_target_buffered_extent();
        const mapnik::geometry::point<double> p1_min(buffered_extent.minx(), buffered_extent.miny());
        const mapnik::geometry::point<double> p1_max(buffered_extent.maxx(), buffered_extent.maxy());
        const mapnik::geometry::point<std::int64_t> p2_min = mapnik::geometry::transform<std::int64_t>(p1_min, vs);
        const mapnik::geometry::point<std::int64_t> p2_max = mapnik::geometry::transform<std::int64_t>(p1_max, vs);
        const double minx = std::min(p2_min.x, p2_max.x);
        const double maxx = std::max(p2_min.x, p2_max.x);
        const double miny = std::min(p2_min.y, p2_max.y);
        const double maxy = std::max(p2_min.y, p2_max.y);
        const mapnik::box2d<int> tile_clipping_extent(minx, miny, maxx, maxy);

        if (simplify_distance > 0)
        {
            using simplifier_process = mapnik::vector_tile_impl::geometry_simplifier<clipping_process>;
            if (layer.get_proj_transform().equal())
            {
                using strategy_type = mapnik::vector_tile_impl::vector_tile_strategy;
                using transform_type = mapnik::vector_tile_impl::transform_visitor<strategy_type, simplifier_process>;
                mapnik::geometry::geometry<double> const& geom = feature->get_geometry();
                encoding_process encoder(*feature, builder);
                clipping_process clipper(tile_clipping_extent, 
                        area_threshold, 
                        strictly_simple, 
                        multi_polygon_union, 
                        fill_type,
                        process_all_rings,
                        encoder);
                simplifier_process simplifier(simplify_distance, clipper);
                transform_type transformer(vs, buffered_extent, simplifier);
                mapnik::util::apply_visitor(transformer, geom);
            }
            else
            {
                using strategy_type = mapnik::vector_tile_impl::vector_tile_strategy_proj;
                using transform_type = mapnik::vector_tile_impl::transform_visitor<strategy_type, simplifier_process>;
                strategy_type vs2(layer.get_proj_transform(), layer.get_view_transform());
                mapnik::box2d<double> const& trans_buffered_extent = layer.get_source_buffered_extent();
                mapnik::geometry::geometry<double> const& geom = feature->get_geometry();
                encoding_process encoder(*feature, builder);
                clipping_process clipper(tile_clipping_extent, 
                        area_threshold, 
                        strictly_simple, 
                        multi_polygon_union, 
                        fill_type,
                        process_all_rings,
                        encoder);
                simplifier_process simplifier(simplify_distance, clipper);
                transform_type transformer(vs2, trans_buffered_extent, simplifier);
                mapnik::util::apply_visitor(transformer, geom);
            }
        }
        else
        {
            if (layer.get_proj_transform().equal())
            {
                using strategy_type = mapnik::vector_tile_impl::vector_tile_strategy;
                using transform_type = mapnik::vector_tile_impl::transform_visitor<strategy_type, clipping_process>;
                mapnik::geometry::geometry<double> const& geom = feature->get_geometry();
                encoding_process encoder(*feature, builder);
                clipping_process clipper(tile_clipping_extent, 
                        area_threshold, 
                        strictly_simple, 
                        multi_polygon_union, 
                        fill_type,
                        process_all_rings,
                        encoder);
                transform_type transformer(vs, buffered_extent, clipper);
                mapnik::util::apply_visitor(transformer, geom);
            }
            else
            {
                using strategy_type = mapnik::vector_tile_impl::vector_tile_strategy_proj;
                using transform_type = mapnik::vector_tile_impl::transform_visitor<strategy_type, clipping_process>;
                strategy_type vs2(layer.get_proj_transform(), layer.get_view_transform());
                mapnik::box2d<double> const& trans_buffered_extent = layer.get_source_buffered_extent();
                mapnik::geometry::geometry<double> const& geom = feature->get_geometry();
                encoding_process encoder(*feature, builder);
                clipping_process clipper(tile_clipping_extent, 
                        area_threshold, 
                        strictly_simple, 
                        multi_polygon_union, 
                        fill_type,
                        process_all_rings,
                        encoder);
                transform_type transformer(vs2, trans_buffered_extent, clipper);
                mapnik::util::apply_visitor(transformer, geom);
            }
        }
        return true;
    }

    inline eAttributeCollectionPolicy attribute_collection_policy() const
    {
        return DEFAULT;
    }

    inline double scale_factor() const
    {
        return common_.scale_factor_;
    }

    inline attributes const& variables() const
    {
        return common_.vars_;
    }

private:
    std::string image_format_;
    double area_threshold_;
    double simplify_distance_;
    polygon_fill_type fill_type_;
    scaling_method_e scaling_method_;
    bool strictly_simple_;
    bool multi_polygon_union_;
    bool process_all_rings_;
    //renderer_common common_;
    void setup(Map const & m, buffer_type & pixmap);
};

} // namespace mapnik

#endif // __MAPNIK_VECTOR_TILE_STYLE_LEVEL_PROCESSOR__
