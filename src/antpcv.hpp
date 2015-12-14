#ifndef __ANTPCV_HPP__
#define __ANTPCV_HPP__

#include <array>
#include "grid.hpp"
#include "obstype.hpp"

namespace ngpt
{

///  Hold phase center variation pattern for a single frequency (i.e. 
///  ObservationType). This class will hold:
///  - the antenna phase center offset vector
///  - 'NO-AZI' (i.e. non-azimouth dependent) phase center variation values
///  - 'AZI' (i.e. azimouth dependent) phase center variation values
///
/// \note This class has absolutely no clue of the grid these values correspond
///       to (i.e. starting/ending azimouth, starting/ending zenith angle).
///       Only vectors of pcv values are stored here, which actualy cannot be
///       correctly indexed.
///
class frequency_pcv
{

typedef std::vector<float>  fltvec;
typedef std::array<float,3> fltarr;

public:
    explicit frequency_pcv(ngpt::ObservationType type, 
             std::size_t no_azi_hint = 1, std::size_t azi_hint = 1)
    noexcept
        : type_(type),
          eccentricity_vector_{.0e0, .0e0, .0e0}
    {
      no_azi_pcv_values_.reserve(no_azi_hint);
      azi_pcv_values_.reserve(azi_hint);
    }

    explicit 
    frequency_pcv(std::size_t no_azi_hint = 1, std::size_t azi_hint = 1) 
    noexcept
        : type_(),
          eccentricity_vector_{.0e0, .0e0, .0e0}
    {
      no_azi_pcv_values_.reserve(no_azi_hint);
      azi_pcv_values_.reserve(azi_hint);
    }

  ~frequency_pcv() noexcept = default;

  frequency_pcv(const frequency_pcv& rhs) noexcept(
         std::is_nothrow_copy_constructible<fltarr>::value
      && std::is_nothrow_copy_constructible<fltvec>::value 
      ) = default;
#ifdef DEBUG
//  TODO:: Test#2 fails, i.e. std::vector<float> is not noexcept for the copy
//         constructor.
//
//  static_assert( std::is_nothrow_copy_constructible<std::array<float,3>>::value,
//      "std::array<float,3> -> throws for copy c'tor!" );
//  static_assert( std::is_nothrow_copy_constructible<std::vector<float>>::value,
//      "std::vector<float> -> throws for copy c'tor!" );
#endif

  frequency_pcv(frequency_pcv&& rhs) noexcept(
         std::is_nothrow_move_constructible<fltarr>::value
      && std::is_nothrow_move_constructible<fltvec>::value 
      ) = default;
#ifdef DEBUG
    static_assert( std::is_nothrow_move_constructible<fltarr>::value,
      "std::array<float,3> -> throws for copy c'tor!" );
    static_assert( std::is_nothrow_move_constructible<fltvec>::value,
      "std::vector<float> -> throws for copy c'tor!" );
#endif

  frequency_pcv& operator=(const frequency_pcv& rhs) noexcept(
         std::is_nothrow_copy_assignable<fltarr>::value
      && std::is_nothrow_copy_assignable<fltvec>::value 
      ) = default;

  frequency_pcv& operator=(frequency_pcv&& rhs) noexcept(
         std::is_nothrow_move_assignable<fltarr>::value
      && std::is_nothrow_move_assignable<fltvec>::value 
      ) = default;

private:
    ngpt::ObservationType type_;                 ///< Observation type
    fltarr                eccentricity_vector_;  ///< phase center offset
    fltvec                no_azi_pcv_values_;    ///< 'NO_AZI' pcv
    fltvec                azi_pcv_values_;       ///< 'AZI' pcv
};

namespace antenna_pcv_details
{
    /// If we have azimout-dependent calibration, the min azimouth is always
    /// 0 degrees.
    constexpr float azi1 {    .0e0 };
    /// If we have azimout-dependent calibration, the max azimouth is always
    /// 360 degrees.
    constexpr float azi2 { 360.0e0 };
}

///  This class hold all information for a given antenna recorded in an ANTEX
///  file. Since all frequency pcv patterns correspond to the same grid (i.e.
///  the zen1, zen2, dzen, azi1, azi2 and dazi) are the same for all recorded
///  frequencies, the class has a Grid for the 'NO-AZI' pcv's and one for the
///  azimouth-dependent patterns (if and only if dazi != 0).
///  Apart from the grid(s), the class also hold a vector of frequency_pcv
///  instances, one per each frequency recorded in the ANTEX file.
///
class antenna_pcv
{

typedef GridSkeleton<float, false, Grid_Dimension::OneDim> dim1_grid;
typedef GridSkeleton<float, false, Grid_Dimension::TwoDim> dim2_grid;
typedef std::vector<frequency_pcv>                         fr_pcv_vec;

public:
  
    /// Constructor. 
    explicit antenna_pcv(float zen1, float zen2, float dzen,
                         int freqs, float dazi = 0)
    /*noexcept*/
        : no_azi_grid_(zen1, zen2, dzen),
          azi_grid_(dazi
                    ? new dim2_grid{zen1, zen2, dzen, antenna_pcv_details::azi1, antenna_pcv_details::azi2, dazi}
                    : nullptr)
    {
        assert( dzen > .0e0 );

        std::size_t no_azi_hint = no_azi_grid_.size();
        std::size_t    azi_hint = ( azi_grid_ ) ? ( azi_grid_->size() ) : ( 0 );
    
        for (int i=0; i<freqs; ++i) {
            freq_pcv.emplace_back( no_azi_hint, azi_hint );
        }
    }

    /// Copy constructor.
    antenna_pcv(const antenna_pcv& other)
        : no_azi_grid_ {other.no_azi_grid_},
          azi_grid_    {other.azi_grid_
            ? new dim2_grid(other.zen1(), other.zen2(), other.dzen(), antenna_pcv_details::azi1, antenna_pcv_details::azi2, other.dazi())
            : nullptr },
        freq_pcv       {other.freq_pcv}
    {}

    /// Move constructor.
    antenna_pcv(antenna_pcv&& other)
        : no_azi_grid_{ std::move(other.no_azi_grid_) },
          azi_grid_   { std::move(other.azi_grid_)    },
          freq_pcv    { std::move(other.freq_pcv)     }
    {
        other.azi_grid_ = nullptr;
    }

    /// Destructor
    ~antenna_pcv() noexcept
    {
        delete azi_grid_;
    }

    float zen1() const noexcept { return no_azi_grid_.from(); }
    float zen2() const noexcept { return no_azi_grid_.to();   }
    float dzen() const noexcept { return no_azi_grid_.step(); }
    bool  has_azi_pcv() const noexcept { return azi_grid_ != nullptr; }
    /// \warning Watch yourself bitch! can cause UB if azi_grid_ is invalid.
    float azi1() const noexcept { return azi_grid_->y_axis_from(); }
    /// \warning Watch yourself bitch! can cause UB if azi_grid_ is invalid.
    float azi2() const noexcept { return azi_grid_->y_axis_to();   }
    /// \warning Watch yourself bitch! can cause UB if azi_grid_ is invalid.
    float dazi() const noexcept { return azi_grid_->y_axis_step(); }

private:
    dim1_grid  no_azi_grid_; ///< Non-azimouth dependent grid (skeleton)
    dim2_grid* azi_grid_;    ///< Azimouth dependent grid (skeleton)
    fr_pcv_vec freq_pcv;     ///< A vector of frequency_pcv
};

} // end namespace

#endif
