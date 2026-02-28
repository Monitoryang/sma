#ifndef EAPS_UTILS_H
#define EAPS_UTILS_H

#include "Poco/JSON/Object.h"
#include "Poco/Path.h"

#include <string>

namespace eap {
	namespace sma {
		#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

		using ByteArray_t = std::shared_ptr<std::vector<std::uint8_t>>;
		ByteArray_t MakeByteArray(std::size_t size, std::uint8_t* data = nullptr);

		std::string generate_guid(const unsigned int len);
		std::string get_current_time_string_second();
		std::string get_current_time_string_second_compact();
		std::string exePath(bool isExe = true);
		std::string exeDir(bool isExe = true);
		std::string readFileContents(const std::string& filePath);
		void listFilesRecursively(const Poco::Path& path, std::list<std::string>& paths, const std::string& prefix, const std::string& endfix);

		std::string getEnv(const std::string &key);

		std::string jsonToString(const Poco::JSON::Object &json);

		bool moveAndRenameFile(std::string src, std::string dst); 

		bool fileExist(const std::string &path);

		void recursiveRemove(Poco::Path path);

		void deleteFile(const std::string &path);

		bool createPath(const std::string &path);

		std::string makeRandStr(int sz, bool printable = true);

		/**
		 * Decode a base64-encoded string.
		 *
		 * @param out      buffer for decoded data
		 * @param in       null-terminated input string
		 * @param out_size size in bytes of the out buffer, must be at
		 *                 least 3/4 of the length of in
		 * @return         number of bytes written, or a negative value in case of
		 *                 invalid input
		 */
		int av_base64_decode(uint8_t *out, const char *in, int out_size);

		/**
		 * Encode data to base64 and null-terminate.
		 *
		 * @param out      buffer for encoded data
		 * @param out_size size in bytes of the output buffer, must be at
		 *                 least AV_BASE64_SIZE(in_size)
		 * @param in_size  size in bytes of the 'in' buffer
		 * @return         'out' or NULL in case of error
		 */
		char *av_base64_encode(char *out, int out_size, const uint8_t *in, int in_size);

		/**
		 * Calculate the output size needed to base64-encode x bytes.
		 */
		#define AV_BASE64_SIZE(x)  (((x)+2) / 3 * 4 + 1)


		/**
		 * 编码base64
		 * @param txt 明文
		 * @return 密文
		 */
		std::string encodeBase64(const std::string &txt);

		/**
		 * 解码base64
		 * @param txt 密文
		 * @return 明文
		 */
		std::string decodeBase64(const std::string &txt);
	}
}

#endif // !EAPS_UTILS_H

