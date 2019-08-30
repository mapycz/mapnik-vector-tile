#ifndef __MAPNIK_VECTOR_TILE_LAYER_H__
#define __MAPNIK_VECTOR_TILE_LAYER_H__

// mapnik-vector-tile
#include "vector_tile_config.hpp"

// mapnik
#include <mapnik/box2d.hpp>
#include <mapnik/datasource.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/feature_type_style.hpp>
#include <mapnik/rule.hpp>
#include <mapnik/layer.hpp>
#include <mapnik/map.hpp>
#include <mapnik/projection.hpp>
#include <mapnik/proj_transform.hpp>
#include <mapnik/scale_denominator.hpp>
#include <mapnik/value.hpp>
#include <mapnik/view_transform.hpp>
#include <mapnik/rule_cache.hpp>
#include <mapnik/expression_evaluator.hpp>

// protozero
#include <protozero/pbf_writer.hpp>

// std
#include <map>
#include <unordered_map>
#include <utility>

namespace mapnik
{

namespace vector_tile_impl
{

struct layer_builder_pbf
{
    typedef std::map<std::string, unsigned> keys_container;
    typedef std::unordered_map<mapnik::value, unsigned> values_container;

    keys_container keys;
    values_container values;
    std::string & layer_buffer;
    std::size_t initial_size;

    layer_builder_pbf(std::string const & name, std::uint32_t extent, std::string & _layer_buffer)
        : keys(),
          values(),
          layer_buffer(_layer_buffer)
    {
        protozero::pbf_writer layer_writer(layer_buffer);
        layer_writer.add_uint32(Layer_Encoding::VERSION, 2);
        layer_writer.add_string(Layer_Encoding::NAME, name);
        layer_writer.add_uint32(Layer_Encoding::EXTENT, extent);
        initial_size = layer_buffer.size();
    }

    bool empty() const
    {
        return layer_buffer.size() <= initial_size;
    }

    void finalize()
    {
        if (empty())
        {
            layer_buffer.clear();
        }
    }

    MAPNIK_VECTOR_INLINE protozero::pbf_writer add_feature(mapnik::feature_impl const& mapnik_feature,
                                                           std::vector<std::uint32_t> & feature_tags);
};

class vector_layer
{
protected:
    bool valid_;
    unsigned span_;
    mapnik::Map const& map_;
    mapnik::layer const& layer_;
    double scale_denom_;
    std::int32_t buffer_size_;
    mapnik::datasource_ptr ds_;
    mapnik::projection target_proj_;
    mapnik::projection source_proj_;
    mapnik::proj_transform prj_trans_;
    std::string name_;
    std::uint32_t layer_extent_;
    mapnik::box2d<double> target_buffered_extent_;
    mapnik::box2d<double> source_buffered_extent_;
    mapnik::query query_;
    mapnik::view_transform view_trans_;
    const double simplify_distance_;

public:
    vector_layer(mapnik::Map const& map,
               mapnik::layer const& lay,
               mapnik::box2d<double> const& tile_extent_bbox,
               std::uint32_t tile_size,
               std::int32_t buffer_size,
               double scale_factor,
               double scale_denom,
               int offset_x,
               int offset_y,
               bool style_level_filter,
               double simplify_distance,
               mapnik::attributes const& vars,
               unsigned span)
        : valid_(true),
          span_(span),
          map_(map),
          layer_(lay),
          scale_denom_(scale_denom),
          buffer_size_(calc_buffer_size(buffer_size, tile_size, span, lay)),
          ds_(lay.datasource()),
          target_proj_(map.srs(), true),
          source_proj_(lay.srs(), true),
          prj_trans_(target_proj_, source_proj_),
          name_(lay.name()),
          layer_extent_(calc_extent(tile_size)),
          target_buffered_extent_(calc_target_buffered_extent(tile_extent_bbox, lay, map)),
          source_buffered_extent_(calc_source_buffered_extent()),
          query_(calc_query(scale_factor, scale_denom, tile_extent_bbox, map, lay, style_level_filter, vars)),
          view_trans_(layer_extent_, layer_extent_, tile_extent_bbox, offset_x, offset_y),
          simplify_distance_(calc_simplify_distance(simplify_distance))
    {
    }


    vector_layer(vector_layer && rhs)
        : valid_(std::move(rhs.valid_)),
          span_(std::move(rhs.span_)),
          map_(std::move(rhs.map_)),
          layer_(std::move(rhs.layer_)),
          scale_denom_(std::move(rhs.scale_denom_)),
          buffer_size_(std::move(rhs.buffer_size_)),
          ds_(std::move(rhs.ds_)),
          target_proj_(std::move(rhs.target_proj_)),
          source_proj_(std::move(rhs.source_proj_)),
          prj_trans_(target_proj_, source_proj_),
          name_(std::move(rhs.name_)),
          layer_extent_(std::move(rhs.layer_extent_)),
          target_buffered_extent_(std::move(rhs.target_buffered_extent_)),
          source_buffered_extent_(std::move(rhs.source_buffered_extent_)),
          query_(std::move(rhs.query_)),
          view_trans_(std::move(rhs.view_trans_)),
          simplify_distance_(std::move(rhs.simplify_distance_))
    {
    }

    vector_layer& operator=(vector_layer&&) = default;

    vector_layer(vector_layer const& rhs) = delete;
    vector_layer& operator=(const vector_layer&) = delete;

    double calc_simplify_distance(double simplify_distance) const
    {
        if (ds_)
        {
            auto val = ds_->params().template get<
                mapnik::value_double>("mvt_simplify_distance");
            if (val)
            {
                return *val;
            }
        }
        return simplify_distance;
    }

    std::uint32_t calc_extent(std::uint32_t layer_extent)
    {
        if (!ds_)
        {
            valid_ = false;
            return 4096;
        }
        auto ds_extent = ds_->params().template get<mapnik::value_integer>("vector_layer_extent");
        if (ds_extent)
        {
            if (ds_->type() == datasource::Vector)
            {
                layer_extent = *ds_extent;
            }
            else
            {
                layer_extent = 256;
            }
        }
        if (layer_extent == 0)
        {
            valid_ = false;
            layer_extent = 4096;
        }
        return layer_extent;
    }

    std::int32_t calc_buffer_size(std::int32_t buffer_size,
                                  std::uint32_t tile_size,
                                  unsigned span,
                                  mapnik::layer const& lay) const
    {
        boost::optional<int> layer_buffer_size = lay.buffer_size();
        if (layer_buffer_size)
        {
            return (*layer_buffer_size) *
                (static_cast<double>(tile_size) /
                 (VT_LEGACY_IMAGE_SIZE * span));
        }
        return buffer_size;
    }

    mapnik::box2d<double> calc_target_buffered_extent(mapnik::box2d<double> const& tile_extent_bbox,
                                               mapnik::layer const& lay,
                                               mapnik::Map const& map) const
    {
        mapnik::box2d<double> ext(tile_extent_bbox);
        double scale = ext.width() / layer_extent_;
        double buffer_padding = 2.0 * scale * buffer_size_;
        double buffered_width = ext.width() + buffer_padding;
        double buffered_height = ext.height() + buffer_padding;
        if (buffered_width < 0.0)
        {
            buffered_width = 0.0;
        }
        if (buffered_height < 0.0)
        {
            buffered_height = 0.0;
        }
        ext.width(buffered_width);
        ext.height(buffered_height);
        
        boost::optional<box2d<double> > const& maximum_extent = map.maximum_extent();
        if (maximum_extent)
        {
            ext.clip(*maximum_extent);
        }
        return ext;
    }
    
    mapnik::box2d<double> calc_source_buffered_extent()
    {
        mapnik::box2d<double> new_extent(target_buffered_extent_);
        if (!prj_trans_.forward(new_extent, PROJ_ENVELOPE_POINTS))
        {
            // this modifies the layer_ext by clipping to the buffered_ext
            valid_ = false;
        }
        return new_extent;
    }

    mapnik::query calc_query(double scale_factor,
                             double scale_denom,
                             mapnik::box2d<double> const& tile_extent_bbox,
                             mapnik::Map const& map,
                             mapnik::layer const& lay,
                             bool style_level_filter,
                             mapnik::attributes const& vars)
    {
        // Adjust the scale denominator if required
        if (scale_denom <= 0.0)
        {
            double scale = tile_extent_bbox.width() / (VT_LEGACY_IMAGE_SIZE * span_);
            scale_denom = mapnik::scale_denominator(scale, target_proj_.is_geographic());
        }
        scale_denom *= scale_factor;
        if (!is_active(map, lay, scale_denom, style_level_filter))
        {
            valid_ = false;
        }
        scale_denom_ = scale_denom;

        mapnik::box2d<double> query_extent(lay.envelope()); // source projection

        // first, try intersection of map extent forward projected into layer srs
        if (source_buffered_extent_.intersects(query_extent))
        {
            // this modifies the query_extent by clipping to the buffered_ext
            query_extent.clip(source_buffered_extent_);
        }
        // if no intersection and projections are also equal, early return
        else if (prj_trans_.equal())
        {
            valid_ = false;
        }
        // next try intersection of layer extent back projected into map srs
        else if (prj_trans_.backward(query_extent, PROJ_ENVELOPE_POINTS) && target_buffered_extent_.intersects(query_extent))
        {
            query_extent.clip(target_buffered_extent_);
            // forward project layer extent back into native projection
            if (!prj_trans_.forward(query_extent, PROJ_ENVELOPE_POINTS))
            {
                throw std::runtime_error("vector_tile_processor: query extent did not reproject back to map projection");
            }
        }
        else
        {
            // if no intersection then nothing to do for layer
            valid_ = false;    
        }

        mapnik::box2d<double> unbuffered_query_extent(tile_extent_bbox);
        if (!prj_trans_.equal())
        {
            if (!prj_trans_.forward(unbuffered_query_extent, PROJ_ENVELOPE_POINTS))
            {
                unbuffered_query_extent = lay.envelope();
                if (prj_trans_.backward(unbuffered_query_extent, PROJ_ENVELOPE_POINTS))
                {
                    unbuffered_query_extent.clip(tile_extent_bbox);
                    prj_trans_.forward(unbuffered_query_extent, PROJ_ENVELOPE_POINTS);
                }
            }
        }

        double qw = unbuffered_query_extent.width() > 0 ? unbuffered_query_extent.width() : 1;
        double qh = unbuffered_query_extent.height() > 0 ? unbuffered_query_extent.height() : 1;
        if (!ds_ || ds_->type() == datasource::Vector)
        {
            qw = (VT_LEGACY_IMAGE_SIZE * span_) / qw;
            qh = (VT_LEGACY_IMAGE_SIZE * span_) / qh;
        }
        else
        {
            qw = static_cast<double>(layer_extent_) / qw;
            qh = static_cast<double>(layer_extent_) / qh;
        }
        mapnik::query::resolution_type res(qw, qh);
        mapnik::query q(query_extent, res, scale_denom, unbuffered_query_extent);
        if (ds_)
        {
            mapnik::layer_descriptor lay_desc = ds_->get_descriptor();
            for (mapnik::attribute_descriptor const& desc : lay_desc.get_descriptors())
            {
                q.add_property_name(desc.get_name());
            }
        }
        q.set_variables(vars);
        return q;
    }

    bool is_active(mapnik::Map const & map,
                   mapnik::layer const & lay,
                   double scale_denom,
                   bool style_level_filter)
    {
        if (!lay.visible(scale_denom))
        {
            return false;
        }

        if (!style_level_filter)
        {
            return true;
        }

        for (auto const & style_name : lay.styles())
        {
            boost::optional<mapnik::feature_type_style const &> style = map.find_style(style_name);

            if (!style)
            {
                continue;
            }

            if (style->active(scale_denom))
            {
                return true;
            }
        }

        return false;
    }

    std::vector<mapnik::rule_cache> get_active_rules() const
    {
        std::vector<mapnik::rule_cache> active_rules;

        for (auto const & style_name : layer_.styles())
        {
            boost::optional<mapnik::feature_type_style const &> style = map_.find_style(style_name);

            if (!style)
            {
                continue;
            }

            active_rules.emplace_back();
            bool has_active_rules = false;

            for (auto const & rule : style->get_rules())
            {
                if (rule.active(scale_denom_))
                {
                    has_active_rules = true;
                    active_rules.back().add_rule(rule);
                }
            }

            if (!has_active_rules)
            {
                active_rules.pop_back();
            }
        }

        return active_rules;
    }

    bool evaluate_feature(feature_impl const & feature,
                          std::vector<mapnik::rule_cache> const & active_rules) const
    {
        // TODO: accept vars
        mapnik::attributes vars;

        for (auto const & rc : active_rules)
        {
            for (mapnik::rule const* r : rc.get_if_rules())
            {
                expression_ptr const& expr = r->get_filter();
                mapnik::value result = mapnik::util::apply_visitor(
                    mapnik::evaluate<feature_impl, value_type, attributes>(feature, vars), *expr);
                if (result.to_bool())
                {
                    return true;
                }
            }

            if (rc.get_else_rules().size() > 0)
            {
                return true;
            }
        }
        return false;
    }

    mapnik::datasource_ptr get_ds() const
    {
        return ds_;
    }

    mapnik::featureset_ptr get_features() const
    {
        return ds_->features(query_);
    }

    mapnik::query const& get_query() const
    {
        return query_;
    }

    mapnik::view_transform const& get_view_transform() const
    {
        return view_trans_;
    }

    mapnik::proj_transform const& get_proj_transform() const
    {
        return prj_trans_;
    }

    mapnik::box2d<double> const& get_source_buffered_extent() const
    {
        return source_buffered_extent_;
    }

    mapnik::box2d<double> const& get_target_buffered_extent() const
    {
        return target_buffered_extent_;
    }

    bool is_valid() const
    {
        return valid_;
    }

    std::uint32_t layer_extent() const
    {
        return layer_extent_;
    }

    std::string const& name() const
    {
        return name_;
    }
    
    void name(std::string const& val)
    {
        name_ = val;
    }

    double simplify_distance() const
    {
        return simplify_distance_;
    }
};

class tile_layer : public vector_layer
{
    std::string buffer_;

public:
    template <typename Tile>
    tile_layer(mapnik::Map const& map,
               mapnik::layer const& lay,
               Tile & tile,
               double scale_factor,
               double scale_denom,
               int offset_x,
               int offset_y,
               bool style_level_filter,
               double simplify_distance,
               mapnik::attributes const& vars) :
        vector_layer(map, lay, tile.extent(), tile.tile_size(),
                     tile.buffer_size(), scale_factor, scale_denom,
                     offset_x, offset_y, style_level_filter,
                     simplify_distance, vars, 1)
    {
    }

    tile_layer(tile_layer && rhs)
        : vector_layer(std::move(rhs)),
          buffer_(std::move(rhs.buffer_))
    {
    }

    tile_layer& operator=(tile_layer &&) = default;

    std::string const& get_data() const
    {
        return buffer_;
    }

    std::string & get_data()
    {
        return buffer_;
    }

    bool is_empty() const
    {
        return buffer_.empty();
    }
};

class wafer_layer : public vector_layer
{
    std::deque<std::string> buffers_;

public:
    template <typename Wafer>
    wafer_layer(mapnik::Map const& map,
               mapnik::layer const& lay,
               Wafer & wafer,
               double scale_factor,
               double scale_denom,
               int offset_x,
               int offset_y,
               bool style_level_filter,
               double simplify_distance,
               mapnik::attributes const& vars) :
        vector_layer(map, lay, wafer.extent(), wafer.tile_size(),
                     wafer.buffer_size(), scale_factor, scale_denom,
                     offset_x, offset_y, style_level_filter,
                     simplify_distance, vars, wafer.span()),
        buffers_(wafer.tiles().size())
    {
    }

    wafer_layer(wafer_layer && rhs)
        : vector_layer(std::move(rhs)),
          buffers_(std::move(rhs.buffers_))
    {
    }

    wafer_layer& operator=(wafer_layer &&) = default;

    std::deque<std::string> const& buffers() const
    {
        return buffers_;
    }

    std::deque<std::string> & buffers()
    {
        return buffers_;
    }

    bool is_empty() const
    {
        for (auto const & buffer : buffers_)
        {
            if (!buffer.empty())
            {
                return false;
            }
        }
        return true;
    }

    // TODO: Fake support for rasters
    std::string raster_buffer_;
    std::string & get_data()
    {
        return raster_buffer_;
    }

    std::int32_t buffer_size() const
    {
        return buffer_size_;
    }
};

} // end ns vector_tile_impl

} // end ns mapnik

#if !defined(MAPNIK_VECTOR_TILE_LIBRARY)
#include "vector_tile_layer.ipp"
#endif

#endif // __MAPNIK_VECTOR_TILE_LAYER_H__
