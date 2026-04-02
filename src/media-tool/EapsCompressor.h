#ifndef EAPS_COMPRESSOR_H
#define EAPS_COMPRESSOR_H

#include <vector>
#include <cstdint>

namespace eap {
	namespace sma {
		std::vector<std::uint8_t> zlibCompress(const std::vector<std::uint8_t>& raw_data);
		std::vector<std::uint8_t> zlibDecompress(const std::vector<std::uint8_t>& raw_data);
	}
}

#endif // !EAPS_COMPRESSOR_H




