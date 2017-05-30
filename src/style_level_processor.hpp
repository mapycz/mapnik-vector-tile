#ifndef __MAPNIK_VECTOR_TILE_STYLE_LEVEL_PROCESSOR__
#define __MAPNIK_VECTOR_TILE_STYLE_LEVEL_PROCESSOR__

// mapnik
#include <mapnik/config.hpp>            // for MAPNIK_DECL
#include <mapnik/feature_style_processor.hpp>
#include <mapnik/util/noncopyable.hpp>       // for noncopyable
#include <mapnik/rule.hpp>              // for rule, symbolizers
#include <mapnik/geometry/box2d.hpp>     // for box2d
#include <mapnik/view_transform.hpp>    // for view_transform
#include <mapnik/image_compositing.hpp>  // for composite_mode_e
#include <mapnik/pixel_position.hpp>
#include <mapnik/request.hpp>
#include <mapnik/symbolizer_enumerations.hpp>
#include <mapnik/renderer_common.hpp>
#include <mapnik/image_util.hpp>
// stl
#include <memory>
#include <stack>

// fwd declaration to avoid dependence on agg headers
namespace agg { struct trans_affine; }

// fwd declarations to speed up compile
namespace mapnik {
  class Map;
  class feature_impl;
  class feature_type_style;
  class label_collision_detector4;
  class layer;
  class color;
  struct marker;
  class proj_transform;
  struct rasterizer;
  struct rgba8_t;
  template<typename T> class image;
}

namespace mapnik
{

namespace vector_tile_impl
{

class MAPNIK_DECL style_level_processor :
    public feature_style_processor<style_level_processor>,
    private util::noncopyable
{

public:
    using processor_impl_type = agg_renderer<T0>;
    using detector_type = T1;
    // create with default, empty placement detector
    agg_renderer(Map const& m, buffer_type & pixmap, double scale_factor=1.0, unsigned offset_x=0, unsigned offset_y=0);
    // create with external placement detector, possibly non-empty
    style_level_processor(Map const &m, buffer_type & pixmap, std::shared_ptr<detector_type> detector,
                 double scale_factor=1.0, unsigned offset_x=0, unsigned offset_y=0);
    // pass in mapnik::request object to provide the mutable things per render
    style_level_processor(Map const& m, request const& req, attributes const& vars, buffer_type & pixmap, double scale_factor=1.0, unsigned offset_x=0, unsigned offset_y=0);
    ~style_level_processor();
    void start_map_processing(Map const& map);
    void end_map_processing(Map const& map);
    void start_layer_processing(layer const& lay, box2d<double> const& query_extent);
    void end_layer_processing(layer const& lay);

    void start_style_processing(feature_type_style const& st);
    void end_style_processing(feature_type_style const& st);

    void render_marker(pixel_position const& pos, marker const& marker, agg::trans_affine const& tr,
                       double opacity, composite_mode_e comp_op);

    void process(point_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(line_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(line_pattern_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(polygon_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(polygon_pattern_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(raster_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(shield_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(text_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(building_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(markers_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(group_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(debug_symbolizer const& sym,
                 feature_impl & feature,
                 proj_transform const& prj_trans);
    void process(dot_symbolizer const& sym,
                 mapnik::feature_impl & feature,
                 proj_transform const& prj_trans);

    inline bool process(rule::symbolizers const&,
                        mapnik::feature_impl&,
                        proj_transform const& )
    {
        // agg renderer doesn't support processing of multiple symbolizers.
        return false;
    }

    void painted(bool painted);
    bool painted();

    inline eAttributeCollectionPolicy attribute_collection_policy() const
    {
        return DEFAULT;
    }

    inline double scale_factor() const
    {
        return common_.scale_factor_;
    }

    inline attributes const& variables() const
    {
        return common_.vars_;
    }
protected:
    template <typename R>
    void debug_draw_box(R& buf, box2d<double> const& extent,
                        double x, double y, double angle = 0.0);
    void debug_draw_box(box2d<double> const& extent,
                        double x, double y, double angle = 0.0);
    void draw_geo_extent(box2d<double> const& extent,mapnik::color const& color);

private:
    std::stack<std::reference_wrapper<buffer_type>> buffers_;
    buffer_stack<buffer_type> internal_buffers_;
    std::unique_ptr<buffer_type> inflated_buffer_;
    const std::unique_ptr<rasterizer> ras_ptr;
    gamma_method_enum gamma_method_;
    double gamma_;
    renderer_common common_;
    void setup(Map const & m, buffer_type & pixmap);
};

} // namespace mapnik

#endif // __MAPNIK_VECTOR_TILE_STYLE_LEVEL_PROCESSOR__
