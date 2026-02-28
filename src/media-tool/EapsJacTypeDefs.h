#ifndef EAPS_JAC_TYPE_DEFS_H
#define EAPS_JAC_TYPE_DEFS_H

#include <cstdint>

#ifndef UINT8
typedef unsigned char UINT8;
#endif

#ifndef INT8
typedef signed char INT8;
#endif

#ifndef UINT16
typedef unsigned short int UINT16;
#endif

#ifndef INT16
typedef short int INT16;
#endif

#ifndef UINT32
typedef unsigned int UINT32;
#endif

#ifndef INT32
typedef int INT32;
#endif

#ifndef FLOAT32
typedef float FLOAT32;
#endif

//Constant
#define DB_PI (3.14159265358979323846f)
#define HALF_PI (3.14159265358979323846 / 2.0f)
#ifndef TWO_PI
#define TWO_PI (3.14159265358979323846 * 2.0f)
#endif
#define EARTHA (6378137.0f) //major axis of the earth
#define EARTHB (6356752.31424518f)
#define WGS84E12 (0.00669437999014112f) //(a^2-b^2)/a^2
#define WGS84E22 (0.00673949674227624f) //(a^2-b^2)/b^2
#define OMEGAE (7.292115e-5f)           // rotate rate of the earth
#define ECCENT (8.181919e-2f)           //eccentre of the earth
#define FLATF (3.352813e-3f)            //flat rate of the earth
#define RAD2DEG (57.2957795131f)
#define PI (3.1415926)
#define DEG2RAD (0.01745329252f)
#define GRIVATY (9.8f)
#define ECCENT2 ((8.181919e-2f) * (8.181919e-2f))

#ifndef Min
#define Min(x, y) (x < y ? x : y)
#endif

#ifndef Max
#define Max(x, y) (x > y ? x : y)
#endif

#ifndef Sqr
#define Sqr(x) ((x) * (x))
#endif

#ifndef NormRadAngle
#define NormRadAngle(x)  \
    {                    \
        if (x > DB_PI)   \
            x -= TWO_PI; \
        if (x < -DB_PI)  \
            x += TWO_PI; \
    }
#endif

#ifndef Bound
#define Bound(_x, _min, _max) \
    {                         \
        if (_x > _max)        \
            _x = _max;        \
        else if (_x < _min)   \
            _x = _min;        \
    }
#endif

#ifndef BoundAbs
#define BoundAbs(_x, _max) Bound(_x, -_max, _max)
#endif

// Mask constants
#define BIGEND_BIT0 0x80000000  //!< Bit 0 in 32-bit PowerPC, most significant bit
#define BIGEND_BIT1 0x40000000  //!< Bit 1 in 32-bit PowerPC
#define BIGEND_BIT2 0x20000000  //!< Bit 2 in 32-bit PowerPC
#define BIGEND_BIT3 0x10000000  //!< Bit 3 in 32-bit PowerPC
#define BIGEND_BIT4 0x08000000  //!< Bit 4 in 32-bit PowerPC
#define BIGEND_BIT5 0x04000000  //!< Bit 5 in 32-bit PowerPC
#define BIGEND_BIT6 0x02000000  //!< Bit 6 in 32-bit PowerPC
#define BIGEND_BIT7 0x01000000  //!< Bit 7 in 32-bit PowerPC
#define BIGEND_BIT8 0x00800000  //!< Bit 8 in 32-bit PowerPC
#define BIGEND_BIT9 0x00400000  //!< Bit 9 in 32-bit PowerPC
#define BIGEND_BIT10 0x00200000 //!< Bit 10 in 32-bit PowerPC
#define BIGEND_BIT11 0x00100000 //!< Bit 11 in 32-bit PowerPC
#define BIGEND_BIT12 0x00080000 //!< Bit 12 in 32-bit PowerPC
#define BIGEND_BIT13 0x00040000 //!< Bit 13 in 32-bit PowerPC
#define BIGEND_BIT14 0x00020000 //!< Bit 14 in 32-bit PowerPC
#define BIGEND_BIT15 0x00010000 //!< Bit 15 in 32-bit PowerPC
#define BIGEND_BIT16 0x00008000 //!< Bit 16 in 32-bit PowerPC
#define BIGEND_BIT17 0x00004000 //!< Bit 17 in 32-bit PowerPC
#define BIGEND_BIT18 0x00002000 //!< Bit 18 in 32-bit PowerPC
#define BIGEND_BIT19 0x00001000 //!< Bit 19 in 32-bit PowerPC
#define BIGEND_BIT20 0x00000800 //!< Bit 20 in 32-bit PowerPC
#define BIGEND_BIT21 0x00000400 //!< Bit 21 in 32-bit PowerPC
#define BIGEND_BIT22 0x00000200 //!< Bit 22 in 32-bit PowerPC
#define BIGEND_BIT23 0x00000100 //!< Bit 23 in 32-bit PowerPC
#define BIGEND_BIT24 0x00000080 //!< Bit 24 in 32-bit PowerPC
#define BIGEND_BIT25 0x00000040 //!< Bit 25 in 32-bit PowerPC
#define BIGEND_BIT26 0x00000020 //!< Bit 26 in 32-bit PowerPC
#define BIGEND_BIT27 0x00000010 //!< Bit 27 in 32-bit PowerPC
#define BIGEND_BIT28 0x00000008 //!< Bit 28 in 32-bit PowerPC
#define BIGEND_BIT29 0x00000004 //!< Bit 29 in 32-bit PowerPC
#define BIGEND_BIT30 0x00000002 //!< Bit 30 in 32-bit PowerPC
#define BIGEND_BIT31 0x00000001 //!< Bit 31 in 32-bit PowerPC, least significant bit

#define BIT(nr) (1 << (nr))

#ifndef null
#define null ((void *)0)
#endif

#endif // !EAPS_JAC_TYPE_DEFS_H