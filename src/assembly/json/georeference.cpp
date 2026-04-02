#include <georeference.h>
#include <geospheroid.h>
#include <geoellipsoid.h>
#include <capi.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <iostream>
#include <vector>
#include <iomanip>
#include <float.h>

constexpr double RADIANS_PER_DEGREE = constant::PI / (180.0);
constexpr double DEGREES_PER_RADIAN = (180.0) / constant::PI;

using namespace Eigen;

static Matrix4d ImageSpace2MatrixcRi(ImageSpace ims)
{
    Matrix4d cRi;
    switch (ims)
    {
    case ImageSpace::IMS_OpenCV:
    {
        cRi << 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1;
    }
    break;
    case ImageSpace::IMS_BLUH:
    {
        cRi << 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1;
    }
    break;
    case ImageSpace::IMS_PATB:
    {
        cRi << 0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1;
    }
    break;
    case ImageSpace::IMS_CCHZ:
    {
        cRi << 0, 0, -1, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1;
    }
    break;
    }
    return cRi;
}

static ImageSpace MatrixcRi2ImageSpace(const Matrix4d &cRi)
{
    if (cRi == ImageSpace2MatrixcRi(ImageSpace::IMS_OpenCV))
    {
        return ImageSpace::IMS_OpenCV;
    }
    else if (cRi == ImageSpace2MatrixcRi(ImageSpace::IMS_BLUH))
    {
        return ImageSpace::IMS_BLUH;
    }
    else if (cRi == ImageSpace2MatrixcRi(ImageSpace::IMS_PATB))
    {
        return ImageSpace::IMS_PATB;
    }
    else if (cRi == ImageSpace2MatrixcRi(ImageSpace::IMS_CCHZ))
    {
        return ImageSpace::IMS_CCHZ;
    }

    return ImageSpace::IMS_OpenCV;
}

static Matrix4d GetRotationZYXM(const EulerAngles &eas)
{
    Matrix4d I = Matrix4d::Identity();
    Matrix3d R;
    R = AngleAxisd(eas.yaw, Vector3d::UnitZ()) * AngleAxisd(eas.pitch, Vector3d::UnitY()) *
        AngleAxisd(eas.roll, Vector3d::UnitX());
    I.topLeftCorner(3, 3) = R;
    I(3, 3) = 0;
    return I;
}

static Matrix4d NED2ENU()
{
    Matrix4d tRn;
    tRn << 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1;
    return tRn;
}

static Matrix4Xd GEO2NED(const Vector4d &op, const GeoEllipsoid &ge, const Matrix4Xd &gps)
{
    double oLonRad = op.x() * RADIANS_PER_DEGREE; // LON(X)
    double oLatRad = op.y() * RADIANS_PER_DEGREE; // LAT(Y)
    double oHgt = op.z();

    double e2 = ge.getSquaredEccentricity();
    double slamd2 = pow(sin(oLatRad), 2.0);
    double den = pow(1.0 - e2 * slamd2, 1.5);

    double a = ge.getSemiMajorAxis();
    double RLAT = a * (1.0 - e2) / den + oHgt;
    double den1 = sqrt(1.0 - e2 * slamd2);
    double RLON = (a / den1 + oHgt) * cos(oLatRad);

    Matrix4Xd nps(4, gps.cols());

    nps.row(1) = (gps.row(0).array() * RADIANS_PER_DEGREE - oLonRad) * RLON;
    nps.row(0) = (gps.row(1).array() * RADIANS_PER_DEGREE - oLatRad) * RLAT;
    nps.row(2) = (oHgt - gps.row(2).array());
    nps.row(3) = gps.row(3);

    return nps;
}

static Matrix4Xd NED2GEO(const Vector4d &op, const GeoEllipsoid &ge, const Matrix4Xd &nps)
{
    double oLonRad = op.x() * RADIANS_PER_DEGREE; // LON(X)
    double oLatRad = op.y() * RADIANS_PER_DEGREE; // LAT(Y)
    double oHgt = op.z();

    double e2 = ge.getSquaredEccentricity();
    double slamd2 = pow(sin(oLatRad), 2.0);
    double den = pow(1.0 - e2 * slamd2, 1.5);

    double a = ge.getSemiMajorAxis();
    double RLAT = a * (1.0 - e2) / den + oHgt;
    double den1 = sqrt(1.0 - e2 * slamd2);
    double RLON = (a / den1 + oHgt) * cos(oLatRad);

    Matrix4Xd gps(4, nps.cols());

    gps.row(0) = (oLonRad + nps.row(1).array() / RLON) * DEGREES_PER_RADIAN;
    gps.row(1) = (oLatRad + nps.row(0).array() / RLAT) * DEGREES_PER_RADIAN;
    gps.row(2) = (oHgt - nps.row(2).array());
    gps.row(3) = nps.row(3);

    /*double Xg = gps.col(0).x();
    double Yg = gps.col(0).y();
    double Zg = gps.col(0).z();*/

    return gps;
}

static Matrix4Xd Flatten(const Vector4d &ep, const Matrix4Xd &nps, double ph)
{
    double oZ = ep.z();
    double num = abs(ph - oZ);

    RowVectorXd rts = num / ((nps.row(2).array() - oZ).abs());
    Matrix4Xd mts(4, rts.cols());

    // std::cout << std::endl
    //	//<< std::fixed << std::setprecision(8)
    //	<< "rts = " << std::endl
    //	<< (rts.transpose())
    //	<< std::endl << std::endl;
    // mts << rts, rts, rts,rts;
    // Matrix4Xd m_fps(4, nps.size());
    mts << rts, rts, rts, rts;
    Matrix4Xd fps = (nps.array() * mts.array()).matrix().colwise() + ep;
    // std::cout << std::endl
    //	//<< std::fixed << std::setprecision(8)
    //	<< "fps = " << std::endl
    //	<< (fps.transpose())
    //	<< std::endl << std::endl;

    return fps;
}

/// <summary>
/// 利用共线方程根据物方坐标和主距计算像方坐标
/// </summary>
/// <param name="ep">摄影中心物方坐标</param>
/// <param name="oRi">像方到物方的旋转矩阵</param>
/// <param name="obj">目标点物方坐标</param>
/// <param name="f">摄影主距[m]：约等于焦距</param>
/// <returns>返回像方坐标</returns>
static Matrix4Xd OBJ2IMG(const Vector4d &ep, const Matrix4d &oRi, const Matrix4Xd &obj, double f)
{
    double a1 = oRi(0, 0), a2 = oRi(0, 1), a3 = oRi(0, 2);
    double b1 = oRi(1, 0), b2 = oRi(1, 1), b3 = oRi(1, 2);
    double c1 = oRi(2, 0), c2 = oRi(2, 1), c3 = oRi(2, 2);

    double XS = ep.x(), YS = ep.y(), ZS = ep.z();

    RowVectorXd XA = obj.row(0);
    RowVectorXd YA = obj.row(1);
    RowVectorXd ZA = obj.row(2);

    RowVectorXd Lambda = -f / (a3 * (XA.array() - XS) + b3 * (YA.array() - YS) + c3 * (ZA.array() - ZS));
    RowVectorXd XI = Lambda.array() * (a1 * (XA.array() - XS) + b1 * (YA.array() - YS) + c1 * (ZA.array() - ZS));
    RowVectorXd YI = Lambda.array() * (a2 * (XA.array() - XS) + b2 * (YA.array() - YS) + c2 * (ZA.array() - ZS));
    RowVectorXd ZI = RowVectorXd::Constant(obj.cols(), -f);
    RowVectorXd MI = obj.row(3);
    Matrix4Xd img(4, obj.cols());
    img << XI, YI, ZI, MI;

    return img;
}

static Vector4d GetIntersectionOfLineAndPlane(const Vector4d &sp1,
                                              const Vector4d &sp2,
                                              const Vector3d &nv,
                                              double d) // 获取平面（法线）与线的交点
{
    double a = nv(0), b = nv(1), c = nv(2);
    double x1 = sp1.x(), y1 = sp1.y(), z1 = sp1.z();
    double x2 = sp2.x(), y2 = sp2.y(), z2 = sp2.z();

    double num = abs(a * x1 + b * y1 + c * z1 + d);
    double den = abs(a * (x2 - x1) + b * (y2 - y1) + c * (z2 - z1));

    if (abs(den) < DBL_EPSILON)
    {
        return Vector4d(constant::DBL_NAN, constant::DBL_NAN, constant::DBL_NAN, 0);
    }
    else
    {
        double times = num / den;
        Vector3d op1(x1, y1, z1);
        Vector3d p1p2(x2 - x1, y2 - y1, z2 - z1);
        Vector3d op = op1 + (times * p1p2);
        return Vector4d(op.x(), op.y(), op.z(), 0.0);
    }
}

#pragma endregion

class DirectGeoreferenceData
{
public:
    DirectGeoreferenceData(ImageSpace imageSpace, const EulerAngles &loadAngles, const EulerAngles &bodyAngles)
        : cRi(ImageSpace2MatrixcRi(imageSpace)), bRc(GetRotationZYXM(loadAngles)), nRb(GetRotationZYXM(bodyAngles)), tRn(NED2ENU())
    {
        cleanUp();
    }

    inline Matrix4d getcRi() const
    {
        return cRi;
    }

    inline void setcRi(const Matrix4d &v)
    {
        cRi = v;
        cleanUp();
    }

    inline Matrix4d getbRc() const
    {
        return bRc;
    }

    inline void setbRc(const Matrix4d &v)
    {
        bRc = v;
        cleanUp();
    }

    inline Matrix4d getnRb() const
    {
        return nRb;
    }

    inline void setnRb(const Matrix4d &v)
    {
        nRb = v;
        cleanUp();
    }

    inline Matrix4d gettRn() const
    {
        return tRn;
    }

    inline Matrix4d getnRi() const
    {
        return nRi;
    }

    inline Matrix4d gettRi() const
    {
        return tRi;
    }

private:
    void cleanUp()
    {
        nRi = nRb * bRc * cRi;
        tRi = tRn * nRi;
    }

    Matrix4d cRi; // Rotation from image (photo) to load (camera).
    Matrix4d bRc; // Rotation from load (camera) to body (vehicle).
    Matrix4d nRb; // Rotation from body (vehicle) to NED (lcoal-navigation).
    Matrix4d tRn; // Rotation from NED (lcoal-navigation) to ENU (local-tangent-plane).
    Matrix4d nRi; // Rotation from image (photo) to NED (lcoal-navigation).
    Matrix4d tRi; // Rotation from image (photo) to ENU (local-tangent-plane).
};

DirectGeoreference::DirectGeoreference()
    : _dp(new DirectGeoreferenceData(ImageSpace::IMS_OpenCV, EulerAngles{0, 0, 0}, EulerAngles{0, 0, 0}))
{
}

DirectGeoreference::DirectGeoreference(ImageSpace imageSpace)
    : _dp(new DirectGeoreferenceData(imageSpace, EulerAngles{0, 0, 0}, EulerAngles{0, 0, 0}))
{
}

DirectGeoreference::DirectGeoreference(ImageSpace imageSpace,
                                       const EulerAngles &loadAngles,
                                       const EulerAngles &bodyAngles)
    : _dp(new DirectGeoreferenceData(imageSpace, loadAngles, bodyAngles))
{
}

ImageSpace DirectGeoreference::getImageSpace() const
{
    return MatrixcRi2ImageSpace(_dp->getcRi());
}

void DirectGeoreference::setImageSpace(ImageSpace v)
{
    _dp->setcRi(ImageSpace2MatrixcRi(v));
}

EulerAngles DirectGeoreference::getLoadAngles() const
{
    Matrix3d R(_dp->getbRc().topLeftCorner(3, 3));
    Vector3d eas = R.eulerAngles(2, 1, 0);
    return EulerAngles{eas(0), eas(1), eas(2)};
}

void DirectGeoreference::setLoadAngles(const EulerAngles &v)
{
    _dp->setbRc(GetRotationZYXM(v));
}

EulerAngles DirectGeoreference::getBodyAngles() const
{
    Matrix3d R(_dp->getnRb().topLeftCorner(3, 3));
    Vector3d eas = R.eulerAngles(2, 1, 0);
    return EulerAngles{eas(0), eas(1), eas(2)};
}

void DirectGeoreference::setBodyAngles(const EulerAngles &v)
{
    _dp->setnRb(GetRotationZYXM(v));
}

EulerAngles DirectGeoreference::getImageKappaPhiOmega() const
{
    Matrix4d cRi = _dp->getcRi();
    Matrix4d tRi = _dp->gettRi();
    switch (MatrixcRi2ImageSpace(cRi))
    {
    default:
    case ImageSpace::IMS_OpenCV:
    {
        // TODO: throw NotImplementedException();

        return EulerAngles();
    }
    case ImageSpace::IMS_BLUH:
    {
        double c31 = tRi(2, 0);
        double c33 = tRi(2, 2);
        double c32 = tRi(2, 1);
        double c12 = tRi(0, 1);
        double c22 = tRi(1, 1);

        double phiRad = atan2(c31, c33);
        double omgRad = atan2(-c32, sqrt(c12 * c12 + c22 * c22));
        double kppRad = atan2(c12, c22);

        return EulerAngles{kppRad, phiRad, omgRad}; // Z, Y, X
    }
    case ImageSpace::IMS_PATB:
    {
        double c13 = tRi(0, 2);
        double c23 = tRi(1, 2);
        double c33 = tRi(2, 2);
        double c12 = tRi(0, 1);
        double c11 = tRi(0, 0);

        double phiRad = atan2(c13, sqrt(c23 * c23 + c33 * c33));
        double omgRad = atan2(-c23, c33);
        double kppRad = atan2(-c12, c11);

        return EulerAngles{kppRad, phiRad, omgRad}; // Z, Y, X
    }
    case ImageSpace::IMS_CCHZ:
    {
        double a3 = tRi(0, 2); // -sinφcosω
        double c3 = tRi(2, 2); // cosφcosω
        double b1 = tRi(1, 0); // cosωsinκ
        double b2 = tRi(1, 1); // cosωcosκ
        double b3 = tRi(1, 2); // -sinω

        double phiRad = atan2(-a3, c3);
        double omgRad = atan2(-b3, sqrt(a3 * a3 + c3 * c3));
        double kppRad = atan2(b1, b2);

        return EulerAngles{kppRad, phiRad, omgRad}; // Z, Y, X
    }
    }
}

double DirectGeoreference::getOpticAxisTiltToVertical() const // 主光轴与垂直方向的倾斜角
{
    EulerAngles kpo = getImageKappaPhiOmega();
    double phiRad = kpo.yaw, omgRad = kpo.pitch;
    double tiltRad = acos(cos(phiRad) * cos(omgRad));

    return tiltRad;
}

/// <summary>
/// 获取像幅控制点坐标 0-4
/// </summary>
/// <remarks>
/// ————————————————→ u
/// |      width
/// |        4-------3
/// |height  |   0   |
/// |        1-------2
/// ↓
/// v
/// </remarks>
/// <param name="w">The image system definition.</param>
/// <param name="h">The image size in m.</param>
/// <param name="f">摄影主距[m]：约等于焦距</param>
/// <returns>返回[像主点，左上像角点，左下像角点，右下像角点，右上像角点]</returns>
Coordinates DirectGeoreference::getImageControls(double w, double h, double f)
{
    // 焦距近似等于主距，正确的主距计算方法：f = H * F / (H - F)
    Matrix4Xd fcs(4, 5);
    // f = f; // to meters.
    double m = w / 2.0, n = h / 2.0;

    switch (MatrixcRi2ImageSpace(_dp->getcRi()))
    {
    case ImageSpace::IMS_OpenCV:
    {
        fcs << Vector4d(0, 0, f, 0), // 0
            Vector4d(-m, n, f, 0),   // 1
            Vector4d(m, n, f, 0),    // 2
            Vector4d(m, -n, f, 0),   // 3
            Vector4d(-m, -n, f, 0);  // 4
    }
    break;
    case ImageSpace::IMS_BLUH:
    {
        fcs << Vector4d(0, 0, -f, 0), // 0
            Vector4d(-n, m, -f, 0),   // 1
            Vector4d(-n, -m, -f, 0),  // 2
            Vector4d(n, -m, -f, 0),   // 3
            Vector4d(n, m, -f, 0);    // 4
    }
    break;
    case ImageSpace::IMS_PATB:
    {
        fcs << Vector4d(0, 0, -f, 0), // 0
            Vector4d(n, -m, -f, 0),   // 1
            Vector4d(n, m, -f, 0),    // 2
            Vector4d(-n, m, -f, 0),   // 3
            Vector4d(-n, -m, -f, 0);  // 4
    }
    break;
    case ImageSpace::IMS_CCHZ:
    {
        fcs << Vector4d(0, 0, -f, 0), // 0
            Vector4d(-m, -n, -f, 0),  // 1
            Vector4d(m, -n, -f, 0),   // 2
            Vector4d(m, n, -f, 0),    // 3
            Vector4d(-m, n, -f, 0);   // 4
    }
    break;
    }

    Coordinates c_fcs(fcs.cols());
    memcpy(c_fcs.data(), fcs.data(), fcs.cols() * fcs.rows() * sizeof(double));

    return c_fcs;
}

/// <summary>
/// Transform points from geodetic system to local NED.
/// </summary>
/// <param name="op">The origin point of coordinate system.</param>
/// <param name="gps">The positions in geodetic system.</param>
/// <returns>Points in local NED.</returns>
Coordinates DirectGeoreference::TransformGEOToNED(const Coordinate &op, const Coordinates &gps)
{
    GeoEllipsoid ge = GeoEllipsoid(constant::WGS84_RADIUS, constant::WGS84_INVFLATTENING);

    Matrix4Xd m_gps(4, gps.size());
    memcpy(m_gps.data(), gps.data(), m_gps.cols() * m_gps.rows() * sizeof(double));

    Vector4d v_op(op.x, op.y, op.z, op.m);

    Matrix4Xd nps = GEO2NED(v_op, ge, m_gps);

    Coordinates c_nps(nps.cols());
    memcpy(c_nps.data(), nps.data(), nps.cols() * nps.rows() * sizeof(double));
    return c_nps;
}

/// <summary>
/// Transform points from local NED to geodetic system.
/// </summary>
/// <param name="op">The origin point of coordinate system.</param>
/// <param name="nps">The points in local NED.</param>
/// <returns>Positions in geodetic system.</returns>
Coordinates DirectGeoreference::TransformNEDToGEO(const Coordinate &op, const Coordinates &nps)
{
    GeoEllipsoid ge = GeoEllipsoid(constant::WGS84_RADIUS, constant::WGS84_INVFLATTENING);

    Matrix4Xd m_nps(4, nps.size());
    memcpy(m_nps.data(), nps.data(), m_nps.cols() * m_nps.rows() * sizeof(double));

    Vector4d v_op(op.x, op.y, op.z, op.m);

    Matrix4Xd gps = NED2GEO(v_op, ge, m_nps);

    Coordinates c_gps(gps.cols());
    memcpy(c_gps.data(), gps.data(), gps.cols() * gps.rows() * sizeof(double));

    return c_gps;
}

/// <summary>
/// Rotate points from image plane to local NED.
/// </summary>
/// <param name="ips">The points in image.</param>
/// <returns>The points in local NED.</returns>
Coordinates DirectGeoreference::RotateImageToNED(const Coordinates &ips)
{
    Matrix4Xd m_ips(4, ips.size());
    memcpy(m_ips.data(), ips.data(), m_ips.cols() * m_ips.rows() * sizeof(double));

    Matrix4Xd nRi(_dp->getnRi());

    Matrix4Xd nps(nRi * m_ips);

    Coordinates c_nps(nps.cols());
    memcpy(c_nps.data(), nps.data(), nps.cols() * nps.rows() * sizeof(double));

    return c_nps;
}

/// <summary>
/// Rotate points from image plane to local ENU.
/// </summary>
/// <param name="ips">The points in image plane.</param>
/// <returns>The points in local ENU.</returns>
Coordinates DirectGeoreference::RotateImageToENU(const Coordinates &ips)
{
    Matrix4Xd m_ips(4, ips.size());
    memcpy(m_ips.data(), ips.data(), m_ips.cols() * m_ips.rows() * sizeof(double));

    Matrix4Xd tRi = _dp->gettRi();
    Matrix4Xd tps(tRi * m_ips);

    Coordinates c_tps(tps.cols());
    memcpy(c_tps.data(), tps.data(), tps.cols() * tps.rows() * sizeof(double));

    return c_tps;
}

/// <summary>
/// Rotate points from image plane to geodetic system.
/// </summary>
/// <param name="ep">The exposure station is represented by a geodetic position.
/// This geodetic position is the origin of this local coordinate system.</param>
/// <param name="ips">The points in image plane.</param>
/// <returns>The positions in geodetic system.</returns>
Coordinates DirectGeoreference::RotateImageToGEO(const Coordinate &ep, const Coordinates &ips)
{
    Coordinates nps(RotateImageToNED(ips));
    Coordinates gps(TransformNEDToGEO(ep, nps));
    return gps;
}

/// <summary>
/// Project points from image plane to local NED by specifing height to altitude datum.
/// </summary>
/// <param name="ep">The exposure station is represented by a geodetic position.
/// This geodetic position is the origin of this local coordinate system.</param>
/// <param name="ph">The specified height to altitude datum.</param>
/// <param name="ips">The points in image plane.</param>
/// <returns>The points in local NED.</returns>
Coordinates DirectGeoreference::ProjectImageToNED(const Coordinate &ep, const Coordinates &ips, double ph)
{
    /*double Xeg = ep.x;
    double Yeg = ep.y;
    double Zeg = ep.z;*/

    double dHgt = ep.z - ph;

    /*double u = ips.at(0).x;
    double v = ips.at(0).y;
    double f = ips.at(0).z;*/

    Coordinates nps = RotateImageToNED(ips);

    Matrix4Xd m_nps(4, nps.size());
    memcpy(m_nps.data(), nps.data(), m_nps.cols() * m_nps.rows() * sizeof(double));

    Matrix4Xd pps(4, m_nps.cols());
    for (int i = 0; i < m_nps.cols(); i++)
    {
        if (m_nps(2, i) < 0)
        {
            Vector4d pp(constant::DBL_NAN, constant::DBL_NAN, constant::DBL_NAN, 0);
            pps.col(i) = pp;
        }
        else
        {
            Vector4d pp = dHgt * m_nps.col(i).array() / m_nps(2, i);
            pps.col(i) = pp;
        }
    }

    Coordinates c_pps(pps.cols());
    memcpy(c_pps.data(), pps.data(), pps.cols() * pps.rows() * sizeof(double));

    return c_pps;
}

/// <summary>
/// Project points from image plane to local ENU by specifing height to altitude datum.
/// </summary>
/// <param name="ep">The exposure station is represented by a geodetic position.
/// This geodetic position is the origin of this local coordinate system.</param>
/// <param name="ph">The specified height to altitude datum.</param>
/// <param name="ips">The points in image plane.</param>
/// <returns>The points in local ENU.</returns>
Coordinates DirectGeoreference::ProjectImageToENU(const Coordinate &ep, const Coordinates &ips, double ph)
{
    Coordinates nps = ProjectImageToNED(ep, ips, ph);

    Matrix4Xd m_nps(4, nps.size());
    memcpy(m_nps.data(), nps.data(), m_nps.cols() * m_nps.rows() * sizeof(double));

    Matrix4d tRn = _dp->gettRn();
    Matrix4Xd tps(tRn * m_nps);

    Coordinates c_tps(tps.cols());
    memcpy(c_tps.data(), tps.data(), tps.cols() * tps.rows() * sizeof(double));
    return c_tps;
}

/// <summary>
/// Project points from image plane to geodetic system by specifing height to altitude datum.
/// </summary>
/// <param name="ep">The exposure station is represented by a geodetic position.
/// This geodetic position is the origin of this local coordinate system.</param>
/// <param name="pht">The specified height to altitude datum.</param>
/// <param name="ips">The points in image plane.</param>
/// <returns>The positions in geodetic system.</returns>
Coordinates DirectGeoreference::ProjectImageToGEO(const Coordinate &ep, const Coordinates &ips, double ph)
{
    Coordinates nps(ProjectImageToNED(ep, ips, ph));
    Coordinates gps(TransformNEDToGEO(ep, nps));
    return gps;
}

/// <summary>
/// 利用共线方程根据物点局部导航坐标反算像空间坐标系的像点坐标
/// </summary>
/// <param name="ep">The exposure station is represented by a geodetic position.
/// This geodetic position is the origin of this local coordinate system.</param>
/// <param name="ph">指定椭球高的投影面[m]</param>
/// <param name="f">摄影主距[m]：约等于焦距</param>
/// <param name="nps">物点局部导航坐标</param>
/// <returns>返回像空间坐标系的像点坐标</returns>
Coordinates DirectGeoreference::ReflectNEDToImage(const Coordinate &ep, const Coordinates &nps, double f, double ph)
{
    /* -------------------------------------------------------------------- */
    /*      Project all points of geographic features to the horizontal     */
    /*      plane through the ground principal point.                       */
    /* -------------------------------------------------------------------- */
    Vector4d S{0, 0, 0, 0}; // exposure point (S) in NED.

    Matrix4Xd m_nps(4, nps.size());
    memcpy(m_nps.data(), nps.data(), m_nps.cols() * m_nps.rows() * sizeof(double));

    double dhgt = ep.z - ph;
    Matrix4Xd fps = Flatten(S, m_nps, dhgt); // flattened points in the object space.

    /* -------------------------------------------------------------------- */
    /*      Calculate the coordinates of the image principal point in       */
    /*      the image space coordinate system and image plane normal        */
    /*      vector.                                                         */
    /* -------------------------------------------------------------------- */

    Coordinates ipp{
        {0, 0, -f}};                       // image principal point in image space.
    Coordinates o = RotateImageToNED(ipp); // image principal point in NED.

    double a = o[0].x, b = o[0].y, c = o[0].z;
    Vector3d ipnv(a, b, c); // image plane normal vector (S→o) in NED.

    /* -------------------------------------------------------------------- */
    /*      Calculate the coordinates of the ground principal point in      */
    /*      the NED coordinate system and clip the back-projected and       */
    /*      flattened points in the object space.                           */
    /* -------------------------------------------------------------------- */

    Coordinates O = ProjectImageToNED(ep, ipp, ph - ep.z); // ground principal point in NED.
    /* Remarks: This is different from the patent.                          */
    /* TODO: Implement the corresponding part of the patent.                */
    double d = -dhgt / 300.0;     // experience adjustment distance, the true value: d = -(a * a + b * b + c * c)
    RowVector4d cpec(a, b, c, d); // clip-plane equation coefficients as a row vector.

    Matrix4Xd m_O(4, O.size());
    memcpy(m_O.data(), O.data(), m_O.cols() * m_O.rows() * sizeof(double));

    Matrix4Xd opMat(4, fps.cols() + 1);               // fps expand one row as 4 x (nfps + 1).
    opMat.topLeftCorner(4, 1) = m_O;                  // insert O at 0th col.
    opMat.topRightCorner(4, fps.cols()) = fps;        // insert fps at 1~nth cols.
    opMat.row(3) = RowVectorXd::Ones(fps.cols() + 1); // make last row as all as 1.

    RowVectorXd dpvs = cpec * opMat; // displacement row vector.

    std::vector<Vector4d> ops; // all points in object space.
    double dpO = dpvs(0);      // displacement of O to clip-plane, dpO = a * O(0) + b * O(1) + c * O(2) + d.
    for (int i = 1; i < dpvs.size(); i++)
    {
        double dpPi = dpvs(i); // displacement of Pi to clip-plane.
        if (dpPi * dpO > 0)    // if Pi and O are on the same side of the clipp-plane.
        {
            Vector4d Pi = opMat.block(0, i, 4, 1);
            ops.push_back(Pi); // add Pi into the result list.
        }

        int j = i + 1;
        if (j < dpvs.size())
        {
            double dpPj = dpvs(j); // displacement of Pj to clip-plane.
            if (dpPi * dpPj < 0)   // if PI and PJ are on both sides of the clipping plane respectively
            {
                Vector4d Pi = opMat.block(0, i, 4, 1);
                Vector4d Pj = opMat.block(0, j, 4, 1);
                Vector4d Vij =
                    GetIntersectionOfLineAndPlane(Pi, Pj, ipnv, d); // intersection Vij of Pi-Pj and image plane.
                ops.push_back(Vij);                                 // add Vij into the result list.
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Reflect all points in object space onto image plane.            */
    /* -------------------------------------------------------------------- */
    Matrix4d nRi = _dp->getnRi();

    Matrix4Xd obj(4, ops.size());
    for (int i = 0; i < ops.size(); i++)
    {
        obj.col(i) = ops.at(i);
    }

    // #ifdef _DEBUG
    //
    //	std::cout << std::endl
    //		//<< std::fixed << std::setprecision(8)
    //		<< "ops = " << std::endl
    //		<< (obj.transpose())
    //		<< std::endl << std::endl;
    //
    //	std::cout << std::endl
    //		//<< std::fixed << std::setprecision(8)
    //		<< "nRb = " << std::endl
    //		<< (dp->getnRb())
    //		<< std::endl << std::endl;
    //
    //	std::cout << std::endl
    //		//<< std::fixed << std::setprecision(8)
    //		<< "bRc = " << std::endl
    //		<< (dp->getbRc())
    //		<< std::endl << std::endl;
    //
    //	std::cout << std::endl
    //		//<< std::fixed << std::setprecision(8)
    //		<< "cRi = " << std::endl
    //		<< (dp->getcRi())
    //		<< std::endl << std::endl;
    //
    //	std::cout << std::endl
    //		//<< std::fixed << std::setprecision(8)
    //		<< "nRi = " << std::endl
    //		<< nRi
    //		<< std::endl << std::endl;
    //
    // #endif // _DEBUG

    Matrix4Xd ips = OBJ2IMG(S, nRi, obj, f);

    Coordinates c_ips(ips.cols());
    memcpy(c_ips.data(), ips.data(), ips.cols() * ips.rows() * sizeof(double));

    return c_ips;
}

/// <summary>
/// 利用共线方程根据物点局部切面坐标反算像空间坐标系的像点坐标
/// </summary>
/// <param name="ep">The exposure station is represented by a geodetic position.
/// This geodetic position is the origin of this local coordinate system.</param>
/// <param name="ph">指定椭球高的投影面[m]</param>
/// <param name="f">摄影主距[m]：约等于焦距</param>
/// <param name="tps">物点局部切面坐标</param>
/// <returns>返回像空间坐标系的像点坐标</returns>
Coordinates DirectGeoreference::ReflectENUToImage(const Coordinate &ep, const Coordinates &tps, double f, double ph)
{
    Matrix4d nRt = _dp->gettRn().transpose();

    Matrix4Xd m_tps(4, tps.size());
    memcpy(m_tps.data(), tps.data(), m_tps.cols() * m_tps.rows() * sizeof(double));

    Matrix4Xd nps(nRt * m_tps);

    Coordinates c_nps(nps.cols());
    memcpy(c_nps.data(), nps.data(), nps.cols() * nps.rows() * sizeof(double));

    Coordinates ips = ReflectNEDToImage(ep, c_nps, f, ph);
    return ips;
}

/// <summary>
/// 利用共线方程根据物点局部导航坐标反算像空间坐标系的像点坐标
/// </summary>
/// <param name="ep">The exposure station is represented by a geodetic position.
/// This geodetic position is the origin of this local coordinate system.</param>
/// <param name="ph">指定投影的椭球高</param>
/// <param name="f">摄影主距[m]：约等于焦距</param>
/// <param name="gps">物点大地测量坐标</param>
/// <returns>返回像空间坐标系的像点坐标</returns>
Coordinates DirectGeoreference::ReflectGEOToImage(const Coordinate &ep, const Coordinates &gps, double f, double ph)
{
    GeoEllipsoid ge = GeoEllipsoid(constant::WGS84_RADIUS, constant::WGS84_INVFLATTENING);

    Matrix4Xd m_gps(4, gps.size());
    memcpy(m_gps.data(), gps.data(), m_gps.cols() * m_gps.rows() * sizeof(double));

    Vector4d v_ep(ep.x, ep.y, ep.z, ep.m);

    Matrix4Xd nps = GEO2NED(v_ep, ge, m_gps);
    Coordinates c_nps(nps.cols());
    memcpy(c_nps.data(), nps.data(), nps.cols() * nps.rows() * sizeof(double));

    Coordinates ips = ReflectNEDToImage(Coordinate{0, 0, ep.z}, c_nps, f, ph);

    // #ifdef _DEBUG
    //
    //	std::cout << std::endl
    //		//<< std::fixed << std::setprecision(8)
    //		<< "gps = " << std::endl
    //		<< (gps.transpose())
    //		<< std::endl << std::endl;

    //	std::cout << std::endl
    //		//<< std::fixed << std::setprecision(8)
    //		<< "nps = " << std::endl
    //		<< (nps.transpose())
    //		<< std::endl << std::endl;
    //	std::cout << std::endl
    //		//<< std::fixed << std::setprecision(8)
    //		<< "ips = " << std::endl
    //		<< (ips.transpose())
    //		<< std::endl << std::endl;
    //
    // #endif // _DEBUG

    // Matrix4Xd m_ips(nps.cols());
    // memcpy(m_ips.data(), ips.data(), m_ips.cols()*m_ips.rows() * sizeof(double));

    // m_ips.row(2) = RowVectorXd::Ones(m_ips.cols());

    // std::cout << std::endl
    //	//<< std::fixed << std::setprecision(8)
    //	<< "ips = " << std::endl
    //	<< (m_ips.transpose())
    //	<< std::endl << std::endl;
    // Matrix4d A = MakeAffine(1920, 1080);
    // A(3, 3) = 1;
    // Matrix4Xd pxs = A.inverse() * m_ips; // px = [1 u v]^T

    // Coordinates c_pxs(pxs.cols());
    // memcpy(c_pxs.data(), pxs.data(), pxs.cols()*pxs.rows() * sizeof(double));

    return ips;
}

DirectGeoreferenceH DirectGeoreferenceCreate(EulerAngles load, EulerAngles body)

    {
        DGR_TRY{
            DirectGeoreference *geoPtr = new DirectGeoreference(IMS_BLUH);
geoPtr->setLoadAngles(load); // 设置载荷姿态角
geoPtr->setBodyAngles(body); // 设置机体姿态角
return static_cast<DirectGeoreferenceH>(geoPtr);
}
DGR_CATCH(nullptr)
}

Coordinate DirectGeoreferenceImageToGeo(
    DirectGeoreferenceH hDirectGeoreference, Coordinate uavLoc, double targetAlt, CameraInfo cInfo, Coordinate imageLoc){
    DGR_TRY{
        DirectGeoreference *geoPtr = static_cast<DirectGeoreference *>(hDirectGeoreference);
EulerAngles kpo = geoPtr->getImageKappaPhiOmega(); // 获取图像外方位元素
double d1 = (cInfo.frameWidth / cInfo.pixelWidth) / 1000;
double d2 = (cInfo.frameHeight / cInfo.pixelHeight) / 1000;
double m = (cInfo.frameWidth / 2.0) / 1000;
double n = (cInfo.frameHeight / 2.0) / 1000;
double f = (/*sqrt(pow(cInfo.frameHeight, 2) + pow(cInfo.frameWidth, 2))*/ cInfo.frameWidth / 2.0 /
            tan(cInfo.fov / 2.0 * RADIANS_PER_DEGREE)) /
           1000;                                                   // 焦距
Coordinate con{-(d2 * imageLoc.y - n), -(d1 *imageLoc.x - m), -f}; // BLUH (uv -- xyz)
Coordinates img = {con};                                           // 输入的像点坐标
return geoPtr->ProjectImageToGEO(uavLoc, img, targetAlt)[0];       // 像点到大地
}
DGR_CATCH(Coordinate{})
}

Coordinate DirectGeoreferenceGeoToImage(
    DirectGeoreferenceH hDirectGeoreference, Coordinate uavLoc, double targetAlt, CameraInfo cInfo, Coordinate geoLoc)
{
    DGR_TRY
    {
        DirectGeoreference *geoPtr = static_cast<DirectGeoreference *>(hDirectGeoreference);
        EulerAngles kpo = geoPtr->getImageKappaPhiOmega(); // 获取图像外方位元素
        double d1 = (cInfo.frameWidth / cInfo.pixelWidth) / 1000;
        double d2 = (cInfo.frameHeight / cInfo.pixelHeight) / 1000;
        double m = (cInfo.frameWidth / 2.0) / 1000;
        double n = (cInfo.frameHeight / 2.0) / 1000;
        double f = (/*sqrt(pow(cInfo.frameHeight, 2) + pow(cInfo.frameWidth, 2))*/ cInfo.frameWidth / 2.0 /
                    tan(cInfo.fov / 2.0 * RADIANS_PER_DEGREE)) /
                   1000;                                                            // 焦距
        Coordinates img = {geoLoc};                                                 // 输入的像点坐标
        Coordinates cImages = geoPtr->ReflectGEOToImage(uavLoc, img, f, targetAlt); // 物点到像点
        int s = (int)cImages.size();
        for (int i = 0; i < s; i++) // BLUH (xy -- uv)
        {
            double tempx = cImages[i].x;
            cImages[i].x = (int)((-cImages[i].y + m) / d1);
            cImages[i].y = (int)((-tempx + n) / d2);
            cImages[i].z = 0;
        }
        if(!cImages.empty())
            return Coordinate{cImages[0].x, cImages[0].y};
        return Coordinate{constant::DBL_NAN, constant::DBL_NAN};
    }
    DGR_CATCH(Coordinate{})
}

void DirectGeoreferenceDestroy(DirectGeoreferenceH hDirectGeoreference)
{
    if (hDirectGeoreference != nullptr)
    {
        delete static_cast<DirectGeoreference *>(hDirectGeoreference);
    }
}