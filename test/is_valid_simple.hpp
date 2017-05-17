
namespace mapnik { namespace geometry {

template <typename T>
bool is_valid(T p, std::string & message)
{
    message = "Geometry is valid";
    return true;
}

template <typename T>
bool is_simple(T p)
{
    return true;
}

} }
