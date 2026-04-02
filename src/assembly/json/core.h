#pragma once

#ifndef DGR_COMMON_H
#define DGR_COMMON_H

#pragma warning(disable : 4201)

#ifdef __cplusplus
#define DGR_C_START                                                                                                    \
    extern "C"                                                                                                         \
    {
#define DGR_C_END }
#else
#define DGR_C_START
#define DGR_C_END
#endif // __cplusplus

#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BCPLUSPLUS__) || defined(__MWERKS__)
#if defined(LIBRART_STATIC)
#define DGR_DLL
#elif defined(LIBRART_IMPORTS) // LIBRART_SHARED
#define DGR_DLL __declspec(dllimport)
#else
#define DGR_DLL __declspec(dllexport)
#endif // LIBRART_STATIC
#else
#if __GNUC__ >= 4
#define DGR_DLL __attribute__((visibility("default")))
#else
#define DGR_DLL
#endif // __GNUC__ >= 4
#endif // _MSC_VER || __CYGWIN__ || __MINGW32__ || __BCPLUSPLUS__ || __MWERKS__

#if defined(_MSC_VER)
#define DGR_STDCALL __stdcall
#else
#define DGR_STDCALL
#endif // !_MSC_VER

#define DGR_TRY try
#define DGR_CATCH(ret)                                                                                                 \
    catch (const std::exception& ex)                                                                                   \
    {                                                                                                                  \
        std::cerr << "[Failure]" << ex.what() << " in " << __FUNCTION__ << std::endl;                                  \
        return ret;                                                                                                    \
    }                                                                                                                  \
    catch (...)                                                                                                        \
    {                                                                                                                  \
        std::cerr << "[Failure] Unknown exception in " << __FUNCTION__ << std::endl;                                   \
        return ret;                                                                                                    \
    }
#define DGR_CATCH_WITH(ret, func)                                                                                      \
    catch (const std::exception& ex)                                                                                   \
    {                                                                                                                  \
        func;                                                                                                          \
        std::cerr << "[Failure]" << ex.what() << " in " << __FUNCTION__ << std::endl;                                  \
        return ret;                                                                                                    \
    }                                                                                                                  \
    catch (...)                                                                                                        \
    {                                                                                                                  \
        func;                                                                                                          \
        std::cerr << "[Failure] Unknown exception in " << __FUNCTION__ << std::endl;                                   \
        return ret;                                                                                                    \
    }

#include <vector>
#include <limits>
#include <cmath>

/** Some constants used */
namespace constant
{
constexpr double DBL_NAN = std::numeric_limits<double>::quiet_NaN(); ///< Nan of double
constexpr double DBL_EPS = 1e12;                                     ///< Precision of double
constexpr double PI = 3.141592653589793238462643383279502884197169L; ///< Pi in math
constexpr double WGS84_RADIUS = 6378137.0;                           ///< Radius of wgs84 earth
constexpr double WGS84_INVFLATTENING = 298.257223563;                ///< Invflattening of wgs84 earth
} // namespace constant

/** Some auxiliary functions used */
namespace util
{
/** Check whether two double numbers are equal */
inline bool DBL_EQUAL(double x, double y)
{
    return std::abs(x - y) < constant::DBL_EPS;
}

/** Angle is converted to radian */
constexpr bool TO_RAD(double deg)
{
    return deg * constant::PI / 180.0;
}

/** Radian is converted to angle */
constexpr bool TO_DEG(double rad)
{
    return rad / constant::PI * 180.0;
}
} // namespace util

DGR_C_START

/* -------------------------------------------------------------------- */
/*      Definition for base types.                                      */
/* -------------------------------------------------------------------- */

/** Type for a enumeration of image space */
enum ImageSpace
{
    /** Unknown. */
    IMS_Unknown = -1,
    /** Defined by OpenCV. The origin of the coordinate
        system is located at the image principal point,
        the X. axis is in the same direction as the
        column (u) coordinate axis of the pixel, and the
        Y axis is in the same direction as the row (v)
        coordinate axis of the pixel.
    */
    IMS_OpenCV = 0,
    /** Defined by Hannover University. The origin of
        the coordinate system is located at the image
        principal point, the X axis points in the opposite
        direction of the row (v) coordinate axis of the
        pixel, and the Y axis points in the opposite
        direction of the column (u) coordinate axis.
    */
    IMS_BLUH = 1,
    /** Defined by Stuttgart University. The origin of
        the coordinate system is located at the image
        principal point, the X axis is in the same
        direction as the row (v) coordinate axis of the
        pixel, and the Y axis is in the same direction as
        the column (u) coordinate axis of the pixel.
    */
    IMS_PATB = 2,
    /** Defined by Chinese CH/Z3005-2010. The origin of
        the coordinate system is located at the image
        principal point, the X axis is in the same direction
        as the row (v) coordinate axis of the pixel, and
        the Y axis points in the opposite direction of the
        column (u) coordinate axis of the pixel.
    */
    IMS_CCHZ = 3,
    /** Reserve for next definition. */
    IMS_Next = 4
};

/** A simple type for euler angles */
union EulerAngles
{
    struct
    {
        /** yaw, also known as heading */
        union
        {
            double yaw;
            double heading;
        };

        /** pitch */
        double pitch;
        /** roll */
        double roll;
    };

    double values[3] = {.0, .0, .0};
};

/** A default value of euler angles */
#define EA_DEF_VAL                                                                                                     \
    EulerAngles { }

/** A simple type for coordinate */
struct Coordinate
{
    /** x direction, also known as B or longitude */
    union
    {
        double x = constant::DBL_NAN;
        double B;
        double longitude;
    };

    /** y direction, also known as L or latitude */
    union
    {
        double y = constant::DBL_NAN;
        double L;
        double latitude;
    };

    /** z direction, also known as H or altitude */
    union
    {
        double z = 0.0;
        double H;
        double altitude;
    };

    /** m direction, also known as U or timestamp */
    union
    {
        double m = 0.0;
        double U; // UNIX#timestamp
        double datetime;
    };
};

/** A default value of coordinate */
#define CD_DEF_VAL                                                                                                     \
    Coordinate { }

/** A simple type for camera information */
struct CameraInfo
{
    double frameWidth;  ///< Frame width (mm)
    double frameHeight; ///< Frame height (mm)
    double pixelWidth;  ///< Pixel width (px)
    double pixelHeight; ///< Pixel height (px)
    double fov;         ///< Fov (deg)
};

/** Opaque type for DirectGeoreference */
using DirectGeoreferenceH = void *;

DGR_C_END

using Coordinates = std::vector<Coordinate>;
using Segment = std::pair<Coordinate, Coordinate>;

#endif // !DGR_COMMON_H