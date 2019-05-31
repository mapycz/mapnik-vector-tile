// mapnik-vector-tile
#include "vector_tile_geometry_clipper.hpp"
#include "vector_tile_geometry_feature.hpp"
#include "vector_tile_geometry_simplifier.hpp"
#include "vector_tile_geometry_translate.hpp"
#include "vector_tile_raster_clipper.hpp"
#include "vector_tile_strategy.hpp"
#include "vector_tile_tile.hpp"
#include "vector_tile_wafer.hpp"
#include "vector_tile_layer.hpp"
#include "tiler.hpp"
#include "unique_points.hpp"

// mapnik
#include <mapnik/box2d.hpp>
#include <mapnik/datasource.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/image_scaling.hpp>
#include <mapnik/layer.hpp>
#include <mapnik/map.hpp>
#include <mapnik/version.hpp>
#include <mapnik/attribute.hpp>
#include <mapnik/geometry_transform.hpp>

// boost
#include <boost/optional.hpp>

// std
#include <future>

namespace mapnik
{

namespace vector_tile_impl
{

namespace detail
{

template <typename Tile>
inline void create_geom_layer(Tile & tile,
                              typename tile_traits<Tile>::Layer & layer,
                              double simplify_distance,
                              double area_threshold,
                              polygon_fill_type fill_type,
                              bool strictly_simple,
                              bool multi_polygon_union,
                              bool process_all_rings,
                              bool style_level_filter)
{
    using Tiler = typename tile_traits<Tile>::Tiler;
    Tiler tiler(tile, layer);
    std::vector<mapnik::rule_cache> active_rules(layer.get_active_rules());

    // query for the features
    mapnik::featureset_ptr features = layer.get_features();

    if (!features)
    {
        return;
    }

    mapnik::feature_ptr feature = features->next();

    if (!feature)
    {
        return;
    }

    using tiler_proc = typename Tiler::visitor;
    using indexer_proc = geometry_indexer<tiler_proc>;
    using uniquer_proc = unique_points<indexer_proc>;

    mapnik::vector_tile_impl::vector_tile_strategy vs(layer.get_view_transform());
    mapnik::box2d<double> const& buffered_extent = layer.get_target_buffered_extent();
    const clipper_params clip_params {
        area_threshold, strictly_simple, multi_polygon_union,
        fill_type, process_all_rings };

    if (simplify_distance > 0)
    {
        using simplifier_process = mapnik::vector_tile_impl::geometry_simplifier<uniquer_proc>;
        if (layer.get_proj_transform().equal())
        {
            using strategy_type = mapnik::vector_tile_impl::vector_tile_strategy;
            using transform_type = mapnik::vector_tile_impl::transform_visitor<strategy_type, simplifier_process>;
            while (feature)
            {
                if (!style_level_filter || layer.evaluate_feature(*feature, active_rules))
                {
                    mapnik::geometry::geometry<double> const& geom = feature->get_geometry();
                    tiler_proc tiler_visitor(tiler.get_visitor(*feature, clip_params));
                    indexer_proc indexer(tiler_visitor);
                    uniquer_proc uniquer(indexer);
                    simplifier_process simplifier(simplify_distance, uniquer);
                    transform_type transformer(vs, buffered_extent, simplifier);
                    mapnik::util::apply_visitor(transformer, geom);
                }
                feature = features->next();
            }
        }
        else
        {
            using strategy_type = mapnik::vector_tile_impl::vector_tile_strategy_proj;
            using transform_type = mapnik::vector_tile_impl::transform_visitor<strategy_type, simplifier_process>;
            strategy_type vs2(layer.get_proj_transform(), layer.get_view_transform());
            mapnik::box2d<double> const& trans_buffered_extent = layer.get_source_buffered_extent();
            while (feature)
            {
                if (!style_level_filter || layer.evaluate_feature(*feature, active_rules))
                {
                    mapnik::geometry::geometry<double> const& geom = feature->get_geometry();
                    tiler_proc tiler_visitor(tiler.get_visitor(*feature, clip_params));
                    indexer_proc indexer(tiler_visitor);
                    uniquer_proc uniquer(indexer);
                    simplifier_process simplifier(simplify_distance, uniquer);
                    transform_type transformer(vs2, trans_buffered_extent, simplifier);
                    mapnik::util::apply_visitor(transformer, geom);
                }
                feature = features->next();
            }
        }
    }
    else
    {
        if (layer.get_proj_transform().equal())
        {
            using strategy_type = mapnik::vector_tile_impl::vector_tile_strategy;
            using transform_type = mapnik::vector_tile_impl::transform_visitor<strategy_type, uniquer_proc>;
            while (feature)
            {
                if (!style_level_filter || layer.evaluate_feature(*feature, active_rules))
                {
                    mapnik::geometry::geometry<double> const& geom = feature->get_geometry();
                    tiler_proc tiler_visitor(tiler.get_visitor(*feature, clip_params));
                    indexer_proc indexer(tiler_visitor);
                    uniquer_proc uniquer(indexer);
                    transform_type transformer(vs, buffered_extent, uniquer);
                    mapnik::util::apply_visitor(transformer, geom);
                }
                feature = features->next();
            }
        }
        else
        {
            using strategy_type = mapnik::vector_tile_impl::vector_tile_strategy_proj;
            using transform_type = mapnik::vector_tile_impl::transform_visitor<strategy_type, uniquer_proc>;
            strategy_type vs2(layer.get_proj_transform(), layer.get_view_transform());
            mapnik::box2d<double> const& trans_buffered_extent = layer.get_source_buffered_extent();
            while (feature)
            {
                if (!style_level_filter || layer.evaluate_feature(*feature, active_rules))
                {
                    mapnik::geometry::geometry<double> const& geom = feature->get_geometry();
                    tiler_proc tiler_visitor(tiler.get_visitor(*feature, clip_params));
                    indexer_proc indexer(tiler_visitor);
                    uniquer_proc uniquer(indexer);
                    transform_type transformer(vs2, trans_buffered_extent, uniquer);
                    mapnik::util::apply_visitor(transformer, geom);
                }
                feature = features->next();
            }
        }
    }
}

template <typename Layer>
inline void create_raster_layer(Layer & layer,
                                std::string const& image_format,
                                scaling_method_e scaling_method)
{
    layer_builder_pbf builder(layer.name(), layer.layer_extent(), layer.get_data());

    // query for the features
    mapnik::featureset_ptr features = layer.get_features();

    if (!features)
    {
        return;
    }

    mapnik::feature_ptr feature = features->next();

    if (!feature)
    {
        return;
    }

    mapnik::raster_ptr const& source = feature->get_raster();
    if (!source)
    {
        return;
    }

    mapnik::box2d<double> target_ext = box2d<double>(source->ext_);

    layer.get_proj_transform().backward(target_ext, PROJ_ENVELOPE_POINTS);
    mapnik::box2d<double> ext = layer.get_view_transform().forward(target_ext);

    int start_x = static_cast<int>(std::floor(ext.minx()+.5));
    int start_y = static_cast<int>(std::floor(ext.miny()+.5));
    int end_x = static_cast<int>(std::floor(ext.maxx()+.5));
    int end_y = static_cast<int>(std::floor(ext.maxy()+.5));
    int raster_width = end_x - start_x;
    int raster_height = end_y - start_y;
    if (raster_width > 0 && raster_height > 0)
    {
        //builder.make_painted();
        raster_clipper visit(*source,
                             target_ext,
                             ext,
                             layer.get_proj_transform(),
                             image_format,
                             scaling_method,
                             layer.layer_extent(),
                             layer.layer_extent(),
                             raster_width,
                             raster_height,
                             start_x,
                             start_y);
        std::string buffer = mapnik::util::apply_visitor(visit, source->data_);
        raster_to_feature(buffer, *feature, builder);
    }
}

} // end ns detail

template <typename Parent, typename Tile, typename Layer>
void processor::append_sublayers(Parent const& parent,
                                 std::vector<Layer> & tile_layers,
                                 Tile & t,
                                 double scale_denom,
                                 int offset_x,
                                 int offset_y,
                                 bool style_level_filter) const
{
    for (mapnik::layer const& lay : parent.layers())
    {
        if (t.has_layer(lay.name()))
        {
            continue;
        }
        tile_layers.emplace_back(m_, lay, t,
                             scale_factor_,
                             scale_denom,
                             offset_x,
                             offset_y,
                             style_level_filter,
                             vars_);
        if (!tile_layers.back().is_valid())
        {
            t.add_empty_layer(lay.name());
            tile_layers.pop_back();
            continue;
        }

        append_sublayers(lay, tile_layers, t, scale_denom, offset_x, offset_y,
                         style_level_filter);
    }
}

template <typename Tile>
MAPNIK_VECTOR_INLINE void processor::update_tile(Tile & t,
                                                 double scale_denom,
                                                 int offset_x,
                                                 int offset_y,
                                                 bool style_level_filter)
{
    // Futures
    using Layer = typename tile_traits<Tile>::Layer;
    std::vector<Layer> tile_layers;

    append_sublayers(m_, tile_layers, t, scale_denom, offset_x, offset_y,
                     style_level_filter);

    if (threading_mode_ == std::launch::deferred)
    {
        for (auto & layer : tile_layers)
        {
            if (layer.get_ds()->type() == datasource::Vector)
            {
                detail::create_geom_layer(t, layer,
                                          simplify_distance_,
                                          area_threshold_,
                                          fill_type_,
                                          strictly_simple_,
                                          multi_polygon_union_,
                                          process_all_rings_,
                                          style_level_filter
                                         );
            }
            else // Raster
            {
                detail::create_raster_layer(layer,
                                            image_format_,
                                            scaling_method_
                                           );
            }
        }
    }
    else
    {
        std::vector<std::future<void> > future_layers;
        future_layers.reserve(tile_layers.size());

        for (auto & layer_ref : tile_layers)
        {
            if (layer_ref.get_ds()->type() == datasource::Vector)
            {
                future_layers.push_back(std::async(
                                        threading_mode_,
                                        detail::create_geom_layer<Tile>,
                                        std::ref(t),
                                        std::ref(layer_ref),
                                        simplify_distance_,
                                        area_threshold_,
                                        fill_type_,
                                        strictly_simple_,
                                        multi_polygon_union_,
                                        process_all_rings_,
                                        style_level_filter
                            ));
            }
            else // Raster
            {
                future_layers.push_back(std::async(
                                        threading_mode_,
                                        detail::create_raster_layer<Layer>,
                                        std::ref(layer_ref),
                                        image_format_,
                                        scaling_method_
                ));
            }
        }

        for (auto && lay_future : future_layers)
        {
            if (!lay_future.valid())
            {
                throw std::runtime_error("unexpected invalid async return");
            }
            lay_future.get();
        }
    }

    for (auto & layer_ref : tile_layers)
    {
        t.add_layer(layer_ref);
    }
}

template
void processor::update_tile(tile & t,
                            double scale_denom,
                            int offset_x,
                            int offset_y,
                            bool style_level_filter);

template
void processor::update_tile(merc_tile & t,
                            double scale_denom,
                            int offset_x,
                            int offset_y,
                            bool style_level_filter);

template
void processor::update_tile(merc_wafer & t,
                            double scale_denom,
                            int offset_x,
                            int offset_y,
                            bool style_level_filter);

} // end ns vector_tile_impl

} // end ns mapnik
