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
struct simple_tiler
{
    Tile & tile_;
    tile_layer & layer_;
    layer_builder_pbf builder_;

    simple_tiler(Tile & tile, tile_layer & layer) :
        tile_(tile),
        layer_(layer),
        builder_(layer.name(), layer.layer_extent(), layer.get_data())
    {
    }

    tile_layer & layer()
    {
        return layer_;
    }

    struct visitor
    {
        visitor(visitor &&) = default;

        using Encoder = mapnik::vector_tile_impl::geometry_to_feature_pbf_visitor;

        mapnik::feature_impl const& mapnik_feature_;
        Encoder encoder_;

        visitor(mapnik::feature_impl const& mapnik_feature,
                layer_builder_pbf & builder) :
            mapnik_feature_(mapnik_feature),
            encoder_(mapnik_feature, builder)
        {}

        template <typename T>
        void operator() (T const& geom)
        {
            encoder_(geom);
        }
    };

    visitor get_visitor(mapnik::feature_impl const& mapnik_feature_,
                        clipper_params const &)
    {
        return visitor(mapnik_feature_, builder_);
    }
};

struct wafer_tiler
{
    merc_wafer & wafer_;
    wafer_layer & layer_;
    std::deque<layer_builder_pbf> builders_;
    std::uint32_t tile_size_;
    std::int32_t buffer_size_;

    wafer_tiler(merc_wafer & wafer, wafer_layer & layer) :
        wafer_(wafer),
        layer_(layer)
    {
        merc_tile const & tile = wafer_.tiles().front();
        tile_size_ = tile.tile_size();
        buffer_size_ = tile.buffer_size();

        for (auto & buffer : layer_.buffers())
        {
            builders_.emplace_back(layer.name(), tile_size_, buffer);
        }
    }

    struct visitor
    {
        visitor(visitor &&) = default;

        using Encoder = geometry_to_feature_pbf_visitor;
        using Translator = geometry_translate<Encoder>;
        using Clipper = geometry_clipper<Translator>;

        wafer_tiler & tiler_;
        mapnik::feature_impl const& mapnik_feature_;
        std::deque<Encoder> encoders_;
        clipper_params const & clipper_params_;

        visitor(wafer_tiler & tiler,
                mapnik::feature_impl const& mapnik_feature,
                std::deque<layer_builder_pbf> & builders,
                clipper_params const & clip_params) :
            tiler_(tiler),
            mapnik_feature_(mapnik_feature),
            clipper_params_(clip_params)
        {
            for (auto & builder : builders)
            {
                encoders_.emplace_back(mapnik_feature, builder);
            }
        }

        template <typename T>
        void operator() (T const& geom)
        {
            auto encoder = encoders_.begin();
            std::int32_t wafer_size = tiler_.wafer_.tile_size();
            for (std::int32_t y = 0; y < wafer_size; y += tiler_.tile_size_)
            {
                for (std::int32_t x = 0; x < wafer_size; x += tiler_.tile_size_)
                {
                    T geom_copy(geom);
                    mapnik::box2d<std::int32_t> tile_box(
                        x, y, x + tiler_.tile_size_, y + tiler_.tile_size_);
                    tile_box.pad(tiler_.buffer_size_);
                    Translator translate(-x, -y, *encoder);
                    Clipper clipper(tile_box, clipper_params_, translate);
                    clipper(geom_copy);
                    ++encoder;
                }
            }
        }
    };

    visitor get_visitor(mapnik::feature_impl const& mapnik_feature_,
                        clipper_params const & clip_params)
    {
        return visitor(*this, mapnik_feature_, builders_, clip_params);
    }
};

template <typename Tile>
struct tile_traits
{
};

template <>
struct tile_traits<tile>
{
    using Layer = tile_layer;
    using Tiler = simple_tiler<tile>;
};

template <>
struct tile_traits<merc_tile>
{
    using Layer = tile_layer;
    using Tiler = simple_tiler<merc_tile>;
};

template <>
struct tile_traits<merc_wafer>
{
    using Layer = wafer_layer;
    using Tiler = wafer_tiler;
};

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

    using clipping_process = mapnik::vector_tile_impl::geometry_clipper<typename Tiler::visitor>;

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
    const clipper_params clip_params {
        area_threshold, strictly_simple, multi_polygon_union,
        fill_type, process_all_rings };

    if (simplify_distance > 0)
    {
        using simplifier_process = mapnik::vector_tile_impl::geometry_simplifier<clipping_process>;
        if (layer.get_proj_transform().equal())
        {
            using strategy_type = mapnik::vector_tile_impl::vector_tile_strategy;
            using transform_type = mapnik::vector_tile_impl::transform_visitor<strategy_type, simplifier_process>;
            while (feature)
            {
                if (!style_level_filter || layer.evaluate_feature(*feature, active_rules))
                {
                    mapnik::geometry::geometry<double> const& geom = feature->get_geometry();
                    typename Tiler::visitor tiler_visitor(tiler.get_visitor(*feature, clip_params));
                    clipping_process clipper(tile_clipping_extent, clip_params, tiler_visitor);
                    simplifier_process simplifier(simplify_distance, clipper);
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
                    typename Tiler::visitor tiler_visitor(tiler.get_visitor(*feature, clip_params));
                    clipping_process clipper(tile_clipping_extent, clip_params, tiler_visitor);
                    simplifier_process simplifier(simplify_distance, clipper);
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
            using transform_type = mapnik::vector_tile_impl::transform_visitor<strategy_type, clipping_process>;
            while (feature)
            {
                if (!style_level_filter || layer.evaluate_feature(*feature, active_rules))
                {
                    mapnik::geometry::geometry<double> const& geom = feature->get_geometry();
                    typename Tiler::visitor tiler_visitor(tiler.get_visitor(*feature, clip_params));
                    clipping_process clipper(tile_clipping_extent, clip_params, tiler_visitor);
                    transform_type transformer(vs, buffered_extent, clipper);
                    mapnik::util::apply_visitor(transformer, geom);
                }
                feature = features->next();
            }
        }
        else
        {
            using strategy_type = mapnik::vector_tile_impl::vector_tile_strategy_proj;
            using transform_type = mapnik::vector_tile_impl::transform_visitor<strategy_type, clipping_process>;
            strategy_type vs2(layer.get_proj_transform(), layer.get_view_transform());
            mapnik::box2d<double> const& trans_buffered_extent = layer.get_source_buffered_extent();
            while (feature)
            {
                if (!style_level_filter || layer.evaluate_feature(*feature, active_rules))
                {
                    mapnik::geometry::geometry<double> const& geom = feature->get_geometry();
                    typename Tiler::visitor tiler_visitor(tiler.get_visitor(*feature, clip_params));
                    clipping_process clipper(tile_clipping_extent, clip_params, tiler_visitor);
                    transform_type transformer(vs2, trans_buffered_extent, clipper);
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
    using Layer = typename detail::tile_traits<Tile>::Layer;
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

} // end ns vector_tile_impl

} // end ns mapnik
