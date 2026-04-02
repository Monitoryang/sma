#ifndef EAPS_CRC32_H
#define EAPS_CRC32_H

#include <stdint.h>

namespace eap {
	namespace sma {
		uint32_t GetCRC32(uint32_t crc, uint8_t* buf, int len);
	}
}

#endif // !EAPS_CRC32_H
