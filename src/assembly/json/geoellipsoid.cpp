#include <geoellipsoid.h>

bool GeoEllipsoid::operator==(const GeoEllipsoid& o) const
{
    return util::DBL_EQUAL(this->_a, o._a) && util::DBL_EQUAL(this->_invf, o._invf);
}

GeoEllipsoid& GeoEllipsoid::operator=(const GeoEllipsoid& o)
{
    if (this != &o)
    {
        this->_a = o._a;
        this->_invf = o._invf;
    }
    return *this;
}

double GeoEllipsoid::getSemiMinorAxis() const
{
    return _a * (1.0 - 1.0 / _invf);
}

void GeoEllipsoid::setSemiMinorAxis(double v)
{
    _invf = _a / (_a - v);
}

double GeoEllipsoid::getFlattening() const
{
    return 1.0 / _invf;
}

void GeoEllipsoid::setFlattening(double v)
{
    _invf = 1.0 / v;
}

double GeoEllipsoid::getEccentricity() const
{
    if (util::DBL_EQUAL(_invf, 0))
    {
        return 0.0;
    }
    if (_invf < 0.5)
    {
        return -1.0;
    }
    return sqrt(2.0 / _invf - 1.0 / (_invf * _invf));
}

double GeoEllipsoid::getSquaredEccentricity() const
{
    double e = getEccentricity();
    return pow(e, 2);
}