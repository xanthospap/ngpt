#ifndef __ANTPCV_HPP__
#define __ANTPCV_HPP__

#include "grid.hpp"
#include "obstype.hpp"

namespace ngpt
{
class frequency_pcv
{
public:

private:
  ngpt::ObservationType    type_;
  float                    eccentricity_vector_[3];
  float*                   no_azi_pcv_values_;
  float*                   azi_pcv_values_;
};

/// Antenna eccentricity vector.
/// - Receiver antenna:
///   Eccentricities of the mean antenna phase center relative to the antenna 
///   reference point (ARP). North, east and up component (in millimeters).
/// - Satellite antenna:
///   Eccentricities of the mean antenna phase center relative to the center of
///   mass of the satellite in X-, Y- and Z-direction (in millimeters).
///
struct antenna_eccentricity
{
  double north_, east_, up_;
};

/// Non-azimouth dependent pattern (that has to be specified in any case). 
/// Phase pattern values in millimeters from 'ZEN1' to 'ZEN2' (with increment 
/// 'DZEN').
class noazi_pcv
: public ngpt::GridSkeleton<float, false, ngpt::Grid_Dimension::OneDim>
{
public:
  explicit noazi_pcv(float zen1, float zen2, float dzen, int freqs) noexcept
    : ngpt::GridSkeleton<float, false, ngpt::Grid_Dimension::OneDim>
    (zen1, zen2, dzen),
    pcv_values_.reserve(freqs)
    {
      for (auto& i : pcv_values_) {
        i.reserve( this->size() );
      }
    }
private:
  std::vector<std::vector<float>>    pcv_values_;
  std::vector<ngpt::ObservationType> obs_types_;
};

/// 
class azi_pcv
: public ngpt::GridSkeleton<float, false, ngpt::Grid_Dimension::TwoDim>
{
public:
  explicit noazi_pcv(float zen1, float zen2, float dzen, 
      float azi1, float azi2, float dazi, int freqs) noexcept
    : ngpt::GridSkeleton<float, false, ngpt::Grid_Dimension::TwoDim>
    (float zen1, float zen2, float dzen, float azi1, float azi2, float dazi),
    pcv_values_.reserve(freqs)
    {}
};

} // end namespace

#endif
