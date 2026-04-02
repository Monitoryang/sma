#ifndef EAPS_FILE_MANAGER_H
#define EAPS_FILE_MANAGER_H

#define SD_MEMORY_RSV 200
#define SINGLE_FILE_SIZE (8 * 1024) /*3600*/
#define FILE_IDX_LEN 12

#include <string.h>
#include <mutex>
#include <functional>
#include <sys/stat.h> 
#include <sys/types.h>
// #include <sys/statfs>
// #include <dirent.h>
#include <stdio.h>
#include <sstream>
#include <algorithm>
#include <malloc.h>
#include <time.h>
#include <sys/stat.h>
#include <memory>

#ifdef _WIN32
#include <corecrt_io.h>
#define popen _popen
#define pclose _pclose
#endif
namespace eap {
	namespace sma {
		enum class StoredStatus
		{
			Ok,
			NotHaveSDCardDevice,
			MountSucceedButDeleteFileFailedAndLessThan200M,
			MountSucceedButDeleteFileFailedAndGreaterThan200MAndLessThan8G,
			HaveDeviceButMountFailed
		};

		class FileManager;
		using FileManagerPtr = std::shared_ptr<FileManager>;

		class FileManager
		{
		public:
			static FileManagerPtr createInstance();

		public:
			void setParams(std::string& sd_path, std::string& sd_name, std::string& mount_point);
			//bool getFileSystemSize(std::string path, int& free_size, int& total_size);
			bool fileExist(std::string& path, std::string& file_name);
			bool testSDCard(std::string mount_path);
			bool checkSDCardMount(std::string device_name, std::string mount_path);
			bool checkMount(std::string device_name, std::string mount_path, FILE* fp);
			void fileSystemCheck();
			int getFileCount();
			int removeFile();
			std::string getNewTargetFileName(std::string& prefix);
			std::string removeAndGetNewName(std::string& prefix);
			void getTime(std::uint16_t& year, std::uint16_t& mon, std::uint16_t& day,
				std::uint16_t& hour, std::uint16_t& minute, std::uint16_t& second);
			std::string monitorSpace();
			void setCbs(std::function<void()> func, bool hdflag);
			void setFileThreadStatus(bool status);

		private:
			std::string dev_path_;
			std::string dev_name_;
			std::string mount_path_;
			std::function<void()> resume_store_hd_;
			std::function<void()> resume_store_sd_;
			std::mutex file_manage_mutex_;
			bool file_thread_status{ true };

		private:
			FileManager();
			FileManager(FileManager& other) = delete;
			FileManager(FileManager&& other) = delete;
			FileManager& operator=(FileManager& other) = delete;
			FileManager& operator=(FileManager&& other) = delete;
		};
	}
}

#endif // !EAPS_FILE_MANAGER_H