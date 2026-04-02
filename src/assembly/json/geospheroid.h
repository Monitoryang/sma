#pragma once

#ifndef DGR_GEO_SPHEROID_H
#define DGR_GEO_SPHEROID_H

#include <core.h>

class DGR_DLL GeoSpheroid final
{
public:
    GeoSpheroid();
    explicit GeoSpheroid(double radius);
    ~GeoSpheroid() = default;

    double getRadius() const;
    void setRadius(double radius);
    double wrap360(double degree);
    double getNormalLatitude(double latitude);
    double getNormalLongitude(double longitude);
    double getMercatorLatitude(double latitude);
    bool checkNormalLatitude(double latitude);
    bool checkNormalLongitude(double longitude);
    bool checkMercatorLatitude(double latitude);
    bool checkCoordinate(const Coordinate& p);
    bool checkSegment(const Segment& Segment);
    double fromMileToDegree(double mile);
    double fromDegreeToMile(double degree);
    double fromDegreeToRadian(double degree);
    double fromRadianToDegree(double radian);
    Coordinate fromWebMercatorToGeogCS(const Coordinate& p);
    Coordinate fromGeogCSToWebMercator(const Coordinate& p);
    double getBearing(const Coordinate& origin, const Coordinate& destin); // [0, 360)
    double getFinalBearing(const Coordinate& origin, const Coordinate& destin);
    double fromHeadingToBearing(double heading); // (-∞, +∞) => [0, 360)
    double getIncludedAngle(const Coordinate& center, const Coordinate& p1,
                            const Coordinate& p2); // [0, 180)
    int checkClockwiseSign(const Coordinate& origin,
                           const Coordinate& destin,
                           const Coordinate& Target); // left < 0, right > 0
    Segment rotate(const Coordinate& center, const Segment& destin, double angle, double heightUp = 0.0);
    Coordinate rotate(const Coordinate& center, const Coordinate& destin, double angle, double heightUp = 0.0);
    Coordinate getMidposition(const Coordinate& origin, const Coordinate& destin);
    Coordinate getIntersection(const Coordinate& p1, double bearing1, const Coordinate& p2, double bearing2);
    Coordinate getIntersection(const Segment& segment1, const Segment& segment2);
    Coordinate getDestination(const Coordinate& origin, double distance, double bearing, double heightUp = 0.0);
    double getDistance(const Coordinate& origin, const Coordinate& destin);
    double getCrossTrackDistance(const Coordinate& origin, const Coordinate& destin, const Coordinate& Target);
    double getAlongTrackDistance(const Coordinate& origin, const Coordinate& destin, const Coordinate& Target);
    Coordinate getRhumbDestination(const Coordinate& origin, double distance, double bearing);
    double getRhumbDistance(const Coordinate& origin, const Coordinate& destin);
    double getRhumbBearing(const Coordinate& origin, const Coordinate& destin);
    Coordinate getRhumbMidposition(const Coordinate& origin, const Coordinate& destin);
    double getSurfacePolygonArea(const Coordinates& polygon);
    bool isPoleEnclosedBy(const Coordinates& polygon);
    bool calculateRotationAngles(
        const Coordinate& shootPoint, const Coordinate& targetPoint, double *roll, double *pitch, double *heading);

private:
    double _dfRadius;
};

#endif // !DGR_GEO_SPHEROID_H