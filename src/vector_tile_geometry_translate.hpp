#pragma once

// mapnik-vector-tile
#include "vector_tile_config.hpp"

// mapbox
#include <mapbox/geometry/geometry.hpp>

namespace mapnik
{

namespace vector_tile_impl
{

template <typename NextProcessor>
class geometry_translate 
{
    std::int32_t tx_;
    std::int32_t ty_;
    NextProcessor & next_;

public:
    geometry_translate(std::int32_t tx,
                       std::int32_t ty,
                       NextProcessor & next) :
          tx_(tx),
          ty_(ty),
          next_(next)
    {
    }

    void operator() (mapbox::geometry::point<std::int64_t> & geom)
    {
        geom.x += tx_;
        geom.y += ty_;
        next_(geom);
    }

    void operator() (mapbox::geometry::multi_point<std::int64_t> & geom)
    {
        for (auto & point : geom)
        {
            point.x += tx_;
            point.y += ty_;
        }
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
        for (auto & point : geom)
        {
            point.x += tx_;
            point.y += ty_;
        }
        next_(geom);
    }

    void operator() (mapbox::geometry::multi_line_string<std::int64_t> & geom)
    {
        for (auto & ls : geom)
        {
            for (auto & point : ls)
            {
                point.x += tx_;
                point.y += ty_;
            }
        }
        next_(geom);
    }

    void operator() (mapbox::geometry::polygon<std::int64_t> & geom)
    {
        for (auto & ring : geom)
        {
            for (auto & point : ring)
            {
                point.x += tx_;
                point.y += ty_;
            }
        }
        next_(geom);
    }

    void operator() (mapbox::geometry::multi_polygon<std::int64_t> & geom)
    {
        for (auto & poly : geom)
        {
            for (auto & ring : poly)
            {
                for (auto & point : ring)
                {
                    point.x += tx_;
                    point.y += ty_;
                }
            }
        }
        next_(geom);
    }
};

} // end ns vector_tile_impl
} // end ns mapnik
