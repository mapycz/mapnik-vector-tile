#pragma once
// mapnik-vector-tile
#include "vector_tile_config.hpp"

// mapnik
#include <mapnik/box2d.hpp>

// mapbox
#include <mapbox/geometry/geometry.hpp>
#include <mapbox/geometry/envelope.hpp>

namespace mapnik
{

namespace vector_tile_impl
{

template <typename Geom>
mapnik::box2d<std::int64_t> make_envelope(Geom const & g)
{
    const mapbox::geometry::box<std::int64_t> envelope(
        mapbox::geometry::envelope(g));
    return mapnik::box2d<std::int64_t>(
        envelope.min.x, envelope.min.y,
        envelope.max.x, envelope.max.y);
}

template <typename Geom>
struct indexed_geom
{
    indexed_geom(Geom const & g) :
        geom(g), envelope(make_envelope(g))
    {
    }

    Geom const & geom;
    mapnik::box2d<std::int64_t> envelope;
};

template <typename Geom>
struct indexed_multi_geom
{
    indexed_multi_geom(Geom const & multi)
    {
        for (auto const & geom : multi)
        {
            geoms.emplace_back(geom);
        }

        bool first;
        for (auto const & geom : geoms)
        {
            if (first)
            {
                envelope = geom.envelope;
                first = false;
            }
            else
            {
                envelope.expand_to_include(geom.envelope);
            }
        }
    }

    std::vector<indexed_geom<typename Geom::value_type>> geoms;
    mapnik::box2d<std::int64_t> envelope;
};

using indexed_point = indexed_geom<mapbox::geometry::point<std::int64_t>>;
using indexed_multi_point = indexed_geom<mapbox::geometry::multi_point<std::int64_t>>;
using indexed_line_string = indexed_geom<mapbox::geometry::line_string<std::int64_t>>;
using indexed_multi_line_string = indexed_multi_geom<mapbox::geometry::multi_line_string<std::int64_t>>;
using indexed_polygon = indexed_geom<mapbox::geometry::polygon<std::int64_t>>;
using indexed_multi_polygon = indexed_multi_geom<mapbox::geometry::multi_polygon<std::int64_t>>;

template <typename NextProcessor>
struct geometry_indexer
{
    NextProcessor & next_;

    geometry_indexer(NextProcessor & next) : next_(next)
    {
    }

    void operator() (mapbox::geometry::point<std::int64_t> & geom)
    {
        indexed_point indexed(geom);
        next_(indexed);
    }

    void operator() (mapbox::geometry::multi_point<std::int64_t> & geom)
    {
        indexed_multi_point indexed(geom);
        next_(indexed);
    }

    void operator() (mapbox::geometry::geometry_collection<std::int64_t> & geom)
    {
        for (auto & g : geom)
        {
            mapbox::util::apply_visitor((*this), g);
        }
    }

    void operator() (mapbox::geometry::line_string<std::int64_t> & geom)
    {
        indexed_line_string indexed(geom);
        next_(indexed);
    }

    void operator() (mapbox::geometry::multi_line_string<std::int64_t> & geom)
    {
        indexed_multi_line_string indexed(geom);
        next_(indexed);
    }

    void operator() (mapbox::geometry::polygon<std::int64_t> & geom)
    {
        indexed_polygon indexed(geom);
        next_(indexed);
    }

    void operator() (mapbox::geometry::multi_polygon<std::int64_t> & geom)
    {
        indexed_multi_polygon indexed(geom);
        next_(indexed);
    }
};

}
}
