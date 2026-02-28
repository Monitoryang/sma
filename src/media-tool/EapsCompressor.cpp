#include "EapsCompressor.h"

#include "Logger.h"
#include "Poco/DeflatingStream.h"
#include "Poco/InflatingStream.h"

#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <iostream>

namespace eap {
	namespace sma {
		std::vector<std::uint8_t> zlibCompress(const std::vector<std::uint8_t>& raw_data)
		{
			// Create an input stringstream
			std::stringstream input;
			input.write(reinterpret_cast<const char*>(raw_data.data()), raw_data.size() * sizeof(std::uint8_t));

			// Create a DeflatingOutputStream
			std::stringstream outputStream;
			Poco::DeflatingOutputStream deflater(outputStream);

			// Compress the input data
			deflater << input.rdbuf();
			deflater.close();

			// Get the compressed data
			std::string compressedData = outputStream.str();
			std::vector<std::uint8_t> compressedBytes(compressedData.begin(), compressedData.end());

			return compressedBytes;
		}

		std::vector<std::uint8_t> zlibDecompress(const std::vector<std::uint8_t>& raw_data)
		{
			// Create an input stringstream
			std::stringstream input;
			input.write(reinterpret_cast<const char*>(raw_data.data()), raw_data.size() * sizeof(std::uint8_t));

			// Create a decompressed stringstream
			std::stringstream decompressedStream;

			// Create an InflatingInputStream
			Poco::InflatingInputStream inflater(input, Poco::InflatingStreamBuf::STREAM_ZLIB);

			// Decompress the input data
			decompressedStream << inflater.rdbuf();

			// Get the decompressed data
			std::string decompressedData = decompressedStream.str();

			// Store the decompressed data in a vector
			std::vector<std::uint8_t> decompressedBytes(decompressedData.begin(), decompressedData.end());
			return decompressedBytes;
		}
	}
}