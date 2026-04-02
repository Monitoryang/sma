#pragma once

#ifndef DGR_GEO_ELLIPSOID_H
#define DGR_GEO_ELLIPSOID_H

#include <core.h>

class DGR_DLL GeoEllipsoid final
{
public:
    GeoEllipsoid(double a, double invf) : _a(a), _invf(invf) { }

    ~GeoEllipsoid() = default;

    bool operator==(const GeoEllipsoid& o) const;

    inline bool operator!=(const GeoEllipsoid& o) const
    {
        return !operator==(o);
    }

    GeoEllipsoid& operator=(const GeoEllipsoid& o);

public:
    inline double getSemiMajorAxis() const
    {
        return _a;
    }

    inline void setSemiMajorAxis(double v)
    {
        _a = v;
    }

    inline double getInverseFlattening() const
    {
        return _invf;
    }

    inline void setInverseFlattening(double v)
    {
        _invf = v;
    }

    double getSemiMinorAxis() const;
    void setSemiMinorAxis(double v);

    double getFlattening() const;
    void setFlattening(double v);

    double getEccentricity() const;
    double getSquaredEccentricity() const;

private:
    double _a, _invf;
};

#endif // !DGR_GEO_ELLIPSOID_H