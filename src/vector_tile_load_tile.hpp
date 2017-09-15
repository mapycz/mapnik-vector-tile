#ifndef __MAPNIK_VECTOR_TILE_LOAD_TILE_H__
#define __MAPNIK_VECTOR_TILE_LOAD_TILE_H__

// mapnik-vector-tile
#include "vector_tile_config.hpp"
#include "vector_tile_merc_tile.hpp"

//protozero
#include <protozero/pbf_reader.hpp>

namespace mapnik
{

namespace vector_tile_impl
{

std::pair<std::string,std::uint32_t> get_layer_name_and_version(protozero::pbf_reader & layer_msg);

void merge_from_buffer(merc_tile & t,
                       const char * data,
                       std::size_t size,
                       bool validate = false,
                       bool upgrade = false)

void merge_from_compressed_buffer(merc_tile & t,
                                  const char * data,
                                  std::size_t size,
                                  bool validate = false,
                                  bool upgrade = false)

void add_image_buffer_as_tile_layer(merc_tile & t,
                                    std::string const& layer_name,
                                    const char * data,
                                    std::size_t size)

} // end ns vector_tile_impl

} // end ns mapnik

#if !defined(MAPNIK_VECTOR_TILE_LIBRARY)
#include "vector_tile_load_tile.ipp"
#endif

#endif // __MAPNIK_VECTOR_TILE_LOAD_TILE_H__
