#pragma once

#ifndef DGR_GEO_REFERENCE_H
#define DGR_GEO_REFERENCE_H

#pragma warning(disable : 4251)

#include <core.h>

#include <memory>

class DirectGeoreferenceData;

class DGR_DLL DirectGeoreference final
{
public:
    DirectGeoreference();
    explicit DirectGeoreference(ImageSpace imageSpace);
    /*!
     * @brief Construction
     * @param imageSpace
     * @param loadAngles is a three-dimensional vector, delta,theta and psi
     * @param bodyAngles is a three-dimensional vector, roll, pitch and yaw
     */
    DirectGeoreference(ImageSpace imageSpace, const EulerAngles& loadAngles, const EulerAngles& bodyAngles);
    ~DirectGeoreference() = default;

    ImageSpace getImageSpace() const;
    void setImageSpace(ImageSpace v);
    EulerAngles getLoadAngles() const;
    void setLoadAngles(const EulerAngles& v);
    EulerAngles getBodyAngles() const;
    void setBodyAngles(const EulerAngles& v);
    double getOpticAxisTiltToVertical() const;
    EulerAngles getImageKappaPhiOmega() const;
    Coordinates getImageControls(double w, double h, double f); // In meters
    Coordinates TransformGEOToNED(const Coordinate& op, const Coordinates& gps);
    Coordinates TransformNEDToGEO(const Coordinate& op, const Coordinates& nps);
    Coordinates RotateImageToNED(const Coordinates& ips);
    Coordinates RotateImageToENU(const Coordinates& ips);
    Coordinates RotateImageToGEO(const Coordinate& ep, const Coordinates& ips);
    Coordinates ProjectImageToNED(const Coordinate& ep, const Coordinates& ips, double ph);           // In meters
    Coordinates ProjectImageToENU(const Coordinate& ep, const Coordinates& ips, double ph);           // In meters
    Coordinates ProjectImageToGEO(const Coordinate& ep, const Coordinates& ips, double ph);           // In meters
    Coordinates ReflectNEDToImage(const Coordinate& ep, const Coordinates& nps, double f, double ph); // In meters
    Coordinates ReflectENUToImage(const Coordinate& ep, const Coordinates& tps, double f, double ph); // In meters
    Coordinates ReflectGEOToImage(const Coordinate& ep, const Coordinates& gps, double f, double ph); // In meters

private:
    std::shared_ptr<DirectGeoreferenceData> _dp;
};

#endif // !DGR_GEO_REFERENCE_H