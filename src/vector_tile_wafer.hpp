#ifndef __MAPNIK_VECTOR_TILE_MERC_WAFER_H__
#define __MAPNIK_VECTOR_TILE_MERC_WAFER_H__

// mapnik-vector-tile
#include "vector_tile_merc_tile.hpp"

// std
#include <vector>

namespace mapnik
{

namespace vector_tile_impl
{

class merc_wafer
{
protected:
    std::uint64_t x_, y_, z_;
    unsigned span_;

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
          span_(span)
    {
        for (std::uint64_t j = y; j < y + span; ++j)
        {
            for (std::uint64_t i = x; i < x + span; ++i)
            {
                tiles_.emplace_back(i, j, z, tile_size, buffer_size);
            }
        }
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

};

}

}

#if !defined(MAPNIK_VECTOR_TILE_LIBRARY)
//#include "vector_tile_tile.ipp"
#endif

#endif // __MAPNIK_VECTOR_TILE_MERC_WAFER_H__
