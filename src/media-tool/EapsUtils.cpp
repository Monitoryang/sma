#include "EapsUtils.h"
#include "Logger.h"

#include "Poco/File.h"
#include "Poco/DirectoryIterator.h"
#include <Poco/FileStream.h>
#include <Poco/StreamCopier.h>
#include <iostream>

#include <sstream>
#include <random>
#include <string>
#include <chrono>
#if defined(_WIN32)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
extern "C" const IMAGE_DOS_HEADER __ImageBase;
#endif // defined(_WIN32)

#if defined(_WIN32)
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif // !PATH_MAX
#else
#include<unistd.h>
#endif // defined(_WIN32)

namespace eap {
	namespace sma {
		using namespace std;
		static constexpr char CCH[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

		static const uint8_t map2[] =
		{
			0x3e, 0xff, 0xff, 0xff, 0x3f, 0x34, 0x35, 0x36,
			0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01,
			0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
			0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
			0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1a, 0x1b,
			0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
			0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
			0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33
		};

		unsigned int random_char()
		{
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> dis(0, 255);
			return dis(gen);
		}

		ByteArray_t MakeByteArray(std::size_t size, std::uint8_t* data)
		{
			auto byte_array = std::make_shared<std::vector<std::uint8_t>>(size);
			if (data) {
				memcpy(byte_array->data(), data, size);
			}
			return byte_array;
		}

		std::string generate_guid(const unsigned int len)
		{
			std::stringstream ss;
			for (auto i = 0; i < len; i++) {
				const auto rc = random_char();
				std::stringstream hexstream;
				hexstream << std::hex << rc;
				auto hex = hexstream.str();
				ss << (hex.length() < 2 ? '0' + hex : hex);
			}
			return ss.str();
		}

		std::string get_current_time_string_second()
		{
			auto tt = std::chrono::system_clock::to_time_t
			(std::chrono::system_clock::now());
			struct tm* ptm = localtime(&tt);
			char date[60] = { 0 };
			sprintf(date, "%d_%02d_%02d_%02d_%02d_%02d",
				(int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
				(int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
			return std::string(date);
		}

		std::string get_current_time_string_second_compact()
		{
			auto tt = std::chrono::system_clock::to_time_t
			(std::chrono::system_clock::now());
			struct tm* ptm = localtime(&tt);
			char date[60] = { 0 };
			sprintf(date, "%d%02d%02d%02d%02d%02d",
				(int)ptm->tm_year + 1900 - 2000, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
				(int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
			return std::string(date);
		}

		std::string exePath(bool isExe /*= true*/) {
			char buffer[PATH_MAX * 2 + 1] = { 0 };
			int n = -1;
#if defined(_WIN32)
			n = GetModuleFileNameA(isExe ? nullptr : (HINSTANCE)&__ImageBase, buffer, sizeof(buffer));
#elif defined(__MACH__) || defined(__APPLE__)
			n = sizeof(buffer);
			if (uv_exepath(buffer, &n) != 0) {
				n = -1;
			}
#elif defined(__linux__)
			n = readlink("/proc/self/exe", buffer, sizeof(buffer));
#endif

			std::string filePath;
			if (n <= 0) {
				filePath = "./";
			}
			else {
				filePath = buffer;
			}

#if defined(_WIN32)
			for (auto &ch : filePath) {
				if (ch == '\\') {
					ch = '/';
				}
			}
#endif //defined(_WIN32)

			return filePath;
		}

		std::string exeDir(bool isExe /*= true*/) {
			auto path = exePath(isExe);
			return path.substr(0, path.rfind('/') + 1);
		}
        std::string readFileContents(const std::string &filePath)
        {
			try {
				// 打开文件
				Poco::File file(filePath);
				if (!file.exists()) {
					throw Poco::FileNotFoundException(filePath);
				}

				// 创建文件输入流
				Poco::FileInputStream fis(filePath);

				// 读取文件内容到字符串
				std::string contents;
				Poco::StreamCopier::copyToString(fis, contents);

				return contents;
			} catch (Poco::Exception &ex) {
				std::cerr << "Poco Exception: " << ex.displayText() << std::endl;
				throw;
			} catch (std::exception &ex) {
				std::cerr << "Standard Exception: " << ex.what() << std::endl;
				throw;
			}
            return std::string();
        }
        void listFilesRecursively(const Poco::Path &path, std::list<std::string> &paths, const std::string &prefix, const std::string &endfix)
        {
			for (Poco::DirectoryIterator it(path); it != Poco::DirectoryIterator(); ++it) {
				if (it->isDirectory()) {
					listFilesRecursively(it->path(), paths, prefix, endfix);
				} else {
					std::string p = it->path();
					std::string file_name = it.name();
					if (Poco::startsWith<std::string>(file_name, prefix) && Poco::endsWith<std::string>(file_name, endfix)) {
						eap_information_printf(" %s", it->path());
						paths.emplace_back(it->path());
					}else {
						return;
					}
				}
			}
        }
        std::string getEnv(const std::string &key)
        {
			auto ekey = key.c_str();
			if (*ekey == '$') {
				++ekey;
			}
			auto value = *ekey ? getenv(ekey) : nullptr;
			return value ? value : "";
		}
		std::string jsonToString(const Poco::JSON::Object & json)
		{
			std::string json_string = "";
			try {
				std::ostringstream oss;
				json.stringify(oss);
				json_string = oss.str();
			} catch(std::exception& e){
				eap_error_printf("jsonToString exception: %s", e.what());
			}
			return json_string;
		}
        bool moveAndRenameFile(std::string src, std::string dst)
        {
            try {
				Poco::Path sourcePath(src);
				Poco::File sourceFile(sourcePath);
				if (!sourceFile.exists()) {
					eap_error_printf("Source file does not exist: %s", src);
					return false;
				}
				// 移动并重命名文件
				sourceFile.moveTo(dst);
			} catch (const Poco::Exception& exc) {
				eap_error_printf("Error: %s", exc.displayText());
				return false;
			} catch (const std::exception& exc) {
				eap_error_printf("Error: %s", std::string(exc.what()));
				return false;
			}
			return true;
        }
        bool fileExist(const std::string &path)
        {
			Poco::File file(path);
			return file.exists();
		}
		void recursiveRemove(Poco::Path path)
		{
			Poco::File file(path);
			if (file.exists())
			{
				if (file.isDirectory())
				{
					Poco::DirectoryIterator it(file), end;
					for (; it != end; ++it)
					{
						recursiveRemove(it.path());
					}
				}

				file.remove();
			}
		}
		void deleteFile(const std::string & path)
		{
			Poco::Path pathToRemove(path);
			try {
				recursiveRemove(pathToRemove);
			} catch (const Poco::Exception& exc) {
				eap_error_printf("Error removing: %s, reason: %s", pathToRemove.toString(), exc.displayText());
			}
		}

		bool createPath(const std::string & path)
		{
			try {
				Poco::File dir(path);
				if (!dir.exists())
				{
					dir.createDirectories();
				}
			} catch (const Poco::Exception& e) {
				eap_error_printf("Error creating directory %s: %s", path, e.what());
				return false;
			}
			return true;
		}
		std::string makeRandStr(int sz, bool printable)
		{
			std::string ret;
			ret.resize(sz);
			std::mt19937 rng(std::random_device{}());
			for (int i = 0; i < sz; ++i) {
				if (printable) {
					uint32_t x = rng() % (sizeof(CCH) - 1);
					ret[i] = CCH[x];
				}
				else {
					ret[i] = rng() % 0xFF;
				}
			}
			return ret;
		}
        int av_base64_decode(uint8_t *out, const char *in, int out_size)
        {
            int i, v;
			uint8_t *dst = out;

			v = 0;
			for (i = 0; in[i] && in[i] != '='; i++) {
				unsigned int index= in[i]-43;
				if (index>=FF_ARRAY_ELEMS(map2) || map2[index] == 0xff)
					return -1;
				v = (v << 6) + map2[index];
				if (i & 3) {
					if (dst - out < out_size) {
						*dst++ = v >> (6 - 2 * (i & 3));
					}
				}
			}

			return dst - out;
        }
		/*****************************************************************************
		* b64_encode: Stolen from VLC's http.c.
		* Simplified by Michael.
		* Fixed edge cases and made it work from data (vs. strings) by Ryan.
		*****************************************************************************/

		char *av_base64_encode_l(char *out, int *out_size, const uint8_t *in, int in_size) {
			static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			char *ret, *dst;
			unsigned i_bits = 0;
			int i_shift = 0;
			int bytes_remaining = in_size;

			if ((size_t)in_size >= UINT_MAX / 4 || *out_size < AV_BASE64_SIZE(in_size)) {
				return nullptr;
			}
			ret = dst = out;
			while (bytes_remaining) {
				i_bits = (i_bits << 8) + *in++;
				bytes_remaining--;
				i_shift += 8;

				do {
					*dst++ = b64[(i_bits << 6 >> i_shift) & 0x3f];
					i_shift -= 6;
				} while (i_shift > 6 || (bytes_remaining == 0 && i_shift > 0));
			}
			while ((dst - ret) & 3)
				*dst++ = '=';
			*dst = '\0';

			*out_size = dst - out;
			return ret;
		}

		char *av_base64_encode(char *out, int out_size, const uint8_t *in, int in_size) {
			return av_base64_encode_l(out, &out_size, in, in_size);
		}

		string encodeBase64(const string &txt) {
			if (txt.empty()) {
				return "";
			}
			int size = AV_BASE64_SIZE(txt.size()) + 10;
			string ret;
			ret.resize(size);

			if (!av_base64_encode_l((char *) ret.data(), &size, (const uint8_t *) txt.data(), txt.size())) {
				return "";
			}
			ret.resize(size);
			return ret;
		}

		string decodeBase64(const string &txt) {
			if (txt.empty()) {
				return "";
			}
			string ret;
			ret.resize(txt.size() * 3 / 4 + 10);
			auto size = av_base64_decode((uint8_t *) ret.data(), txt.data(), ret.size());

			if (size <= 0) {
				return "";
			}
			ret.resize(size);
			return ret;
		}
    }
}