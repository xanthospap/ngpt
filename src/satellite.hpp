#ifndef __GNSS_SATELLITES_HPP
#define __GNSS_SATELLITES_HPP

#include "satsys.hpp"

namespace ngpt
{

class satellite
{
public:
    /// Default constructor
    explicit satellite(ngpt::satellite_system s=ngpt::satellite_system::mixed,
                       int prn=0) noexcept
        : _sys{s}, _prn{prn}
    {}
    /// Constructor from a string
    explicit satellite(const char*);
    /// Copy constructor
    satellite(const satellite&) noexcept = default;
    /// Move Constructor
    satellite(satellite&&) noexcept = default;
    /// Assignment
    satellite& operator=(const satllite&) noexcept = default;
    /// Move assignment
    satellite& operator=(satellite&&) noexcept = default;
    /// Destructor
    ~satellite() noexcept {};
private:
    ngpt::satellite_system _sys;
    int                    _prn;
}; // satellite

} // ngpt

#endif
