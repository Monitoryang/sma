#include "EapsFileManager.h"

#include <Poco/File.h>
#include "Poco/Exception.h"
#include <Poco/DirectoryIterator.h>

#include <iostream>
#include <thread>
namespace eap {
	namespace sma {
		FileManagerPtr FileManager::createInstance()
		{
			return FileManagerPtr(new FileManager());
		}
		void FileManager::setParams(std::string& sd_path, std::string& sd_name, std::string& mount_point)
		{
			dev_path_ = sd_path;
			dev_name_ = sd_name;
			mount_path_ = mount_point;
			resume_store_hd_ = nullptr;
			resume_store_sd_ = nullptr;
		}

		// use when record on/off
		/*
		bool FileManager::getFileSystemSize(std::string path, int& free_size, int& total_size)
		{
			std::unique_lock<std::mutex> lock(file_manage_mutex_);

			struct statfs myStatfs;
			if (statfs(path.c_str(), &myStatfs) == -1)
			{
				return false;
			}

			free_size = (((long long)myStatfs.f_bsize * (long long)myStatfs.f_bfree) / (long long)1024 / (long long)1024);
			total_size = (((long long)myStatfs.f_bsize * (long long)myStatfs.f_blocks) / (long long)1024 / (long long)1024);

			return true;
		}
		*/

		/*check mmcblk exist or not*/
		bool FileManager::fileExist(std::string& path, std::string& file_name)
		{

			Poco::File file((path + "/" + file_name));
			return file.exists();
		}

		bool FileManager::testSDCard(std::string mount_path)
		{
			bool write_flag = 0;
			bool create_flag = 0;
			bool final_flag = 0;
			int file_data;
			std::string filepath = mount_path + std::string("/test.txt");
			std::string filename = std::string("touch ") + filepath;
			system(filename.c_str());
			std::string filewrite = std::string("echo 123456789 > ") + filepath;
			system(filewrite.c_str());

			FILE* pfile = fopen(filepath.c_str(), "r+t");
			std::string rmfile = "rm " + filepath;
			// �򿪹���·��������test.txt�ı��ļ���д��123456789������ɹ�д���򷵻�write_flag=1��û�гɹ�writeflag=0������Ƿ��ܳɹ�д�����·����
			if (pfile != NULL)
			{
				fscanf(pfile, "%d", &file_data);
				if (file_data == 123456789)
				{
					write_flag = 1;
					fclose(pfile);
					system(rmfile.c_str());
					printf("***********create file %s success", filepath.c_str());
				}
				else
				{
					write_flag = 0;
					fclose(pfile);
					system("rm /mnt/sdcard/VIDEO/test.txt");
					printf("***********create file %s failed", filepath.c_str());
				}
			}
			else
			{
				write_flag = 0;
				printf("***********create file %s failed", filepath.c_str());
			}
			// ����Ƿ��ܳɹ�����?ts�ļ�
			std::string video_file_name = mount_path + "/video.ts";
			FILE* out_file_ = fopen(video_file_name.c_str(), "wb");

			if (out_file_)
			{
				create_flag = 1;
				fclose(out_file_);
				std::string rmvideofile = "rm " + video_file_name;
				system(rmvideofile.c_str());
				printf("***********create file %s success", video_file_name.c_str());
			}
			else
			{
				create_flag = 0;
				printf("***********create file %s failed", video_file_name.c_str());
			}
			// �ܳɹ�д���ļ��Լ��ɹ�����.ts�ļ�
			if ((create_flag) == 1 && (write_flag == 1))
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		// ���SD���������?
		bool FileManager::checkSDCardMount(std::string device_name, std::string mount_path)
		{
   /*
			// �����ļ�ָ��
			FILE* fp = NULL;
			char buf[500];
			std::string tmpPath[3];
			// std::stringstream stream;
			std::string s;
			// char s1[256]{0};
			int len;

			int mountok;
			// �г��ѹ��ص��ļ�ϵͳ �����豸�� �ļ�ϵͳ���� �Լ�����Ŀ¼��ʾ
			std::string op_f = std::string("mount|grep ") + device_name;
			// �Զ��뷽ʽ ִ��op_f����
			fp = popen(op_f.c_str(), "r");

			if (!fp)
			{
				perror("popen");
				return false;
			}

			printf("op_f: %s\n", op_f.c_str());

			memset(buf, 0, sizeof(buf));
			// ���ж�ȡ��ָ����������fp��ȡ���ݣ�������buf��
			fgets(buf, sizeof(buf), fp);

			std::stringstream stream;
			// ����<< �����ݴ��ݸ�stream
			stream << buf;

			std::string str = stream.str();

			std::istringstream iss(str);

			for (int i = 0; i < 3; ++i)
			{
				// ���ڷָ�ո�ָ���ַ���
				iss >> tmpPath[i];
			}

			pclose(fp);

			if (fp != NULL)
			{
				fp = NULL;
			}

			len = tmpPath[2].length();
			// s1 = (char *)malloc(len * sizeof(char));
			std::string s1 = tmpPath[2];

			// strcpy(s1, tmpPath[2].c_str());

			printf("s1: %s, mount_path: %s\n", s1.c_str(), mount_path.c_str());

			if (NULL != strstr(s1.c_str(), mount_path.c_str()))
			{

				if (testSDCard(mount_path))
				{
					mountok = 1;
					printf("SD mount success!\n");
				}
				else
				{
					mountok = 0;
					printf("SD mount success but can't write!\n");
				}
			}
			// û�й���sd��
			else
			{
				// ���¹���
				char sysSDmount[500];
				// sprintf����һ����ʽ�����ַ��������Ŀ���ַ���?
				sprintf(sysSDmount, "echo 112 | sudo -S umount %s", s1.c_str());

				if (0 == system(sysSDmount))
				{
					// ����Ŀ¼
					if (0 == system(std::string("echo 112 | sudo -S mount " + device_name + " " + mount_path).c_str()))
					{
						// ����ѹ����ļ�����鵽���й���·���Ϸ� ��˵�����سɹ�
						mountok = checkMount(device_name, mount_path, fp);

						if (mountok)
						{
							// ���سɹ�����sd���Ƿ���?
							if (testSDCard(mount_path))
							{
								mountok = 1;
								printf("SD Remount success!\n");
							}
							else
							{
								mountok = 0;
								printf("SD Remount success but can't write!\n");
							}
						}
						else
						{
							printf("SD Remount failed!\n");
						}
					}
					else
					{
						mountok = 0;
						printf("SD Remount failed again!\n");
					}
				}
				else
				{
					if (0 == system(std::string("echo 112 | sudo -S mount " + device_name + " " + mount_path).c_str()))
					{
						mountok = checkMount(device_name, mount_path, fp);

						if (mountok)
						{
							if (testSDCard(mount_path))
							{
								mountok = 1;
								printf("SD mount success again!\n");
							}
							else
							{
								mountok = 0;
								printf("SD mount success again but can't write!\n");
							}
						}
						else
						{
							printf("SD mount failed again!\n");
						}
					}
					else
					{
						mountok = 0;
						printf("SD mount failed!\n");
					}
				}
			}

			// free(s1);

			if (mountok == 1)
			{
				// �ж���Ƶ�洢·���Ƿ����?
				std::string vtp = mount_path + "/VIDEO";
				bool VtpValid = false;
				if (0 == access(vtp.c_str(), 00))
				{
					std::cout << "1.video storage path exist" << std::endl;
					VtpValid = true;
				}
				else
				{
					VtpValid = false;
					std::cout << "1.video storage path not exist" << std::endl;
				}
				// �����ļ��У�����0�ɹ�������-1���ɹ�
				if (!VtpValid)
				{
					std::cout << "mkdir video storage path" << std::endl;
					std::string systemcmd = "chmod 777 -R " + mount_path;
					system(systemcmd.c_str());
					int status;
					systemcmd = "echo 112 | sudo -S mkdir " + mount_path + "/VIDEO";
					system(systemcmd.c_str());
					systemcmd = "chmod 777 " + mount_path + "/VIDEO";
					status = system(systemcmd.c_str());
				}

				if (0 == access(vtp.c_str(), 00))
				{
					if (!VtpValid)
					{
						std::cout << "mkdir video storage path success" << std::endl;
					}
				}
				else
				{
					std::cout << "mkdir video storage path failed" << std::endl;
				}
			}

			return mountok;*/
      return false;
		}
		// ������·��
		bool FileManager::checkMount(std::string device_name, std::string mount_path, FILE* fp)
		{
			char buf[500];
			std::string tmpPath[3];

			char* s1;
			int len;

			int mount_flag;

			std::string op_f = std::string("mount|grep ") + device_name;

			fp = popen(op_f.c_str(), "r");

			if (!fp)
			{
				perror("popen");
				return false;
			}

			memset(buf, 0, sizeof(buf));

			fgets(buf, sizeof(buf), fp);
			std::stringstream stream;
			stream << buf;

			std::string str = stream.str();

			std::istringstream iss(str);

			for (int i = 0; i < 3; ++i)
			{
				iss >> tmpPath[i];
			}

			pclose(fp);

			if (fp != NULL)
			{
				fp = NULL;
			}
			// tmpPath2 ������·��
			len = tmpPath[2].length();
			s1 = (char*)malloc(len * sizeof(char));

			strcpy(s1, tmpPath[2].c_str());

			if (NULL != strstr(s1, mount_path.c_str()))
			{
				mount_flag = 1;
			}
			else
			{
				mount_flag = 0;
			}

			free(s1);

			return !!mount_flag;
		}

		void FileManager::fileSystemCheck()
		{
			// �����豸������
			if (!fileExist(dev_path_, dev_name_))
			{
				StoredStatus sd_status = StoredStatus::NotHaveSDCardDevice;
				printf("fileSystemCheck: sd_status = StoredStatus::NotHaveSDCardDevice\n");
			}
			else
			{
				// ���سɹ�
				if (checkSDCardMount(dev_path_ + "/" + dev_name_, mount_path_))
				{
					StoredStatus sd_status = StoredStatus::Ok;
					printf("fileSystemCheck: sd_status = StoredStatus::Ok\n");
					int free_size, total_size;
					//getFileSystemSize(mount_path_, free_size, total_size);
				}
				else
				{
					StoredStatus sd_status = StoredStatus::HaveDeviceButMountFailed;
					printf("fileSystemCheck: sd_status = StoredStatus::HaveDeviceButMountFailed\n");
				}
			}
		}

		std::string FileManager::removeAndGetNewName(std::string& prefix)
		{
			int free_size, total_size;
			//getFileSystemSize(mount_path_, free_size, total_size);
			int removeSucessFlag = 1;
			while (free_size < SINGLE_FILE_SIZE)
			{
				if (removeFile() < 0)
				{
					printf("remove file failed\n");
					removeSucessFlag = 0;
					break;
				}
				//getFileSystemSize(mount_path_, free_size, total_size);
			}

			if (removeSucessFlag)
			{
				return getNewTargetFileName(prefix);
			}
			else
			{
				return std::string("");
			}
		}

		std::string FileManager::monitorSpace()
		{
			//while (file_thread_status)
			//{
			//	int free_size = 0, total_size = 0;
			//	//getFileSystemSize(mount_path_, free_size, total_size);

			//	if (free_size < SINGLE_FILE_SIZE)
			//	{
			//		/*retry to save video, space monitored by hd only*/
			//		if (resume_store_hd_)
			//			resume_store_hd_();
			//		// if(resume_store_sd_)
			//		// 	resume_store_sd_();
			//	}
			//	else
			//	{
			//		/* do nothing */
			//	}
			//	std::this_thread::sleep_for(std::chrono::microseconds(10000000));
			//}
			return "";
		}

		void FileManager::setCbs(std::function<void()> func, bool hdflag)
		{
			if (hdflag)
				resume_store_hd_ = func;
			else
				resume_store_sd_ = func;
		}

		void FileManager::setFileThreadStatus(bool status)
		{
			file_thread_status = status;
		}

		FileManager::FileManager()
		{
		}

		void FileManager::getTime(std::uint16_t& year, std::uint16_t& mon, std::uint16_t& day,
			std::uint16_t& hour, std::uint16_t& minute, std::uint16_t& second)
		{
			time_t now;
			time(&now);
			struct tm* current_time = localtime(&now);
			year = current_time->tm_year - 100;
			mon = current_time->tm_mon + 1;
			day = current_time->tm_mday;
			hour = current_time->tm_hour;
			minute = current_time->tm_min;
			second = current_time->tm_sec;
		}

		std::string FileManager::getNewTargetFileName(std::string& prefix)
		{
			std::uint16_t year = 0, month = 0, day = 0;
			std::uint16_t hour = 0, minute = 0, second = 0;
			getTime(year, month, day, hour, minute, second);

			char cunrentNum[40];
			snprintf(cunrentNum, sizeof(cunrentNum), "%02d%02d%02d%02d%02d%02d.ts", (year % 2000), month, day, hour, minute, second);
			std::string file_name = mount_path_ + "/VIDEO/vid-" + prefix + "-f";
			file_name += std::string(cunrentNum);
			return file_name;
		}

		int FileManager::removeFile()
		{
			try
			{
				Poco::File file(mount_path_);
				if (file.exists())
				{
					if (file.isDirectory()) {
						Poco::DirectoryIterator it(file);
						Poco::DirectoryIterator end;

						while (it != end)
						{
							if (it->isFile()) // ֻɾ���ļ�����ɾ����Ŀ¼
							{
								try
								{
									it->remove();
								}
								catch (const Poco::Exception& exc)
								{
									std::cerr << "Error removing file: " << it.name() << " - " << exc.displayText() << std::endl;
								}
							}

							++it;
						}
					}
					else {
						file.remove();
					}
				}
			}
			catch (Poco::Exception& exc)
			{
				std::cerr << "Error deleting directory: " << exc.displayText() << std::endl;
			}

			return 0;
		}

		int FileManager::getFileCount()
		{
			int count = 0;

			// ����һ��DirectoryIteratorʵ��������Ŀ¼
			Poco::DirectoryIterator it(mount_path_);
			Poco::DirectoryIterator end;

			while (it != end)
			{
				// ����"."��".."
				if (it.name() != "." && it.name() != "..")
				{
					// ����Ƿ�Ϊ�ļ�������Ŀ¼��?
					if (it->isFile())
					{
						++count;
					}
				}

				++it;
			}

			return count;
		}

	}
}