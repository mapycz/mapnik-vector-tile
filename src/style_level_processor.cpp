#include "style_level_processor.hpp"

// mapnik
#include <mapnik/agg_rasterizer.hpp>
#include <mapnik/agg_helpers.hpp>
#include <mapnik/rule.hpp>
#include <mapnik/debug.hpp>
#include <mapnik/layer.hpp>
#include <mapnik/label_collision_detector.hpp>
#include <mapnik/feature_type_style.hpp>
#include <mapnik/marker.hpp>
#include <mapnik/marker_cache.hpp>
#include <mapnik/unicode.hpp>
#include <mapnik/font_set.hpp>
#include <mapnik/parse_path.hpp>
#include <mapnik/map.hpp>
#include <mapnik/svg/svg_converter.hpp>
#include <mapnik/svg/svg_renderer_agg.hpp>
#include <mapnik/svg/svg_path_adapter.hpp>
#include <mapnik/pixel_position.hpp>
#include <mapnik/image_compositing.hpp>
#include <mapnik/image_filter.hpp>
#include <mapnik/image_any.hpp>
#include <mapnik/make_unique.hpp>

#pragma GCC diagnostic push
#include <mapnik/warning_ignore.hpp>
#include <boost/optional.hpp>
#pragma GCC diagnostic pop

// stl
#include <cmath>

namespace mapnik
{

    /*
template <typename T0, typename T1>
agg_renderer<T0,T1>::agg_renderer(Map const& m, T0 & pixmap, double scale_factor, unsigned offset_x, unsigned offset_y)
    : feature_style_processor<agg_renderer>(m, scale_factor),
      buffers_(),
      internal_buffers_(m.width(), m.height()),
      inflated_buffer_(),
      ras_ptr(new rasterizer),
      gamma_method_(GAMMA_POWER),
      gamma_(1.0),
      common_(m, attributes(), offset_x, offset_y, m.width(), m.height(), scale_factor)
{
    setup(m, pixmap);
}
*/

template <typename T0, typename T1>
void agg_renderer<T0,T1>::setup(Map const &m, buffer_type & pixmap)
{
    buffers_.emplace(pixmap);

    mapnik::set_premultiplied_alpha(pixmap, true);
    boost::optional<color> const& bg = m.background();
    if (bg)
    {
        if (bg->alpha() < 255)
        {
            mapnik::color bg_color = *bg;
            bg_color.premultiply();
            mapnik::fill(pixmap, bg_color);
        }
        else
        {
            mapnik::color bg_color = *bg;
            bg_color.set_premultiplied(true);
            mapnik::fill(pixmap, bg_color);
        }
    }

    MAPNIK_LOG_DEBUG(agg_renderer) << "agg_renderer: Scale=" << m.scale();
}

template <typename T0, typename T1>
void agg_renderer<T0,T1>::start_map_processing(Map const& map)
{
    MAPNIK_LOG_DEBUG(agg_renderer) << "agg_renderer: Start map processing bbox=" << map.get_current_extent();
    ras_ptr->clip_box(0,0,common_.width_,common_.height_);
}

template <typename T0, typename T1>
void agg_renderer<T0,T1>::end_map_processing(Map const& map)
{
    mapnik::demultiply_alpha(buffers_.top().get());
    MAPNIK_LOG_DEBUG(agg_renderer) << "agg_renderer: End map processing";
}

template <typename T0, typename T1>
void agg_renderer<T0,T1>::start_layer_processing(layer const& lay, box2d<double> const& query_extent)
{
    MAPNIK_LOG_DEBUG(agg_renderer) << "agg_renderer: Start processing layer=" << lay.name();
    MAPNIK_LOG_DEBUG(agg_renderer) << "agg_renderer: -- datasource=" << lay.datasource().get();
    MAPNIK_LOG_DEBUG(agg_renderer) << "agg_renderer: -- query_extent=" << query_extent;

    if (lay.clear_label_cache())
    {
        common_.detector_->clear();
    }

    common_.query_extent_ = query_extent;
    boost::optional<box2d<double> > const& maximum_extent = lay.maximum_extent();
    if (maximum_extent)
    {
        common_.query_extent_.clip(*maximum_extent);
    }

    if (lay.comp_op() || lay.get_opacity() < 1.0)
    {
        buffers_.emplace(internal_buffers_.push());
        set_premultiplied_alpha(buffers_.top().get(), true);
    }
    else
    {
        buffers_.emplace(buffers_.top().get());
    }
}

template <typename T0, typename T1>
void agg_renderer<T0,T1>::end_layer_processing(layer const& lyr)
{
    MAPNIK_LOG_DEBUG(agg_renderer) << "agg_renderer: End layer processing";

    buffer_type & current_buffer = buffers_.top().get();
    buffers_.pop();
    buffer_type & previous_buffer = buffers_.top().get();

    if (&current_buffer != &previous_buffer)
    {
        composite_mode_e comp_op = lyr.comp_op() ? *lyr.comp_op() : src_over;
        composite(previous_buffer, current_buffer,
                  comp_op, lyr.get_opacity(),
                  -common_.t_.offset(),
                  -common_.t_.offset());
        internal_buffers_.pop();
    }
}

template <typename T0, typename T1>
void agg_renderer<T0,T1>::start_style_processing(feature_type_style const& st)
{
    MAPNIK_LOG_DEBUG(agg_renderer) << "agg_renderer: Start processing style";

    if (st.comp_op() || st.image_filters().size() > 0 || st.get_opacity() < 1)
    {
        if (st.image_filters_inflate())
        {
            int radius = 0;
            mapnik::filter::filter_radius_visitor visitor(radius);
            for (mapnik::filter::filter_type const& filter_tag : st.image_filters())
            {
                util::apply_visitor(visitor, filter_tag);
            }
            radius *= common_.scale_factor_;
            if (radius > common_.t_.offset())
            {
                common_.t_.set_offset(radius);
            }
            int offset = common_.t_.offset();
            unsigned target_width = common_.width_ + (offset * 2);
            unsigned target_height = common_.height_ + (offset * 2);
            ras_ptr->clip_box(-int(offset*2),-int(offset*2),target_width,target_height);
            if (!inflated_buffer_ ||
                (inflated_buffer_->width() < target_width ||
                 inflated_buffer_->height() < target_height))
            {
                inflated_buffer_ = std::make_unique<buffer_type>(target_width, target_height);
            }
            else
            {
                mapnik::fill(*inflated_buffer_, 0); // fill with transparent colour
            }
            buffers_.emplace(*inflated_buffer_);
        }
        else
        {
            buffers_.emplace(internal_buffers_.push());
            common_.t_.set_offset(0);
            ras_ptr->clip_box(0,0,common_.width_,common_.height_);
        }
        set_premultiplied_alpha(buffers_.top().get(), true);
    }
    else
    {
        common_.t_.set_offset(0);
        ras_ptr->clip_box(0,0,common_.width_,common_.height_);
        buffers_.emplace(buffers_.top().get());
    }
}

template <typename T0, typename T1>
void agg_renderer<T0,T1>::end_style_processing(feature_type_style const& st)
{
    buffer_type & current_buffer = buffers_.top().get();
    buffers_.pop();
    buffer_type & previous_buffer = buffers_.top().get();
    if (&current_buffer != &previous_buffer)
    {
        bool blend_from = false;
        if (st.image_filters().size() > 0)
        {
            blend_from = true;
            mapnik::filter::filter_visitor<buffer_type> visitor(current_buffer, common_.scale_factor_);
            for (mapnik::filter::filter_type const& filter_tag : st.image_filters())
            {
                util::apply_visitor(visitor, filter_tag);
            }
            mapnik::premultiply_alpha(current_buffer);
        }
        if (st.comp_op())
        {
            composite(previous_buffer, current_buffer,
                      *st.comp_op(), st.get_opacity(),
                      -common_.t_.offset(),
                      -common_.t_.offset());
        }
        else if (blend_from || st.get_opacity() < 1.0)
        {
            composite(previous_buffer, current_buffer,
                      src_over, st.get_opacity(),
                      -common_.t_.offset(),
                      -common_.t_.offset());
        }
        if (&current_buffer == &internal_buffers_.top())
        {
            internal_buffers_.pop();
        }
    }
    if (st.direct_image_filters().size() > 0)
    {
        // apply any 'direct' image filters
        mapnik::filter::filter_visitor<buffer_type> visitor(previous_buffer, common_.scale_factor_);
        for (mapnik::filter::filter_type const& filter_tag : st.direct_image_filters())
        {
            util::apply_visitor(visitor, filter_tag);
        }
        mapnik::premultiply_alpha(previous_buffer);
    }
    MAPNIK_LOG_DEBUG(agg_renderer) << "agg_renderer: End processing style";
}

} // end ns
