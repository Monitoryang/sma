#include <geospheroid.h>

GeoSpheroid::GeoSpheroid() : _dfRadius(constant::WGS84_RADIUS) {}

GeoSpheroid::GeoSpheroid(double dfRadius) : _dfRadius(dfRadius) {}

double GeoSpheroid::getRadius() const
{
    return _dfRadius;
}

void GeoSpheroid::setRadius(double radius)
{
    if (!(abs(_dfRadius - radius) < constant::DBL_EPS))
    {
        _dfRadius = radius;
    }
}

double GeoSpheroid::wrap360(double degree)
{
    if (0 <= degree && degree < 360)
    {
        return degree;
    }
    return std::fmod((std::fmod((360 * degree / 360), 360) + 360), 360);
}

inline double GeoSpheroid::getNormalLatitude(double latitude)
{
    return std::fmax(-90, std::fmin(90, latitude));
}

inline double GeoSpheroid::getNormalLongitude(double longitude)
{
    double remainder = std::fmod(longitude + 180.0, 360.0);
    return std::fmod(remainder + 360.0, 360.0) - 180.0;
}

inline double GeoSpheroid::getMercatorLatitude(double latitude)
{
    return std::fmax(-85.05113, std::fmin(latitude, 85.05113));
}

bool GeoSpheroid::checkNormalLatitude(double latitude)
{
    if (abs(latitude) <= 90)
    {
        return true;
    }

    return false;
}

bool GeoSpheroid::checkNormalLongitude(double longitude)
{
    if (abs(longitude) <= 180)
    {
        return true;
    }

    return false;
}

bool GeoSpheroid::checkMercatorLatitude(double latitude)
{
    if (abs(latitude) <= 85.05113)
    {
        return true;
    }

    return false;
}

bool GeoSpheroid::checkCoordinate(const Coordinate &p)
{
    return checkNormalLatitude(p.latitude) && checkNormalLongitude(p.longitude);
}

bool GeoSpheroid::checkSegment(const Segment &Segment)
{
    return checkCoordinate(Segment.first) && checkCoordinate(Segment.second);
}

double GeoSpheroid::fromMileToDegree(double mile)
{
    double M = std::abs(mile);
    double R = getRadius();

    return M * 180.0 / (constant::PI * R);
}

double GeoSpheroid::fromDegreeToMile(double degree)
{
    double D = std::abs(degree);
    double R = getRadius();

    return constant::PI * R / 180.0 * D;
}

double GeoSpheroid::fromDegreeToRadian(double degree)
{
    return degree * constant::PI / 180.0;
}

double GeoSpheroid::fromRadianToDegree(double radian)
{
    return radian * 180.0 / constant::PI;
}

// double  GeoSpheroid::fromDegreeAreaToMileArea(double degree_2)
// {
//     double S = std::abs(degree_2);
//	   double R = getRadius();
//	   double k = 2 * constant::PI * R / 360;
//	   return  k * k* S;
// }

Coordinate GeoSpheroid::fromWebMercatorToGeogCS(const Coordinate &p)
{
    double x = p.longitude;
    double y = p.latitude;

    double phi = 2.0 * (std::atan(std::exp(y / getRadius())) - constant::PI / 4.0);
    double lambda = x / getRadius();
    double latitude = fromRadianToDegree(phi);
    double longitude = fromRadianToDegree(lambda);
    return Coordinate{longitude, latitude};
}

Coordinate GeoSpheroid::fromGeogCSToWebMercator(const Coordinate &p)
{
    double latitude = getMercatorLatitude(p.latitude);
    double longitude = getNormalLongitude(p.longitude);

    double phi = fromDegreeToRadian(latitude);
    double lamda = fromDegreeToRadian(longitude);
    double x = getRadius() * lamda;
    double y = getRadius() * std::log(std::tan(constant::PI / 4 + phi / 2.0));
    return Coordinate{x, y, 0, 0};
}

double GeoSpheroid::getBearing(const Coordinate &origin, const Coordinate &destin)
{
    double phi0 = fromDegreeToRadian(origin.latitude);
    double phi1 = fromDegreeToRadian(destin.latitude);
    double lambda = fromDegreeToRadian(destin.longitude - origin.longitude);
    double y = std::sin(lambda) * std::cos(phi1);
    double x = std::cos(phi0) * std::sin(phi1) - std::sin(phi0) * std::cos(phi1) * std::cos(lambda);
    double bearing = std::fmod(std::fmod(fromRadianToDegree(std::atan2(y, x)), 360.0) + 360.0, 360.0); // [0, 360)
    return bearing;
}

double GeoSpheroid::getFinalBearing(const Coordinate &origin, const Coordinate &destin)
{
    return std::fmod((getBearing(destin, origin) + 180.0), 360.0);
}

double GeoSpheroid::fromHeadingToBearing(double heading)
{
    // (-∞, +∞) => [0, 360)
    return heading <= 0 ? 360 - std::fmod(-heading, 360.0) : std::fmod(heading, 360.0);
}

double GeoSpheroid::getIncludedAngle(const Coordinate &center, const Coordinate &p1, const Coordinate &p2)
{
    double cp1Bearing = getBearing(center, p1);
    double cp2Bearing = getBearing(center, p2);
    double includeAng = std::abs(cp2Bearing - cp1Bearing);
    return includeAng > 180 ? 360.0 - includeAng : includeAng;
}

int GeoSpheroid::checkClockwiseSign(const Coordinate &origin, const Coordinate &destin, const Coordinate &Target)
{
    // double bearing01 = getBearing(origin, destin);
    // double bearing12 = getBearing(destin, target);
    // double ddBearing = std::fmod(bearing12 - bearing01 + 720, 360);
    // if (abs(ddBearing) < 1e-5) return 0;
    // if (abs(ddBearing - 180) < 1e-8) return -0;
    // if (ddBearing > 0 && ddBearing < 180) return 1;  // turn right, or clockwise
    // else return -1; // turn left, or anticlockwise

    double x1 = destin.latitude - origin.latitude;
    double y1 = destin.longitude - origin.longitude;
    double x2 = Target.latitude - destin.latitude;
    double y2 = Target.longitude - destin.longitude;
    double cross = x1 * y2 - x2 * y1;
    // 1 turn right, or clockwise
    // 0 keep forward
    // -1 turn left, or anticlockwise
    if (std::abs(cross) < 1e-12)
    {
        return 0;
    }
    return (cross > 0) - (cross < 0);
}

Segment GeoSpheroid::rotate(const Coordinate &center, const Segment &destin, double angle, double heightUp)
{
    Coordinate enter = rotate(center, destin.first, angle, heightUp);
    Coordinate leave = rotate(center, destin.second, angle, heightUp);
    Segment result = Segment(enter, leave);
    return result;
}

Coordinate GeoSpheroid::rotate(const Coordinate &center, const Coordinate &destin, double angle, double heightUp)
{
    double distance01 = getDistance(center, destin);
    double bearing0 = getBearing(center, destin);
    double bearing1 = bearing0 + angle;
    Coordinate result = getDestination(center, distance01, bearing1);
    result.altitude = destin.altitude + heightUp;
    return result;
}

Coordinate GeoSpheroid::getMidposition(const Coordinate &origin, const Coordinate &destin)
{
    double phi0 = fromDegreeToRadian(origin.latitude);
    double lambda0 = fromDegreeToRadian(origin.longitude);
    double phi1 = fromDegreeToRadian(destin.latitude);
    double lambda = fromDegreeToRadian(destin.longitude - origin.longitude);

    double bx = std::cos(phi0) + std::cos(phi1) * std::cos(lambda);
    double by = std::cos(phi1) * std::sin(lambda);

    double x = std::sqrt(bx * bx + by * by);
    double y = std::sin(phi0) + std::sin(phi1);

    double phl2 = std::atan2(y, x);
    double lambda2 = lambda0 + std::atan2(by, bx);

    // normalise to −180..+180°
    return Coordinate{getNormalLongitude(fromRadianToDegree(lambda2)), getNormalLatitude(fromRadianToDegree(phl2))};
}

Coordinate GeoSpheroid::getIntersection(const Coordinate &p1, double bearing1, const Coordinate &p2, double bearing2)
{
    double phi1 = fromDegreeToRadian(p1.latitude);
    double lambda1 = fromDegreeToRadian(p1.longitude);
    double phi2 = fromDegreeToRadian(p2.latitude);
    double lambda2 = fromDegreeToRadian(p2.longitude);
    double theta13 = fromDegreeToRadian(bearing1);
    double theta23 = fromDegreeToRadian(bearing2);
    double dPhi = phi2 - phi1;
    double dLambda = lambda2 - lambda1;

    // angular distance p1-p2
    double delta12 =
        2 * std::asin(std::sqrt(std::sin(dPhi / 2.0) * std::sin(dPhi / 2.0) +
                                std::cos(phi1) * std::cos(phi2) * std::sin(dLambda / 2.0) * std::sin(dLambda / 2.0)));

    if (std::abs(delta12) < 1e-5)
    {
        return p1; // p1 == p2, do not return (qNaN, qNaN);
    }

    // initial/final bearings between points
    double aTheta =
        std::acos((std::sin(phi2) - std::sin(phi1) * std::cos(delta12)) / (std::sin(delta12) * std::cos(phi1)));
    if (std::isnan(aTheta))
    {
        aTheta = 0; // protect against rounding
    }
    double bTheta =
        std::acos((std::sin(phi1) - std::sin(phi2) * std::cos(delta12)) / (std::sin(delta12) * std::cos(phi2)));
    if (std::isnan(bTheta))
    {
        bTheta = 0; // protect against rounding
    }

    double theta12 = std::sin(dLambda) > 0 ? aTheta : 2 * constant::PI - aTheta;
    double theta21 = std::sin(dLambda) > 0 ? 2 * constant::PI - bTheta : bTheta;

    double alpha1 = theta13 - theta12; // angle 2-1-3
    double alpha2 = theta21 - theta23; // angle 1-2-3

    // do not return (qNaN, qNaN) - infinite intersections
    if (abs(std::sin(alpha1)) < 1e-5 && abs(std::sin(alpha2)))
    {
        return getMidposition(p1, p2);
    }

    // alpha2 += constant::PI; // return (qNaN, qNaN); - ambiguous intersection
    if (std::sin(alpha1) * std::sin(alpha2) < 0)
    {
        return Coordinate{constant::DBL_NAN, constant::DBL_NAN, 0, 0};
    }

    double alpha3 =
        std::acos(-std::cos(alpha1) * std::cos(alpha2) + std::sin(alpha1) * std::sin(alpha2) * std::cos(delta12));
    double delta13 = std::atan2(std::sin(delta12) * std::sin(alpha1) * std::sin(alpha2),
                                std::cos(alpha2) + std::cos(alpha1) * std::cos(alpha3));
    double phi3 =
        std::asin(std::sin(phi1) * std::cos(delta13) + std::cos(phi1) * std::sin(delta13) * std::cos(theta13));
    double dLambda13 = std::atan2(std::sin(theta13) * std::sin(delta13) * std::cos(phi1),
                                  std::cos(delta13) - std::sin(phi1) * std::sin(phi3));
    double lambda3 = lambda1 + dLambda13;

    // normalise to −180..+180°
    return Coordinate{getNormalLongitude(fromRadianToDegree(lambda3)), getNormalLatitude(fromRadianToDegree(phi3))};
}

Coordinate GeoSpheroid::getIntersection(const Segment &segment1, const Segment &segment2)
{
    Coordinate c1 = segment1.first, c2 = segment1.second;
    Coordinate c3 = segment2.first, c4 = segment2.second;

    double abcArea = (c1.longitude - c3.longitude) * (c2.latitude - c3.latitude) -
                     (c1.latitude - c3.latitude) * (c2.longitude - c3.longitude);
    double abdArea = (c1.longitude - c4.longitude) * (c2.latitude - c4.latitude) -
                     (c1.latitude - c4.latitude) * (c2.longitude - c4.longitude);

    if (util::DBL_EQUAL(abcArea * abdArea, 0.0) || util::DBL_EQUAL(abcArea * abdArea, 0.0))
    {
        return Coordinate();
    }

    double cdaArea = (c3.longitude - c1.longitude) * (c4.latitude - c1.latitude) -
                     (c3.latitude - c1.latitude) * (c4.longitude - c1.longitude);
    double cdbArea = abcArea + abdArea - cdaArea;

    if (util::DBL_EQUAL(cdaArea * cdbArea, 0.0) || util::DBL_EQUAL(cdaArea * cdbArea, 0.0))
    {
        return Coordinate();
    }

    double t = cdaArea / (abdArea - abcArea);

    double dx = t * (c2.longitude - c1.longitude);
    double dy = t * (c2.latitude - c1.latitude);
    double x = c1.longitude + dx;
    double y = c1.latitude + dy;

    if ((x - c1.longitude) * (x - c2.longitude) <= 0 && (y - c1.latitude) * (y - c2.latitude) <= 0 &&
        (x - c3.longitude) * (x - c4.longitude) <= 0 && (y - c3.latitude) * (y - c4.latitude) <= 0)
    {
        return Coordinate{x, y};
    }
    else
    {
        return Coordinate();
    }
}

Coordinate GeoSpheroid::getDestination(const Coordinate &origin, double distance, double bearing, double heightUp)
{
    double latitude = getNormalLatitude(origin.latitude);
    double longitude = getNormalLongitude(origin.longitude);

    double theta01 = fromDegreeToRadian(bearing);
    double phi0 = fromDegreeToRadian(latitude);
    double lambda0 = fromDegreeToRadian(longitude);

    double angularDistance = distance / getRadius();
    double sinPhi1 =
        std::sin(phi0) * std::cos(angularDistance) + std::cos(phi0) * std::sin(angularDistance) * std::cos(theta01);
    double phi1 = std::asin(sinPhi1);
    double lambda1 = lambda0 + std::atan2(std::sin(theta01) * std::sin(angularDistance) * std::cos(phi0),
                                          std::cos(angularDistance) - std::sin(phi0) * sinPhi1);

    double latitude1 = fromRadianToDegree(phi1);
    double longitude1 = fromRadianToDegree(lambda1);

    return Coordinate{longitude1, latitude1, origin.altitude + heightUp};
}

double GeoSpheroid::getDistance(const Coordinate &origin, const Coordinate &destin)
{
    double latitude1 = getNormalLatitude(origin.latitude);
    double latitude2 = getNormalLatitude(destin.latitude);
    double longitude1 = getNormalLongitude(origin.longitude);
    double longitude2 = getNormalLongitude(destin.longitude);

    // if closest:
    if ((std::abs(latitude1 - latitude2) < 1e-12) && (std::abs(longitude1 - longitude2) < 1e-12))
    {
        return 0.0;
    }

    double phi1 = fromDegreeToRadian(latitude1);
    double phi2 = fromDegreeToRadian(latitude2);
    double lambda1 = fromDegreeToRadian(longitude1);
    double lambda2 = fromDegreeToRadian(longitude2);
    double dPhi = phi2 - phi1;
    double dLambda = lambda2 - lambda1;

    double h1 =
        std::pow(std::sin(dPhi / 2.0), 2) + std::cos(phi1) * std::cos(phi2) * std::pow(std::sin(dLambda / 2.0), 2);

    return 2 * getRadius() * std::atan2(std::sqrt(h1), std::sqrt(1 - h1));
}

double GeoSpheroid::getCrossTrackDistance(const Coordinate &origin, const Coordinate &destin, const Coordinate &Target)
{
    double delta13 = getDistance(origin, Target) / getRadius();
    double theta13 = fromDegreeToRadian(getBearing(origin, Target));
    double theta12 = fromDegreeToRadian(getBearing(origin, destin));

    double xtDelta = std::asin(std::sin(delta13) * std::sin(theta13 - theta12));

    return xtDelta * getRadius();
}

double GeoSpheroid::getAlongTrackDistance(const Coordinate &origin, const Coordinate &destin, const Coordinate &Target)
{
    double delta13 = getDistance(origin, Target) / getRadius();
    double theta13 = fromDegreeToRadian(getBearing(origin, Target));
    double theta12 = fromDegreeToRadian(getBearing(origin, destin));

    double xtDelta = std::asin(std::sin(delta13) * std::sin(theta13 - theta12));
    double atDelta = std::acos(std::cos(delta13) / std::abs(std::cos(xtDelta)));

    if (std::cos(theta12 - theta13) < 0)
    {
        return atDelta * -1 * getRadius();
    }
    else if (std::cos(theta12 - theta13) == 0)
    {
        return 0;
    }
    else
    {
        return atDelta * getRadius();
    }
}

Coordinate GeoSpheroid::getRhumbDestination(const Coordinate &origin, double distance, double bearing)
{
    double delta = distance / getRadius(); // angular distance in radians
    double phi1 = fromDegreeToRadian(origin.latitude);
    double lambda1 = fromDegreeToRadian(origin.longitude);
    double theta = fromDegreeToRadian(bearing);

    double dphi = delta * std::cos(theta);
    double phi2 = phi1 + dphi;

    // check for some daft bugger going past the pole, normalise y if so
    if (abs(phi2) > constant::PI / 2.0)
    {
        phi2 = phi2 > 0 ? constant::PI - phi2 : -constant::PI - phi2;
    }

    double dpsi = std::log(std::tan(phi2 / 2.0 + constant::PI / 4.0) / std::tan(phi1 / 2.0 + constant::PI / 4.0));
    // E-W course becomes ill-conditioned with 0/0
    double q = abs(dpsi) > 10e-12 ? dphi / dpsi : std::cos(phi1);
    double dLambda = delta * std::sin(theta) / q;
    double lambda2 = lambda1 + dLambda;

    // normalise to −180..+180°
    return Coordinate{getNormalLongitude(fromRadianToDegree(lambda2)), getNormalLatitude(fromRadianToDegree(phi2))};
}

double GeoSpheroid::getRhumbDistance(const Coordinate &origin, const Coordinate &destin)
{
    double phi1 = fromDegreeToRadian(origin.latitude);
    double phi2 = fromDegreeToRadian(destin.latitude);
    double dphi = phi2 - phi1;
    double dLambda = fromDegreeToRadian(abs(destin.longitude - origin.longitude));
    // if dLon over 180° take shorter rhumb line across the anti-meridian:
    if (dLambda > constant::PI)
    {
        dLambda -= 2 * constant::PI;
    }

    // on Mercator projection, x distances shrink by y; q is the 'stretch factor'
    // q becomes ill-conditioned along E-W line (0/0); use empirical tolerance to avoid it
    double dpsi = std::log(std::tan(phi2 / 2.0 + constant::PI / 4.0) / std::tan(phi1 / 2.0 + constant::PI / 4.0));
    double q = abs(dpsi) > 10e-12 ? dphi / dpsi : std::cos(phi1);

    // distance is pythagoras on 'stretched' Mercator projection
    double delta = std::sqrt(dphi * dphi + q * q * dLambda * dLambda); // angular distance in radians
    return delta * getRadius();
}

double GeoSpheroid::getRhumbBearing(const Coordinate &origin, const Coordinate &destin)
{
    double phi1 = fromDegreeToRadian(origin.latitude);
    double phi2 = fromDegreeToRadian(destin.latitude);
    double dLambda = fromDegreeToRadian(destin.longitude - origin.longitude);

    // if dLon over 180° take shorter rhumb line across the anti-meridian:
    if (dLambda > constant::PI)
    {
        dLambda -= 2 * constant::PI;
    }
    if (dLambda < -constant::PI)
    {
        dLambda += 2 * constant::PI;
    }

    double dpsi = std::log(std::tan(phi2 / 2.0 + constant::PI / 4.0) / std::tan(phi1 / 2.0 + constant::PI / 4.0));
    double theta = std::atan2(dLambda, dpsi);

    return std::fmod(std::fmod(fromRadianToDegree(theta), 360.0) + 360.0, 360.0);
}

Coordinate GeoSpheroid::getRhumbMidposition(const Coordinate &origin, const Coordinate &destin)
{
    double phi1 = fromDegreeToRadian(origin.latitude);
    double lambda1 = fromDegreeToRadian(origin.longitude);
    double phi2 = fromDegreeToRadian(destin.latitude);
    double lambda2 = fromDegreeToRadian(destin.longitude);

    if (abs(lambda2 - lambda1) > constant::PI)
    {
        lambda1 += 2 * constant::PI; // crossing anti-meridian
    }

    double phi3 = (phi1 + phi2) / 2.0;
    double f1 = std::tan(constant::PI / 4.0 + phi1 / 2.0);
    double f2 = std::tan(constant::PI / 4.0 + phi2 / 2.0);
    double f3 = std::tan(constant::PI / 4.0 + phi3 / 2.0);
    double lambda3 =
        ((lambda2 - lambda1) * std::log(f3) + lambda1 * std::log(f2) - lambda2 * std::log(f1)) / std::log(f2 / f1);

    if (!std::isfinite(lambda3))
    {
        lambda3 = (lambda1 + lambda2) / 2.0; // parallel of y
    }

    // normalise to −180..+180°
    return Coordinate{getNormalLongitude(fromRadianToDegree(lambda3)), getNormalLatitude(fromRadianToDegree(phi3))};
}

double GeoSpheroid::getSurfacePolygonArea(const Coordinates &polygon)
{
    if (polygon.size() < 3)
    {
        return 0;
    }

    double R = getRadius();

    // close polygon so that last point equals first point
    Coordinate gp0 = polygon.at(0);
    Coordinate gpe = polygon.at(polygon.size() - 1);
    bool closed = util::DBL_EQUAL(gp0.longitude, gpe.longitude) && util::DBL_EQUAL(gp0.latitude, gpe.latitude) &&
                  util::DBL_EQUAL(gp0.altitude, gpe.altitude);
    size_t size = closed ? (polygon.size() - 1) : polygon.size();
    double S = 0; // spherical excess in steradians
    for (int i = 0; i < size; ++i)
    {
        Coordinate currentCoor = polygon.at(i);
        Coordinate nextCoor = (i == (int)polygon.size() - 1) ? polygon.at(0) : polygon.at(i + 1);
        double phi1 = fromDegreeToRadian(currentCoor.latitude);
        double phi2 = fromDegreeToRadian(nextCoor.latitude);
        double deltaLambda = fromDegreeToRadian(nextCoor.longitude - currentCoor.longitude);
        double E = 2 * std::atan2(std::tan(deltaLambda / 2.0) * (std::tan(phi1 / 2.0) + std::tan(phi2 / 2.0)),
                                  1 + std::tan(phi1 / 2.0) * std::tan(phi2 / 2.0));
        S += E;
    }

    if (isPoleEnclosedBy(polygon))
    {
        S = abs(S) - 2 * constant::PI;
    }
    double A = abs(S * R * R); // area in units of R

    return A;
}

bool GeoSpheroid::isPoleEnclosedBy(const Coordinates &polygon)
{
    if (polygon.size() < 3)
    {
        return false;
    }

    double sigmaDelta = 0;

    // close polygon so that last point equals first point
    Coordinate gp0 = polygon.at(0);
    Coordinate gpe = polygon.at(polygon.size() - 1);
    bool closed = util::DBL_EQUAL(gp0.longitude, gpe.longitude) && util::DBL_EQUAL(gp0.latitude, gpe.latitude) &&
                  util::DBL_EQUAL(gp0.altitude, gpe.altitude);
    auto size = closed ? (polygon.size() - 1) : polygon.size();

    double prevBrng = getBearing(polygon.at(0), polygon.at(1));
    for (int i = 0; i < size; ++i)
    {
        Coordinate currentCoor = polygon.at(i);
        Coordinate nextCoor = (i == (int)polygon.size() - 1) ? polygon.at(0) : polygon.at(i + 1);
        double initBrngij = getBearing(currentCoor, nextCoor);
        double finalBrngij = getFinalBearing(currentCoor, nextCoor);
        sigmaDelta += getNormalLongitude(initBrngij - prevBrng);
        sigmaDelta += getNormalLongitude(finalBrngij - initBrngij);
        prevBrng = finalBrngij;
    }

    double initBrng01 = getBearing(polygon.at(0), polygon.at(1));
    sigmaDelta += getNormalLongitude(initBrng01 - prevBrng);
    // TODO: fix edge crossing pole - eg (85, 90), (85, 0), (85, -90)
    bool enclosed = abs(sigmaDelta) < 90; // 0° - ish

    return enclosed;
}

bool GeoSpheroid::calculateRotationAngles(
    const Coordinate &shootPoint, const Coordinate &targetPoint, double *roll, double *pitch, double *heading)
{
    Coordinate shootPoint_UTM = fromGeogCSToWebMercator(shootPoint);
    Coordinate targetPoint_UTM = fromGeogCSToWebMercator(targetPoint);

    double dx = targetPoint_UTM.longitude - shootPoint_UTM.longitude;
    double dy = targetPoint_UTM.latitude - shootPoint_UTM.latitude;
    double dz = shootPoint.altitude - targetPoint.altitude;

    // 计算Heading
    if (dy > 0)
    {
        if (dx > 0)
        {
            *heading = fromRadianToDegree(std::atan(dx / dy));
        }
        else if (dx < 0)
        {
            *heading = -1 * fromRadianToDegree(std::atan(std::abs(dx) / dy));
        }
        else
        {
            *heading = 0;
        }
    }
    else if (dy < 0)
    {
        if (dx > 0)
        {
            *heading = fromRadianToDegree(std::atan(std::abs(dy) / dx)) + 90;
        }
        else if (dx < 0)
        {
            *heading = -1 * (fromRadianToDegree(std::atan(std::abs(dy) / std::abs(dx))) + 90);
        }
        else
        {
            *heading = 180;
        }
    }
    else
    {
        if (dx > 0)
        {
            *heading = 90;
        }
        else if (dx < 0)
        {
            *heading = -90;
        }
        else
        {
            *heading = 0;
        }
    }

    // 计算Pitch
    double ds = std::sqrt(dx * dx + dy * dy);
    if (dz > 0)
    {
        *pitch = fromRadianToDegree(std::atan(dz / ds));
    }
    else if (dz < 0)
    {
        *pitch = -1 * fromRadianToDegree(std::atan(std::abs(dz) / ds));
    }
    else
    {
        *pitch = 0;
    }

    // 计算roll
    *roll = 0;

    return true;
}