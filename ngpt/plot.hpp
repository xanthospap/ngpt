#ifndef _QWT_PLOT_H_
#define _QWT_PLOT_H_

#include <qwt_polar_plot.h>
#include "antex.hpp"

class QwtPolarGrid;
class QwtPolarCurve;

class PlotSettings
{
public:
    /*enum Curve {
        Spiral, Rose, NumCurves
    };*/

    enum Flag
    {
        MajorGridBegin,
        MinorGridBegin  = MajorGridBegin + QwtPolar::ScaleCount,
        AxisBegin       = MinorGridBegin + QwtPolar::ScaleCount,
        AutoScaling     = AxisBegin + QwtPolar::AxesCount,
        Inverted,
        Logarithmic,
        Antialiasing,
        CurveBegin,
        NumFlags        = CurveBegin/* + NumCurves*/
    };

    bool flags[NumFlags];
};

class Plot
: public QwtPolarPlot
{
    Q_OBJECT

public:
    Plot(QWidget* = NULL);
    PlotSettings settings() const;

public Q_SLOTS:
    void applySettings(const PlotSettings&);

private:
    QwtPolarCurve* createCurve(int id) const;
    QwtPolarCurve* create_pcv_data(ngpt::antex*, const QString&);

    QwtPolarGrid*  d_grid;
    QwtPolarCurve* d_curve;
    /*QwtPolarCurve* d_curve[PlotSettings::NumCurves];*/
};

#endif
