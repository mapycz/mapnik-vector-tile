#pragma once
// mapnik-vector-tile
#include "vector_tile_merc_tile.hpp"
#include "vector_tile_layer.hpp"

// std
#include <vector>

namespace mapnik
{

namespace vector_tile_impl
{

class merc_wafer
{
    std::uint64_t x_, y_, z_;
    unsigned span_;
    mapnik::box2d<double> extent_;
    std::vector<merc_tile> tiles_;

public:
    merc_wafer(std::uint64_t x,
              std::uint64_t y,
              std::uint64_t z,
              unsigned span,
              std::uint32_t tile_size,
              std::int32_t buffer_size)
        : x_(x),
          y_(y),
          z_(z),
          span_(span),
          extent_(merc_extent(x, y, z))
    {
        for (std::uint64_t j = y; j < y + span; ++j)
        {
            for (std::uint64_t i = x; i < x + span; ++i)
            {
                tiles_.emplace_back(i, j, z, tile_size, buffer_size);
            }
        }

        extent_.expand_to_include(merc_extent(x + span - 1, y + span - 1, z));
    }

    merc_wafer(merc_wafer const& rhs) = default;

    merc_wafer(merc_wafer && rhs) = default;

    unsigned span() const
    {
        return span_;
    }

    std::vector<merc_tile> & tiles()
    {
        return tiles_;
    }

    std::vector<merc_tile> const & tiles() const
    {
        return tiles_;
    }

    merc_tile const & tile(std::size_t x, std::size_t y) const
    {
        return tiles_[y * span_ + x];
    }

    box2d<double> const & extent() const
    {
        return extent_;
    }

    std::uint32_t tile_size() const
    {
        return tiles_.front().tile_size() * span_;
    }

    std::int32_t buffer_size() const
    {
        return tiles_.front().buffer_size();
    }

    bool has_layer(std::string const& name) const
    {
        for (auto const & tile : tiles_)
        {
            if (tile.has_layer(name))
            {
                return true;
            }
        }
        return false;
    }

    void add_empty_layer(std::string const& name)
    {
        for (auto & tile : tiles_)
        {
            tile.add_empty_layer(name);
        }
    }

    bool add_layer(wafer_layer const& layer)
    {
        bool added = false;
        auto tile = tiles_.begin();
        for (auto const & buffer : layer.buffers())
        {
            tile->add_layer(layer.name(), buffer);
            ++tile;
        }
        return added;
    }
};

}

}
