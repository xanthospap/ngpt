#include <cassert>
#include <cstdlib>
#include <cmath>
#include <stdexcept>
#include <cstring>
#include "ionex.hpp"
#include "grid.hpp"

#ifdef DEBUG
    #include <iostream>
#endif

using ngpt::ionex;
using ngpt::ionex_grd_type;

typedef std::tuple<std::size_t, ionex_grd_type,
                   std::size_t, ionex_grd_type,
                   std::size_t, ionex_grd_type,
                   std::size_t, ionex_grd_type>  tec_cell;

/// No header line can have more than 80 chars. However, there are cases when
/// they  exceed this limit, just a bit ...
constexpr int MAX_HEADER_CHARS { 82 };

/// According to IONEX specification, each longtitude line can contain a
/// maximum of 16 TEC values (\cite inx1).
constexpr std::size_t MAX_TEC_PER_LINE { 16 };

/**
 *  Size of 'END OF HEADER' C-string.
 *  std::strlen is not 'constexr' so eoh_size can't be one either. Note however
 *  that gcc has a builtin constexpr strlen function (if we want to disable this
 *  we can do so with -fno-builtin).
 */
#ifdef __clang__
    const     std::size_t eoh_size { std::strlen("END OF HEADER") };
#else
    constexpr std::size_t eoh_size { std::strlen("END OF HEADER") };
#endif

/// Max header lines.
constexpr int MAX_HEADER_LINES { 1000 };

/// ionex constructor
ngpt::ionex::ionex(const char* filename)
    : _filename(filename),
      _istream (filename, std::ios::in),
      _version (ionex_version::v10),
      _end_of_head(0),
      _time_scale(ngpt::time_scale::ut1),
      _first_epoch( ngpt::min_date<>() ),
      _last_epoch( ngpt::max_date<>() ),
      _interval(0),
      _maps_in_file(0),
      _min_elevation(0),
      _base_radius(0),
      _map_dimension(2),
      _hgt1(0), _hgt2(0), _dhgt(0),
      _lat1(0), _lat2(0), _dlat(0),
      _lon1(0), _lon2(0), _dlon(0),
      _exp(-1)
{
    if ( !_istream.is_open() ) {
        throw std::runtime_error 
            ("Cannot open ionex file: " + std::string(filename) );
    }
#ifdef DEUG
    try {
        this->read_header();
    } catch (std::runtime_error& e) {
        std::cerr<<"\nFailed to construct IONEX; header error.";
    }
#endif
    if ( this->read_header() ) {
          if ( _istream.is_open() ) { _istream.close(); }
    }
}

/// [Help function] Read an ionex datetime formated as:
/// YYYY MM DD HH MM SS (all integers as 6I6)
/// A return status other than 0 denotes an error.
int
_read_ionex_datetime_(char* c, ionex::datetime_ms* d)
{
    errno = 0;
    char* start = c;
    char* end;
    int fields[] = { 9999, 99, 99, 99, 99, 99 };

    for (int i=0; i<6; ++i) {
        fields[i] = std::strtol(start, &end, 10);
        start = end;
    }
    
    if (errno == ERANGE) {
        errno = 0;
        return 1;
    }

    try {
        ionex::datetime_ms newd (fields[0], fields[1], fields[2],
                fields[3], fields[4], static_cast<double>(fields[5]));

        *d = newd;
        return 0;
    } catch (std::out_of_range& e) {
        return 1;
    }
}

/* Read an ionex header.
 *
 * \warning In DEBUG mode this will throw in case of an error.
 *
 * TODO better validate fields after reading header (are all fields read ok??)
 * should fucking know that before going on!
 */
int
ionex::read_header()
{
    char line    [MAX_HEADER_CHARS];
    char sysmodel[4];
    char mapfun  [5];
    char* start, *end;

    // we are going to use strtol() so reset errno
    // see http://en.cppreference.com/w/cpp/string/byte/strtol
    errno = 0;

    // The stream should be open by now!
    assert( this->_istream.is_open() );

    // Go to the top of the file.
    _istream.seekg(0);

    // Read the first line. Get version, file type and system/model.
    // ----------------------------------------------------
    _istream.getline(line, MAX_HEADER_CHARS);
    // strtod will keep on reading untill a non-valid
    // char is read. Fuck this, lets split the string
    // so that it only reads the valid chars (for version).
    *(line+8) = '\0';
    float fvers = std::strtod(line, &end);
    if (std::abs(fvers - 1.0) < .001) {
        this->_version = ionex::ionex_version::v10;
    } else {
#ifdef DEBUG
        throw std::runtime_error
        ("[DEBUG] ionex::read_header -> Invalid IONEX version.");
#endif
        return 1;
    }
    if ( line[20] != 'I' ) {
#ifdef DEBUG
        throw std::runtime_error
        ("[DEBUG] ionex::read_header -> Invalid IONEX file-type");
#endif
        return 1;
    }
    std::memcpy(sysmodel, line+39, 3);
    sysmodel[3] = '\0';

    // Keep on readling lines until 'END OF HEADER'.
    // ----------------------------------------------------
    int dummy_it = 0;
    _istream.getline(line, MAX_HEADER_CHARS);
    while (dummy_it < MAX_HEADER_LINES && 
            strncmp(line+60, "END OF HEADER", eoh_size) )
    {
        if ( !strncmp(line+60, "EPOCH OF FIRST MAP", 18) )
        {
            if ( _read_ionex_datetime_(line, &_first_epoch) )
            {
#ifdef DEBUG
                std::cerr <<"\n[DEBUG] Failed to resolve \"EPOCH OF FIRST MAP\"";
                throw std::runtime_error
                ("[DEBUG] ionex::read_header -> Invalid IONEX line");
#endif
                return 1;
            }
        }
        else if ( !strncmp(line+60, "EPOCH OF LAST MAP", 17) )
        {
            if ( _read_ionex_datetime_(line, &_last_epoch) )
            {
#ifdef DEBUG
                std::cerr <<"\n[DEBUG] Failed to resolve \"EPOCH OF LAST MAP\"";
                throw std::runtime_error
                ("[DEBUG] ionex::read_header -> Invalid IONEX line");
#endif
                return 1;
            }
        }
        else if ( !strncmp(line+60, "INTERVAL", 8) )
        {
            *(line+6) = '\0';
            _interval = std::strtol(line, &end, 10);
        }
        else if ( !strncmp(line+60, "# OF MAPS IN FILE", 17) )
        {
            *(line+6) = '\0';
            _maps_in_file = std::strtol(line, &end, 10);
        }
        else if ( !strncmp(line+60, "MAPPING FUNCTION", 16) )
        {
            std::memcpy(mapfun, line+2, 4);
        }
        else if ( !strncmp(line+60, "ELEVATION CUTOFF", 16) )
        {
            *(line+10) = '\0';
            _min_elevation = std::strtod(line, &end);
        }
        else if ( !strncmp(line+60, "OBSERVABLES USED", 16) )
        {
            /// TODO do i fucking want this ??
        }
        else if ( !strncmp(line+60, "# OF STATIONS", 13) )
        {
            /// TODO do i fucking want this ??
        }
        else if ( !strncmp(line+60, "# OF SATELLITES", 15) )
        {
            /// TODO do i fucking want this ??
        }
        else if ( !strncmp(line+60, "BASE RADIUS", 11) )
        {
            *(line+10) = '\0';
            _base_radius = std::strtod(line, &end);
        }
        else if ( !strncmp(line+60, "MAP DIMENSION", 13) )
        {
            *(line+6) = '\0';
            _map_dimension = std::strtol(line, &end, 10);
            if ( _map_dimension != 2 ) {
#ifdef DEBUG
                std::cerr <<"\n[DEBUG] Oh shit! This map-dimension is not supported";
                std::cerr <<"\n        Need to add code bitch!";
                throw std::runtime_error
                    ("ionex::read_header() -> Invalid map dimension");
#endif
                    return 1;
            }
        }
        else if ( !strncmp(line+60, "HGT1 / HGT2 / DHGT", 18) )
        {
            start = line+2;
            _hgt1 = std::strtod(start, &end);
            start = end;
            _hgt2 = std::strtod(start, &end);
            start = end;
            _dhgt = std::strtod(start, &end);
        }
        else if ( !strncmp(line+60, "LAT1 / LAT2 / DLAT", 18) )
        {
            start = line+2;
            _lat1 = std::strtod(start, &end);
            start = end;
            _lat2 = std::strtod(start, &end);
            start = end;
            _dlat = std::strtod(start, &end);
        }
        else if ( !strncmp(line+60, "LON1 / LON2 / DLON", 18) )
        {
            start = line+2;
            _lon1 = std::strtod(start, &end);
            start = end;
            _lon2 = std::strtod(start, &end);
            start = end;
            _dlon = std::strtod(start, &end);
        }
        else if ( !strncmp(line+60, "EXPONENT", 8) )
        {
            *(line+10) = '\0';
            _exp = std::strtol(line, &end, 10);
        }
        else if ( !strncmp(line+60, "START OF AUX DATA", 17) )
        {
#ifdef DEBUG
            std::cerr<<"\n[DEBUG] Fuck! This IONEX has AUX data.";
            std::cerr<<"\n        Don't know what to do with this shit!";
#endif
            while (  dummy_it < MAX_HEADER_LINES
                  && _istream.getline(line, MAX_HEADER_CHARS)
                  && strncmp(line+60, "END OF AUX DATA", 15)) {
#ifdef DEBUG
                std::cerr << "\n[DEBUG] Skipping: [" << line << "]";
#endif
            }
        }
        // check for str to numeric translation error
        if ( errno == ERANGE )
        {
#ifdef DEBUG
            std::cerr << "\n[DEBUG] Failed to translate line: ";
            std::cerr << "\n        line:[" << line;
            errno = 0;
            throw std::runtime_error
            ("[DEBUG] ionex::read_header -> Invalid IONEX line");
#endif
            errno = 0;
            return 1;
        }
        _istream.getline(line, MAX_HEADER_CHARS);
        dummy_it++;
    }
    if (dummy_it >= MAX_HEADER_LINES) {
#ifdef DEBUG
        throw std::runtime_error
        ("[DEBUG] ionex::read_header -> Could not find 'END OF HEADER'.");
#endif
        return 1;
    }

    this->_end_of_head = _istream.tellg();
    
    return 0;
}

/**  Compute # of maps for constant height.
 * 
 *   \warning This function uses the fact that the (latitude) grid is given
 *   with a precision of 1e-1 degrees.
 */ 
std::size_t
ionex::latitude_maps()
const noexcept
{
    long lat1 = static_cast<long>(_lat1 * 100);
    long lat2 = static_cast<long>(_lat2 * 100);
    long dlat = static_cast<long>(_dlat * 100);
    long maps = (lat2 - lat1) / dlat + 1;
    assert( maps > 0 );
    return static_cast<std::size_t>( maps );
}

/**  Compute how many lines should be read off from a const-latitude map instant
 * 
 *   \warning This function uses the fact that the (longtitude) grid is given
 *   with a precision of 1e-1 degrees.
 */ 
std::size_t
ionex::longtitude_lines()
const noexcept
{
    long lon1 = static_cast<long>(_lon1 * 100);
    long lon2 = static_cast<long>(_lon2 * 100);
    long dlon = static_cast<long>(_dlon * 100);
    long vals = (lon2 - lon1) / dlon + 1;
    assert( vals > 0 );
    long rows = vals / MAX_TEC_PER_LINE + (vals % MAX_TEC_PER_LINE > 0);
    return static_cast<std::size_t>( rows );
}

/**  Skip a whole map. The buffer should be placed in a position such that the
 *   first line to be read is: "LAT/LON1/LON2/H". After the function has
 *   returned, the buffer should be placed after the line: "END OF TEC MAP".
 *   The tec map is read, and absolutely nothing is done with it.
 *   By whole map, i mean all const-latitude maps for a given epoch.
 *
 *   \returns An integer denoting the exit status; anything other than 0, 
 *            denotes failure.
 */
int
ionex::skip_tec_map()
{
    static char line [MAX_HEADER_CHARS];
    char* c;
    ionex_grd_type lat = _lat1;
    ionex_grd_type ltmp;
    // number of const-latitude lines.
    std::size_t num_of_tec_lines = this->longtitude_lines();
    bool ascending ( _lat2 > _lat1 ? true : false );
    
    while ( _istream.getline(line, MAX_HEADER_CHARS) ) {
        // next line should be 'LAT/LON1/LON2/DLON/H'
        if ( std::strncmp(line+60, "LAT/LON1/LON2/DLON/H", 20) ) {
#ifdef DEBUG
            std::cerr<<"\n[DEBUG] Error reading TEC map 'LAT/LON1/LON2/DLON/H'";
            std::cerr<<"\n        found line: " << line;
            throw std::runtime_error
            ("ionex::skip_tec_map() -> Invalid line");
#endif
            return 1;
        }
        // read and validate the current latitude
        ltmp = std::strtof(line+2, &c);
        if ( (int)(ltmp*100) != (int)(lat*100) ) {
#ifdef DEBUG
            std::cerr<<"\n[DEBUG]What the fuck man! Read invalid latitude.";
            std::cerr<<"\n       Expected: "<<lat<<", found: "<<ltmp;
            throw std::runtime_error("ionex::skip_tec_map() -> Invalid latitude");
#endif
            return 1;
        } else {
#ifdef DEBUG
//            std::cout<<"\n[DEBUG] Reading and ignoring map for lat = "<<ltmp;
#endif
        }

        // ok, now we should read these fucking TEC vals; format: I5 max 16 values
        // per line.
        for (std::size_t i=0; i<num_of_tec_lines; ++i) {
            if ( !_istream.getline(line, MAX_HEADER_CHARS) ) {
#ifdef DEBUG
                std::cerr<<"\n[DEBUG] Fucking weird! Failed to read tec map!";
                throw std::runtime_error("ionex::skip_tec_map() -> Invalid line");
#endif
                return 1;
            }
        }

        // did we end yet (better compare ints than floats)?
        lat += _dlat;
        if ( ascending == ((long)(lat*100)>=(long)(_lat2*100)) ) {
            break;
        }
    }

    // should now read 'END OF TEC MAP'
    if (  !_istream.getline(line, MAX_HEADER_CHARS)
        ||std::strncmp(line+60, "END OF TEC MAP", 14) )
    {
#ifdef DEBUG
        std::cerr<<"\n[DEBUG] Expected \"END OF TEC MAP\" but found:";
        std::cerr<<"\n        "<<line;
        throw std::runtime_error
            ("ionex::skip_tec_map() -> Invalid line");
#endif
            return 1;
    }

    // all done probably correctly.
    return 0;
}

/** Read a TEC map for a given epoch. All values are going to be read into the
 *  vector tec_vals, at the order they are read. This means that the vector
 *  will be of the form (at exit):
\verbatim
    vec[0]   tec value at -> lat1, lon1
    vec[1]   tec value at -> lat1, lon1+dlon
    vec[2]   tec value at -> lat1, lon1+2*dlon
    ...
    vec[n]   tec value at -> lat1+dlat,   lon1
    vec[n+1] tec value at -> lat1+2*dlat, lon1+dlon
    ...
\endverbatim
 *  The function will stop after rading the line: 'END OF TEC MAP'.
 *
 *  \param[in] pcv_vals An int vector of size large enough to hold all tec
 *             values to be read from the map.
 *
 *  \returns An integer denoting the exit status; everything other than 0,
 *           denotes an error.
 *
 *  \warning -# The buffer should be placed in a position such that the next line
 *           to be read is "LAT/LON1/LON2/DLON/H".
 *           -# The size of the vector should be large enough to hold the read
 *           values. The size of the vector will not be modified at any way.
 *           -# Note that the get the actual TEC values from the values stored
 *           in the vector, you will need to use the IONEX's exponent.
 */
int
ionex::read_tec_map(std::vector<int>& pcv_vals)
{
    static char line[MAX_HEADER_CHARS];

    // stream should definitely be open!
    // TODO move this somewhere else. don't fucking need to always check.
    assert( _istream.is_open() );
    
    std::size_t index = 0;
    
    // how many consti-latitude maps should we read ?
    std::size_t lat_maps  (this->latitude_maps() );
    // each const-latitude map has how many tec lines ?
    std::size_t lon_lines (this->longtitude_lines() );

    // reset errno
    errno = 0;

    for (std::size_t i=0; i<lat_maps; ++i) {
        if ( this->read_latitude_map(lon_lines, pcv_vals, index) ) {
            return 1;
        }
    }
    
    // should now read 'END OF TEC MAP'
    if ( ! _istream.getline(line, MAX_HEADER_CHARS)
        || std::strncmp(line+60, "END OF TEC MAP", 14) ) {
#ifdef DEBUG
        std::cerr<<"\n[DEBUG] Expected \"END OF TEC MAP\" but found:";
        std::cerr<<"\n        "<<line;
        throw std::runtime_error
            ("ionex::read_tec_map() -> Invalid line");
#endif
            return 1;
    }

    return 0;
}

/** Read a TEC map (for a given latitude) off from the stream and store the
 *  values in the vec vector, starting at vec[index]. The function will modify
 *  the index value, such that at return it will denote the last index inserted
 *  into the vector plus one.
 *
 *  \param[in] num_of_tec_lines The number of lines to read off from a const
 *                              latitude map.
 *  \param[in] vec              The (int) vector where read values are stored.
 *                              This function will start storing values at
 *                              vec[index].
 *  \param[in] index            Where to start storing tec values within the
 *                              input vec vector.
 *                              At exit, this will be set to the first non-set
 *                              element (i.e. if index was 0 at input and the
 *                              function reads and assigns 10 elements, index
 *                              at output will be 10).
 * 
 *  \warning -# The buffer should be placed in a position such that the next line
 *           to be read is "LAT/LON1/LON2/DLON/H".
 *           -# Also, always clear errno before calling this!
 *           -# When this function returns, errno should be what is was before
 *           the function call.
 * 
 */ 
int
ionex::read_latitude_map(std::size_t num_of_tec_lines,
                         std::vector<int>& vec,
                         std::size_t& index)
{
    static char line [MAX_HEADER_CHARS];
    datetime_ms d;
    char* start, *end;
    ionex_grd_type lat, lon1, lon2, dlon, hgt;
    long lval;
    int  prev_errno = errno;
    errno = 0;
    
    // next line should be 'LAT/LON1/LON2/DLON/H'
    if ( !_istream.getline(line, MAX_HEADER_CHARS) 
         || std::strncmp(line+60, "LAT/LON1/LON2/DLON/H", 20) )
    {
        errno = prev_errno;
#ifdef DEBUG
        std::cerr<<"\n[DEBUG] Error reading TEC map 'LAT/LON1/LON2/DLON/H'";
        std::cerr<<"\n        found line: " << line;
        throw std::runtime_error
        ("ionex::read_map() -> Invalid line");
#endif
        return 1;
    }
    // resolve the lat/lon limits.
    start = line+2;
    lat  = std::strtof(start, &end);
    start += 6;
    lon1 = std::strtof(start, &end);
    start += 6;
    lon2 = std::strtof(start, &end);
    start += 6;
    dlon = std::strtof(start, &end);
    start += 6;
    hgt  = std::strtof(start, &end);
#ifdef DEBUG
    if ( lon1!=_lon1 || lon2!=_lon2 || dlon!=_dlon || hgt != _hgt1 ) {
        errno = prev_errno;
        std::cerr << "\n[DEBUG] Oh Fuck! this longtitude seems corrupt!";
        std::cerr << "\n        line: " << line;
        std::string lat_str = std::to_string(lat);
        throw std::runtime_error("ionex::read_map() -> Invalid line ("+lat_str+")");
    }
#endif

    // ok, now we should read these fucking TEC vals; format: I5 max 16 values
    // per line.
    for (std::size_t i=0; i<num_of_tec_lines; ++i) {
        if ( !_istream.getline(line, MAX_HEADER_CHARS) ) {
            errno = prev_errno;
#ifdef DEBUG
            std::cerr<<"\n[DEBUG] Fucking weird! Failed to read tec map!";
            throw std::runtime_error("ionex::read_map() -> Invalid line");
#endif
            return 1;
        }
        start = end = line;
        for (lval = std::strtol(start, &end, 10);
             start != end;
             lval = std::strtol(start, &end, 10) )
        {
            vec[index] = static_cast<int>( lval );
            ++index;
            start = end;
        }
    }

    // check for transformation errors!
    if ( errno ) {
        errno = prev_errno;
#ifdef DEBUG
        std::cerr<<"\n[DEBUG] Fuck! Reading tec values failed!";
        throw std::runtime_error("ionex::read_map() -> Invalid line");
#endif
        return 1;
    }

    // all done probably correctly.
    return 0;
}

/** Perform bilinear interpolation given a grid of tec values (tec_vals), to 
 *  find tec value at a point at point (longtitude, latitude). The function
 *  needs to know the surrounding cell of the point and the number of longtitude
 *  points (i.e. the number of nodes in the longtitude axis).
 *
 *  \param[in] longtitude The longtitude of the point to interpolate at (in
 *                        degrees).
 *  \param[in] latitude   The latitude of the point to interpolate at (in
 *                        degrees).
 *  \param[in] tec_vals   The grid of tec values.
 *  \param[in] cell       The surrounding cell (as provided by the grid_skeleton
 *                        neighbor_nodes function).
 *  \param[in] lon_grid_size Number of nodes in the longtitude axis (the axis
 *                        of the corresponding grid_skeleton instance).
 *  \return               The tec value at the given point (to get the TEC
 *                        value you need to scale this value by the exponent).
 *
 */
ionex_grd_type
bilinear_interpolate(float longtitude, float latitude,
                     std::vector<int>& tec_vals,
                     tec_cell&         cell,
                     std::size_t       lon_grid_size)
{
    // resolve the tuple
    std::size_t x0_idx = std::get<0>( cell );
    std::size_t y0_idx = std::get<4>( cell );
#ifdef DEBUG
    std::size_t x1_idx = std::get<2>( cell );
    std::size_t y1_idx = std::get<6>( cell );
    assert( x1_idx == x0_idx + 1 );
    assert( y1_idx == y0_idx + 1 );
#else
    std::size_t x1_idx = x0_idx + 1;
    std::size_t y1_idx = y0_idx + 1;
#endif

    // do not forget the scale factor !!
    ionex_grd_type x0 = std::get<1>(cell)/100.0f;
    ionex_grd_type x1 = std::get<3>(cell)/100.0f;
    ionex_grd_type y0 = std::get<5>(cell)/100.0f;
    ionex_grd_type y1 = std::get<7>(cell)/100.0f;

    // get the tec values at the indexes we want
    std::size_t low_left = y0_idx * lon_grid_size + x0_idx;
#ifdef DEBUG
    assert( low_left+1+lon_grid_size < tec_vals.size() );
#endif
    ionex_grd_type f00 = static_cast<ionex_grd_type>(tec_vals[low_left]);
    ionex_grd_type f01 = static_cast<ionex_grd_type>(tec_vals[low_left+lon_grid_size]);
    ionex_grd_type f10 = static_cast<ionex_grd_type>(tec_vals[low_left+1]);
    ionex_grd_type f11 = static_cast<ionex_grd_type>(tec_vals[low_left+1+lon_grid_size]);

#ifdef DEBUG
//std::cout <<"\nInterpolating at: ("<<longtitude<<", "<<latitude<<")";
//std::cout <<"\n("<<x0_idx<<", "<<y1_idx<<") ----   ("<<x1_idx<<", "<<y1_idx<<")";
//std::cout <<"\n  |                  |";
//std::cout <<"\n  |                  |";
//std::cout <<"\n  |                  |";
//std::cout <<"\n("<<x0_idx<<", "<<y0_idx<<") ----   ("<<x1_idx<<", "<<y0_idx<<")";
//std::cout<<"\n";
//std::cout <<"\nInterpolating at: ("<<longtitude<<", "<<latitude<<")";
//std::cout <<"\n("<<x0<<", "<<y1<<")="<<f01<<" ----   ("<<x1<<", "<<y1<<")="<<f11;
//std::cout <<"\n  |                  |";
//std::cout <<"\n  |                  |";
//std::cout <<"\n  |                  |";
//std::cout <<"\n("<<x0<<", "<<y0<<")="<<f00<<" ----   ("<<x1<<", "<<y0<<")="<<f10;
//std::cout<<"\n";
#endif

    // bilinear interpolation, see
    // https://en.wikipedia.org/wiki/Bilinear_interpolation
    ionex_grd_type x     { longtitude };
    ionex_grd_type y     { latitude };
    ionex_grd_type denom { (x1-x0)*(y1-y0) };

    return  ((x1-x)*(y1-y)/denom)*f00
           +((x-x0)*(y1-y)/denom)*f10
           +((x1-x)*(y-y0)/denom)*f01
           +((x-x0)*(y-y0)/denom)*f11;
}

/** Extract TEC values for a given list of points, for all epochs included in 
 *  the IONEX instance. For example, if points holds (p1, p2, ..., pn2), then
 *  (on exit), the tec_vals will be formed as:
 *  tec_vals[0][0] -> tec at point p0, at epoch epoch_vector[0]
 *  tec_vals[0][1] -> tec at point p0, at epoch epoch_vector[1]
 *  ...
 *  tec_vals[0][m] -> tec at point p0, at epoch epoch_vector[m]
 *  tec_vals[1][0] -> tec at point p1, at epoch epoch_vector[0]
 *  tec_vals[1][1] -> tec at point p1, at epoch epoch_vector[1]
 *  ...
 *  tec_vals[1][m] -> tec at point p1, at epoch epoch_vector[m]
 *  ....
 *
 *  \param[in] points A vector of coordinates of type (longtitude, latitude)
 *                    in (decimal) degrees (two decimal places are considered)
 *  \param[in] epoch_vector This vector will contain (at exit) the epochs for
 *                    which the function computed TEC values.
 *  \param[in] tec_vals A vector of vectors of ints! For each point in points,
 *                    there will be a corresponding vector of ints in tec_vals
 *                    holding tec values at epoch_vector epochs.
 *  \returns          A vector of int vectors; for each point given, the function
 *                    will create for a vector of tec values for each epoch in the
 *                    epoch_vector
 *  \warning          All (input) vectors should be large enough to handle the
 *                    values assigned to them.
 * 
 */
int
ionex::get_tec_at(const std::vector<std::pair<ionex_grd_type,ionex_grd_type>>& points,
                std::vector<datetime_ms>& epoch_vector,
                std::vector<std::vector<int>>& tec_vals
                )
{

    // for every point we want, we must find an index for it (within the grid)
    // so that we can extract it's value. Let's make a 2D grid. For ease, let's
    // set the grid points to long (instead of ints) so that e.g. lat=37.5
    // lon=23.7 will be 3750 and 2370; i.e. use of factor of 100.
    int factor (100);
#ifdef DEBUG
    grid_skeleton<long,  true, Grid_Dimension::TwoDim> 
        grid(_lon1*factor, _lon2*factor, _dlon*factor,
             _lat1*factor, _lat2*factor, _dlat*factor);
#else
    grid_skeleton<long, false, Grid_Dimension::TwoDim> 
        grid(_lon1*factor, _lon2*factor, _dlon*factor,
             _lat1*factor, _lat2*factor, _dlat*factor);
#endif

    // for each point in the vector, we are going to need the indexes of the
    // surounding nodes (so that we extract these values and interpolate).
    std::vector<tec_cell> indexes;
    indexes.reserve( points.size() );
    for ( auto const& i : points ) {
        indexes.emplace_back(grid.neighbor_nodes( 
                                 i.first*factor, i.second*factor ));
    }

    // get me a vector large enough to hold a whole map
    std::vector<int> tec_map (grid.size(), 0);

    // clear the vector of epochs
    epoch_vector.clear();

    // go to 'END OF HEADER'
    char line[MAX_HEADER_CHARS];
    _istream.seekg(_end_of_head, std::ios::beg);
    std::size_t map_num = 0;
    char* c;
    
    if ( !_istream.getline(line, MAX_HEADER_CHARS) ) {
#ifdef DEBUG
        std::cerr<<"\n[DEBUG] WTF? Failed to read input line!";
        throw std::runtime_error
            ("ionex::get_tec_at() -> Invalid line!");
#endif
        return 1;
    }

    datetime_ms cur_dt = _first_epoch;
    
    while ( map_num < _maps_in_file
            && _istream 
            && !std::strncmp(line+60, "START OF TEC MAP", 16) ) {

        *(line+6) = '\0';
        assert( static_cast<long>(map_num+1) == std::strtol(line, &c, 10) );
        if ( ! _istream.getline(line, MAX_HEADER_CHARS)
            || std::strncmp(line+60, "EPOCH OF CURRENT MAP", 20) 
            || _read_ionex_datetime_(line, &cur_dt) )
        {
#ifdef DEBUG
            std::cerr<<"\n[DEBUG] Expected line \"EPOCH OF CURRENT MAP\", found:";
            std::cerr<<"\n        "<<line;
            throw std::runtime_error
                ("ionex::get_tec_at() -> Invalid line!");
#endif
            return 1;
        }

        // read the map into memmory
        if ( this->read_tec_map(tec_map) ) {
#ifdef DEBUG
            std::cerr<<"\n[DEBUG] Failed reading map nr "<<map_num;
            throw std::runtime_error
                ("ionex::get_tec_at() -> failed reading maps.");
#endif
            return 1;
        }

        // ok. we got the map and we need to extract the cells for all points
        // index; points to current point
        std::size_t j = 0;
        for (const auto& p : points) {
            tec_vals[j].emplace_back( bilinear_interpolate(
                                        p.first, p.second, tec_map, indexes[j], 
                                        grid.x_axis_pts()) );
            epoch_vector.emplace_back( cur_dt );
            ++j;
        }
        _istream.getline(line, MAX_HEADER_CHARS);
        ++map_num;
    }

    // if everything went as planned, we should have read all maps in file.
    return !(map_num == _maps_in_file);
}

/**
 *  \param[in] ifrom  Starting epoch; if not set it will be equal to the first
 *                    epoch in the IONEX file. If it is prior to the first epoch
 *                    in the file, it will be adjusted.
 *  \param[in] ito    Ending epoch; if not set it will be equal to the last
 *                    epoch in the IONEX file. If it is past the last epoch
 *                    in the file, it will be adjusted.
 *  \param[in] interval The time step with which to extract the TEC values. If
 *                    set to '0', it will be set equal to the interval in the
 *                    IONEX file. The value denotes (integer) seconds.
 */
std::vector<std::vector<int>>
ionex::interpolate(const std::vector<std::pair<ionex_grd_type,ionex_grd_type>>& points,
                std::vector<datetime_ms>& epochs,
                datetime_ms* ifrom,
                datetime_ms* ito,
                int interval
                )
{
    datetime_ms from ( ifrom ? (*ifrom) : _first_epoch );
    datetime_ms to   ( ito   ? (*ito)   : _last_epoch  );

    if (interval < 0 ) {
#ifdef DEBUG
        std::cerr<<"\n[DEBUG] Fuck off; setting interval to 0";
#endif
        interval = 0;
    }

    if ( from > to ) {
        throw std::runtime_error
            ("ionex::interpolate() -> Invalid epochs passed!");
    }

    // if (from == to) i'm returning bitch!
    if ( from == to ) { 
        return std::vector<std::vector<int>> ( points.size() );
    }

    if ( from < _first_epoch ) {
#ifdef DEBUG
        std::cerr<< "\n[DEBUG] First epoch is beyond the first epoch in file; adjusted.";
#endif
        from = _first_epoch;
    }

    if ( to > _last_epoch ) {
#ifdef DEBUG
        std::cerr<< "\n[DEBUG] Ending epoch is beyond the last epoch in file; adjusted.";
#endif
        to = _last_epoch;
    }

    if ( !interval ) {
        interval = this->_interval;
    }

    // we must provide an epoch vector
    std::vector<datetime_ms> epoch_vector_1( this->_maps_in_file );

    std::vector<datetime_ms> epoch_vector_2( (interval
                          ? static_cast<std::size_t>(to.delta_sec(from))/interval+1
                          : this->_maps_in_file) );

    // we must also provide a vector of vectors, where
    // vec[i][j] is the tec value for station #i at epoch #j
    std::vector<std::vector<int>> tec_vals_1 ( points.size(),
                                std::vector<int>(epoch_vector_1.size(), 9999) );
    std::vector<std::vector<int>> tec_vals_2 ( points.size(),
                                std::vector<int>(epoch_vector_2.size(), 9999) );

    // get the tec/epoch values that are recorded in the IONEX file, for
    // the points in the list
    if ( this->get_tec_at(points, epoch_vector_1, tec_vals_1) ) {
#ifdef DEBUG
        std::cerr<<"\n[DEBUG] Failed to read te/epochs from IONEX.";
#endif
        throw std::runtime_error
            ("ionex::interpolate() -> failed to read tecs/epochs.");
    }

    // Sweet! now perform the interpolation in time.
    std::size_t i = 0;
    std::size_t j = i+1;
    std::size_t k = 0;
    auto time_i   = epoch_vector_1.cbegin();
    auto time_j   = epoch_vector_1.cbegin() + 1;
    auto cur_time = from;
    double coef1, coef2;
    while ( cur_time <= to ) {
        if ( cur_time > *time_i ) {
            ++time_i;
            ++i;
            if ( j < epoch_vector_1.size()-1 ) {
                ++time_j;
                ++j;
            }
        }
#ifdef DEBUG
        //TODO this check fails cause of fucking accuracy!
        //if (cur_time < *time_i || cur_time > *time_j ) {
        // use this instead:
        if ( (int)cur_time.delta_sec(*time_i)<0 || (int)time_j->delta_sec(cur_time)<0 ) {
            std::cerr<<"\n[DEBUG] Fucked up in time interpolation!";
            std::cout<<"\ni="<<i<<" and j="<<j<<"t-ti="<<cur_time.delta_sec(*time_i)<<", tj-t="<<time_j->delta_sec(cur_time);
            throw std::runtime_error
                ("ionex::interpolate() -> Bad interpolation limits.");
        }
#endif
        // TODO handle missing values (9999) !!
        epoch_vector_2[k] = cur_time;
        if ( cur_time == *time_i ) {
            coef1 = 1.0e0;
            coef2 = 0.0e0;
            for (std::size_t p=0; p<points.size(); ++p) {
                tec_vals_2[p][k] = tec_vals_1[p][i];
            }
        } else {
            coef1 = time_j->delta_sec(cur_time)/time_j->delta_sec(*time_i);
            coef2 = cur_time.delta_sec(*time_i)/time_j->delta_sec(*time_i);
            for (std::size_t p=0; p<points.size(); ++p) {
                tec_vals_2[p][k] = coef1*tec_vals_1[p][i] + coef2*tec_vals_1[p][j];
            }
        }
        ++k;
        if ( !interval ) {
            if ( k==epoch_vector_1.size() ) {
                cur_time.add_seconds(86400.0);
            } else {
                cur_time = *time_j;
            }
        } else {
            cur_time.add_seconds( (double)interval );
        }
    }

    epochs = std::move( epoch_vector_2 );
    return tec_vals_2;
}