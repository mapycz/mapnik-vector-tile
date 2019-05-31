#pragma once
// mapnik-vector-tile
#include "vector_tile_config.hpp"

// boost
#include <boost/geometry/algorithms/unique.hpp>

namespace mapnik
{

namespace vector_tile_impl
{

template <typename NextProcessor>
struct unique_points
{
    NextProcessor & next_;

    unique_points(NextProcessor & next) : next_(next)
    {
    }

    void operator() (mapbox::geometry::point<std::int64_t> & geom)
    {
        next_(geom);
    }

    void operator() (mapbox::geometry::multi_point<std::int64_t> & geom)
    {
        auto last = std::unique(geom.begin(), geom.end());
        geom.erase(last, geom.end());
        next_(geom);
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
        boost::geometry::unique(geom);
        next_(geom);
    }

    void operator() (mapbox::geometry::multi_line_string<std::int64_t> & geom)
    {
        boost::geometry::unique(geom);
        next_(geom);
    }

    void operator() (mapbox::geometry::polygon<std::int64_t> & geom)
    {
        next_(geom);
    }

    void operator() (mapbox::geometry::multi_polygon<std::int64_t> & geom)
    {
        next_(geom);
    }
};

}

}
