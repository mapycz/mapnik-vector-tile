#pragma once
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
#include <mapnik/box2d_impl.hpp>
#include <mapnik/feature.hpp>

namespace mapnik
{

namespace vector_tile_impl
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

    ~simple_tiler()
    {
        builder_.finalize();
    }

    tile_layer & layer()
    {
        return layer_;
    }

    struct visitor
    {
        visitor(visitor &&) = default;

        using Encoder = mapnik::vector_tile_impl::geometry_to_feature_pbf_visitor;
        using Clipper = geometry_clipper<Encoder>;

        mapnik::feature_impl const& mapnik_feature_;
        Encoder encoder_;
        clipper_params const & clipper_params_;
        mapnik::box2d<std::int64_t> tile_box_;

        template <typename T>
        visitor(T & tile,
                mapnik::feature_impl const& mapnik_feature,
                layer_builder_pbf & builder,
                clipper_params const & clip_params) :
            mapnik_feature_(mapnik_feature),
            encoder_(mapnik_feature, builder),
            clipper_params_(clip_params),
            tile_box_(0, 0, tile.tile_size(), tile.tile_size())
        {
            tile_box_.pad(tile.buffer_size());
        }

        template <typename T>
        void operator() (T const& indexed_geom)
        {
            Clipper clipper(tile_box_, clipper_params_, encoder_);
            clipper(indexed_geom);
        }
    };

    visitor get_visitor(mapnik::feature_impl const& mapnik_feature_,
                        clipper_params const & clip_params)
    {
        return visitor(tile_, mapnik_feature_, builder_, clip_params);
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

    ~wafer_tiler()
    {
        for (auto & builder : builders_)
        {
            builder.finalize();
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
        void operator() (T const& indexed_geom)
        {
            auto encoder = encoders_.begin();
            std::int32_t wafer_size = tiler_.wafer_.tile_size();
            for (std::int64_t y = 0; y < wafer_size; y += tiler_.tile_size_)
            {
                for (std::int64_t x = 0; x < wafer_size; x += tiler_.tile_size_)
                {
                    mapnik::box2d<std::int64_t> tile_box(
                        x, y, x + tiler_.tile_size_, y + tiler_.tile_size_);
                    tile_box.pad(tiler_.buffer_size_);
                    if (indexed_geom.envelope.intersects(tile_box))
                    {
                        Translator translate(-x, -y, *encoder);
                        Clipper clipper(tile_box, clipper_params_, translate);
                        clipper(indexed_geom);
                    }
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

}

}
