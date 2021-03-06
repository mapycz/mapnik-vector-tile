#pragma once
// mapnik-vector-tile
#include "vector_tile_config.hpp"
#include "geometry_indexer.hpp"

// mapnik
#include <mapnik/box2d.hpp>

// mapbox
#include <mapbox/geometry/geometry.hpp>
#include <mapbox/geometry/envelope.hpp>
#include <mapbox/geometry/wagyu/quick_clip.hpp>
#include <mapbox/geometry/wagyu/wagyu.hpp>

// boost
#pragma GCC diagnostic push
#include <mapnik/warning_ignore.hpp>
#include <iostream>
#include <boost/geometry/algorithms/intersection.hpp>
#pragma GCC diagnostic pop

namespace mapnik
{

namespace vector_tile_impl
{

namespace detail
{

template <typename T>
double area(mapbox::geometry::linear_ring<T> const& poly) {
    std::size_t size = poly.size();
    if (size < 3) {
        return 0.0;
    }

    double a = 0.0;
    auto itr = poly.begin();
    auto itr_prev = poly.end();
    --itr_prev;
    a += static_cast<double>(itr_prev->x + itr->x) * static_cast<double>(itr_prev->y - itr->y);
    ++itr;
    itr_prev = poly.begin();
    for (; itr != poly.end(); ++itr, ++itr_prev) {
        a += static_cast<double>(itr_prev->x + itr->x) * static_cast<double>(itr_prev->y - itr->y);
    }
    return -a * 0.5;
}

inline mapbox::geometry::wagyu::fill_type get_wagyu_fill_type(polygon_fill_type type)
{
    switch (type)
    {
    case polygon_fill_type_max:
    case even_odd_fill:
        return mapbox::geometry::wagyu::fill_type_even_odd;
    case non_zero_fill:
        return mapbox::geometry::wagyu::fill_type_non_zero;
    case positive_fill:
        return mapbox::geometry::wagyu::fill_type_positive;
    case negative_fill:
        return mapbox::geometry::wagyu::fill_type_negative;
    }
    // Added return here to make gcc happy
    return mapbox::geometry::wagyu::fill_type_even_odd;
}

} // end ns detail

struct clipper_params
{
    double area_threshold;
    bool strictly_simple;
    bool multi_polygon_union;
    polygon_fill_type fill_type;
    bool process_all_rings;
};

template <typename NextProcessor>
class geometry_clipper
{
    NextProcessor & next_;
    mapnik::box2d<std::int64_t> const& tile_clipping_extent_;
    clipper_params const & params_;

public:
    geometry_clipper(mapnik::box2d<std::int64_t> const& tile_clipping_extent,
                     clipper_params const & params,
                     NextProcessor & next) :
              next_(next),
              tile_clipping_extent_(tile_clipping_extent),
              params_(params)
    {
    }

    void operator() (indexed_point const & geom)
    {
        if (tile_clipping_extent_.intersects(geom.geom.x, geom.geom.y))
        {
            mapbox::geometry::point<std::int64_t> point(geom.geom);
            next_(point);
        }
    }

    void operator() (indexed_multi_point const & geom)
    {
        mapbox::geometry::multi_point<std::int64_t> intersection;
        std::copy_if(geom.geom.begin(), geom.geom.end(),
            std::back_inserter(intersection),
            [&](mapbox::geometry::point<std::int64_t> const & p)
            {
                return tile_clipping_extent_.intersects(p.x, p.y);
            });
        if (!intersection.empty())
        {
            next_(intersection);
        }
    }

    void operator() (mapbox::geometry::geometry_collection<std::int64_t> const & geom)
    {
        for (auto & g : geom)
        {
            mapbox::util::apply_visitor((*this), g);
        }
    }

    void operator() (indexed_line_string const & geom)
    {
        if (geom.geom.size() < 2)
        {
            return;
        }
        mapbox::geometry::multi_line_string<int64_t> result;
        mapbox::geometry::linear_ring<std::int64_t> clip_box;
        clip_box.reserve(5);
        clip_box.emplace_back(tile_clipping_extent_.minx(),tile_clipping_extent_.miny());
        clip_box.emplace_back(tile_clipping_extent_.maxx(),tile_clipping_extent_.miny());
        clip_box.emplace_back(tile_clipping_extent_.maxx(),tile_clipping_extent_.maxy());
        clip_box.emplace_back(tile_clipping_extent_.minx(),tile_clipping_extent_.maxy());
        clip_box.emplace_back(tile_clipping_extent_.minx(),tile_clipping_extent_.miny());
        boost::geometry::intersection(clip_box, geom.geom, result);
        if (result.empty())
        {
            return;
        }
        next_(result);
    }

    void operator() (indexed_multi_line_string const & geom)
    {
        if (geom.geoms.empty())
        {
            return;
        }

        mapbox::geometry::linear_ring<std::int64_t> clip_box;
        clip_box.reserve(5);
        clip_box.emplace_back(tile_clipping_extent_.minx(),tile_clipping_extent_.miny());
        clip_box.emplace_back(tile_clipping_extent_.maxx(),tile_clipping_extent_.miny());
        clip_box.emplace_back(tile_clipping_extent_.maxx(),tile_clipping_extent_.maxy());
        clip_box.emplace_back(tile_clipping_extent_.minx(),tile_clipping_extent_.maxy());
        clip_box.emplace_back(tile_clipping_extent_.minx(),tile_clipping_extent_.miny());
        mapbox::geometry::multi_line_string<int64_t> results;
        for (auto const& indexed_line : geom.geoms)
        {
            if (indexed_line.geom.size() < 2)
            {
               continue;
            }
            if (!tile_clipping_extent_.intersects(indexed_line.envelope))
            {
                continue;
            }
            boost::geometry::intersection(clip_box, indexed_line.geom, results);
        }
        if (results.empty())
        {
            return;
        }
        next_(results);
    }

    void operator() (indexed_polygon const & geom)
    {
        if (geom.geom.empty() || ((geom.geom.front().size() < 3) && !params_.process_all_rings))
        {
            return;
        }

        mapbox::geometry::wagyu::wagyu<std::int64_t> clipper;
        mapbox::geometry::point<std::int64_t> min_pt(tile_clipping_extent_.minx(), tile_clipping_extent_.miny());
        mapbox::geometry::point<std::int64_t> max_pt(tile_clipping_extent_.maxx(), tile_clipping_extent_.maxy());
        mapbox::geometry::box<std::int64_t> b(min_pt, max_pt);

        bool first = true;
        for (auto & ring : geom.geom) {
            if (ring.size() < 3)
            {
                if (first) {
                    if (params_.process_all_rings) {
                        first = false;
                    } else {
                        return;
                    }
                }
                continue;
            }
            double area = detail::area(ring);
            if (first) {
                first = false;
                if ((std::abs(area) < params_.area_threshold)  && !params_.process_all_rings) {
                    return;
                }
                mapbox::geometry::linear_ring<std::int64_t> reversed(ring);
                if (area < 0) {
                    std::reverse(reversed.begin(), reversed.end());
                }
                auto new_ring = mapbox::geometry::wagyu::quick_clip::quick_lr_clip(reversed, b);
                if (new_ring.empty()) {
                    if (params_.process_all_rings) {
                        continue;
                    }
                    return;
                }
                clipper.add_ring(new_ring);
            } else {
                if (std::abs(area) < params_.area_threshold) {
                    continue;
                }
                mapbox::geometry::linear_ring<std::int64_t> reversed(ring);
                if (area > 0)
                {
                    std::reverse(reversed.begin(), reversed.end());
                }
                auto new_ring = mapbox::geometry::wagyu::quick_clip::quick_lr_clip(reversed, b);
                if (new_ring.empty()) {
                    continue;
                }
                clipper.add_ring(new_ring);
            }
        }

        mapbox::geometry::multi_polygon<std::int64_t> mp;

        clipper.execute(mapbox::geometry::wagyu::clip_type_union,
                        mp,
                        detail::get_wagyu_fill_type(params_.fill_type),
                        mapbox::geometry::wagyu::fill_type_even_odd);

        if (mp.empty())
        {
            return;
        }
        next_(mp);
    }

    void operator() (indexed_multi_polygon const & geom)
    {
        if (geom.geoms.empty())
        {
            return;
        }

        mapbox::geometry::point<std::int64_t> min_pt(tile_clipping_extent_.minx(), tile_clipping_extent_.miny());
        mapbox::geometry::point<std::int64_t> max_pt(tile_clipping_extent_.maxx(), tile_clipping_extent_.maxy());
        mapbox::geometry::box<std::int64_t> b(min_pt, max_pt);
        mapbox::geometry::multi_polygon<std::int64_t> mp;

        if (params_.multi_polygon_union)
        {
            mapbox::geometry::wagyu::wagyu<std::int64_t> clipper;
            for (auto const & indexed_poly : geom.geoms)
            {
                if (!tile_clipping_extent_.intersects(indexed_poly.envelope))
                {
                    continue;
                }
                bool first = true;
                for (auto const & ring : indexed_poly.geom) {
                    if (ring.size() < 3)
                    {
                        if (first) {
                            first = false;
                            if (!params_.process_all_rings) {
                                break;
                            }
                        }
                        continue;
                    }
                    double area = detail::area(ring);
                    if (first) {
                        first = false;
                        if ((std::abs(area) < params_.area_threshold)  && !params_.process_all_rings) {
                            break;
                        }
                        mapbox::geometry::linear_ring<std::int64_t> reversed(ring);
                        if (area < 0) {
                            std::reverse(reversed.begin(), reversed.end());
                        }
                        auto new_ring = mapbox::geometry::wagyu::quick_clip::quick_lr_clip(reversed, b);
                        if (new_ring.empty()) {
                            if (params_.process_all_rings) {
                                continue;
                            }
                            break;
                        }
                        clipper.add_ring(new_ring);
                    } else {
                        if (std::abs(area) < params_.area_threshold) {
                            continue;
                        }
                        mapbox::geometry::linear_ring<std::int64_t> reversed(ring);
                        if (area > 0)
                        {
                            std::reverse(reversed.begin(), reversed.end());
                        }
                        auto new_ring = mapbox::geometry::wagyu::quick_clip::quick_lr_clip(reversed, b);
                        if (new_ring.empty()) {
                            continue;
                        }
                        clipper.add_ring(new_ring);
                    }
                }
            }
            clipper.execute(mapbox::geometry::wagyu::clip_type_union,
                            mp,
                            detail::get_wagyu_fill_type(params_.fill_type),
                            mapbox::geometry::wagyu::fill_type_even_odd);
        }
        else
        {
            for (auto const & indexed_poly : geom.geoms)
            {
                if (!tile_clipping_extent_.intersects(indexed_poly.envelope))
                {
                    continue;
                }
                mapbox::geometry::wagyu::wagyu<std::int64_t> clipper;
                mapbox::geometry::multi_polygon<std::int64_t> tmp_mp;
                bool first = true;
                for (auto const & ring : indexed_poly.geom) {
                    if (ring.size() < 3)
                    {
                        if (first) {
                            first = false;
                            if (!params_.process_all_rings) {
                                break;
                            }
                        }
                        continue;
                    }
                    double area = detail::area(ring);
                    if (first) {
                        first = false;
                        if ((std::abs(area) < params_.area_threshold)  && !params_.process_all_rings) {
                            break;
                        }
                        mapbox::geometry::linear_ring<std::int64_t> reversed(ring);
                        if (area < 0) {
                            std::reverse(reversed.begin(), reversed.end());
                        }
                        auto new_ring = mapbox::geometry::wagyu::quick_clip::quick_lr_clip(reversed, b);
                        if (new_ring.empty()) {
                            if (params_.process_all_rings) {
                                continue;
                            }
                            break;
                        }
                        clipper.add_ring(new_ring);
                    } else {
                        if (std::abs(area) < params_.area_threshold) {
                            continue;
                        }
                        mapbox::geometry::linear_ring<std::int64_t> reversed(ring);
                        if (area > 0)
                        {
                            std::reverse(reversed.begin(), reversed.end());
                        }
                        auto new_ring = mapbox::geometry::wagyu::quick_clip::quick_lr_clip(reversed, b);
                        if (new_ring.empty()) {
                            continue;
                        }
                        clipper.add_ring(new_ring);
                    }
                }
                clipper.execute(mapbox::geometry::wagyu::clip_type_union,
                                tmp_mp,
                                detail::get_wagyu_fill_type(params_.fill_type),
                                mapbox::geometry::wagyu::fill_type_even_odd);
                mp.insert(mp.end(), tmp_mp.begin(), tmp_mp.end());
            }
        }

        if (mp.empty())
        {
            return;
        }
        next_(mp);
    }
};

} // end ns vector_tile_impl
} // end ns mapnik
