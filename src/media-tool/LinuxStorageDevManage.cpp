#ifdef ENABLE_AIRBORNE
#include "LinuxStorageDevManage.h"
#include "Logger.h"
#include <mutex>
#include <functional>
#include <unistd.h>
#include "LinuxStorageDevManage.h"

LinuxStorageDevManage::LinuxStorageDevManage(std::string dev, std::string mountpoint)
{
  _dev = "/dev/" + dev;
  _mountpoint = mountpoint;
  MemoryMountStatus = mountDev(_dev, _mountpoint);
  if (MemoryMountStatus)
  {
    checkPathExistAndCreatedIt(_mk_path_video);
    checkPathExistAndCreatedIt(_mk_path_query);
    monitor_thread = std::make_shared<std::thread>(&LinuxStorageDevManage::memoryStatusMonitor, this, dev);
  }
}

LinuxStorageDevManage::~LinuxStorageDevManage()
{
}

bool LinuxStorageDevManage::checkMountStatus(std::string dev, std::string mountpoint)
{
  bool mount_flag = false;
  // 定义文件指针
  FILE *fp = NULL;
  char buf[500];
  std::string tmpPath[3];
  // std::stringstream stream;
  std::string s;
  int len;
  int mountok;
  // 列出已挂载的文件系统 按照设备名 文件系统类型 以及挂载目录显示
  std::string op_f = std::string("mount|grep ") + dev;
  // 以读入方式 执行op_f命令
  fp = popen(op_f.c_str(), "r");
  if (!fp)
  {
    perror("popen");
    return false;
  }
  // eap_information_printf("op_f: %s", op_f);
  memset(buf, 0, sizeof(buf));
  // 按行读取从指定的数据流fp读取数据，并存入buf中
  fgets(buf, sizeof(buf), fp);
  std::stringstream stream;
  // 利用<< 将数据传递给stream
  stream << buf;
  std::string str = stream.str();
  std::istringstream iss(str);
  for (int i = 0; i < 3; ++i)
  {
    // 用于分割被空格分割的字符串
    iss >> tmpPath[i];
  }
  pclose(fp);
  if (fp != NULL)
  {
    fp = NULL;
  }
  len = tmpPath[2].length();
  std::string s1 = tmpPath[2];
  // eap_information_printf("s1: %s, mountpoint: %s", s1, mountpoint);
  if (NULL != strstr(s1.c_str(), mountpoint.c_str()))
  {
    mount_flag = true;
  }

  return mount_flag;
}

bool LinuxStorageDevManage::mountDev(std::string dev, std::string mountpoint)
{
  /**
   * 1.检查设备挂载状态
   * 2.未挂载时，挂载设备
   * 3.测试文件读写是否正常
   */

  // 1.检查设备挂载状态
  bool mountstatus = false;
  bool mountok = false;
  mountstatus = checkMountStatus(dev, mountpoint);

  if (mountstatus)
  {
    if (memoryReadWriteTest(mountpoint))
    {
      eap_information("SD mount success! can read or write");
    }
    else
    {
      eap_warning("SD mount success but can't write!");
    }
  }
  else
  {
    char sysSDmount[500];
    sprintf(sysSDmount, "umount %s", dev.c_str());
    system(sysSDmount);
    if (0 == system(std::string("mount " + dev + " " + mountpoint).c_str()))
    {
      mountok = checkMountStatus(dev, mountpoint);
      if (mountok)
      {
        if (memoryReadWriteTest(mountpoint))
        {
          mountok = true;
          eap_information("SD Remount success!");
        }
        else
        {
          mountok = false;
          eap_warning("SD Remount success but can't write!");
        }
      }
      else
      {
        eap_error("SD Remount failed!");
      }
    }
    else
    {
      mountok = false;
      mountok = remountry(dev, mountpoint);
      // remountry_thread_ = std::make_shared<std::thread>(&LinuxStorageDevManage::remountry, this);
      std::shared_ptr<std::thread> remountry_thread_ = std::make_shared<std::thread>(
      std::bind(&LinuxStorageDevManage::remountry, this, dev, mountpoint));

      // 等待线程完成并获取结果
      remountry_thread_->join();
      eap_error("SD Remount failed again!");
    }
    mountstatus = mountok;
  }

  if (mountstatus)
  {
    getMemoryCapacityStatus(mountpoint, MemoryUse, MemoryFree, MemoryTotol);
    eap_information_printf("MemoryDevname : %s, MemoryMountPoint : %s", dev, mountpoint);
    eap_information_printf("MemoryTotol : %f,MemoryFree :%f", float(MemoryTotol) / 1024.0, float(MemoryFree) / 1024.0);
  }

  return mountstatus;
}

bool LinuxStorageDevManage::remountry(std::string dev, std::string mountpoint) 
{
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::minutes(retrytime);  // 默认两分钟

    while (std::chrono::steady_clock::now() < end_time) 
    {
        bool mountok = checkMountStatus(dev, mountpoint);
        eap_information_printf("remountry---mountok status: %d", mountok);
        std::cout << "Mount status: " << std::boolalpha << mountok << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));  // 暂停2秒
        if (mountok) {
            return mountok;
        }
    }
    return false;
}

bool LinuxStorageDevManage::memoryFormat(std::string dev, FORMAT_TYPE format)
{
  std::string cmd;
  if (format == ext4)
  {
    cmd = "mkfs.ext4 " + dev;
  }
  else
  {
    cmd = "mkntfs -f " + dev;
  }
  system(cmd.c_str());
  eap_information(cmd);
}

bool LinuxStorageDevManage::memoryReadWriteTest(std::string mountpoint)
{
  bool write_flag = 0;
  bool create_flag = 0;
  bool final_flag = 0;
  int file_data;
  std::string filepath = mountpoint + std::string("/test.txt");
  std::string filename = std::string("touch ") + filepath;
  system(filename.c_str());
  std::string filewrite = std::string("echo 123456789 > ") + filepath;
  system(filewrite.c_str());

  FILE *pfile = fopen(filepath.c_str(), "r+t");
  std::string rmfile = "rm " + filepath;
  // 打开挂载路径并创建test.txt文本文件，写入123456789，如果成功写入则返回write_flag=1，没有成功writeflag=0（检测是否能成功写入挂载路径）
  if (pfile != NULL)
  {
    fscanf(pfile, "%d", &file_data);
    if (file_data == 123456789)
    {
      write_flag = 1;
      fclose(pfile);
      system(rmfile.c_str());
      eap_information_printf("***********create file %s success", filepath);
    }
    else
    {
      write_flag = 0;
      fclose(pfile);
      system("rm /mnt/sdcard/VIDEO/test.txt");
      eap_information_printf("***********create file %s failed", filepath);
    }
  }
  else
  {
    write_flag = 0;
    eap_warning_printf("***********create file %s failed", filepath);
  }
  // 检查是否能成功创建.ts文件
  std::string video_file_name = mountpoint + "/video.ts";
  FILE *out_file_ = fopen(video_file_name.c_str(), "wb");

  if (out_file_)
  {
    create_flag = 1;
    fclose(out_file_);
    std::string rmvideofile = "rm " + video_file_name;
    system(rmvideofile.c_str());
    eap_information_printf("***********create file %s success", video_file_name);
  }
  else
  {
    create_flag = 0;
    eap_warning_printf("***********create file %s failed", video_file_name);
  }
  // 能成功写入文件以及成功创建.ts文件
  if ((create_flag) == 1 && (write_flag == 1))
  {
    return true;
  }
  else
  {
    return false;
  }
}

void LinuxStorageDevManage::memoryStatusMonitor(std::string dev)
{
  while (1)
  {
    getMemoryCapacityStatus(_mountpoint, MemoryUse, MemoryFree, MemoryTotol);
    if (MemoryFree < MemoryLimit)
    {
      if (MemoryFree > DeleteThresh)
      {
        MemoryReady = true;
      }
      else
      {
        MemoryReady = false;
        if (_autoClearfileflag)
        {
          removeSomeFile(_autoclearfilepath, _autoclearfiletype);
        }
      }
    }
    else
    {
      MemoryReady = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void LinuxStorageDevManage::getMemoryCapacityStatus(std::string mountpoint, int &used_size, int &free_size, int &total_size)
{
  /*
   * todo:
   *    1.查询卡的容量
   *    2.剩余容量小于限制值时，清楚旧文件，且文件类型限定为.ts文件
   */
  std::string path = mountpoint;
  std::mutex statfs_mutex_;
  statfs_mutex_.lock();
  struct statfs myStatfs;
  if (statfs(path.c_str(), &myStatfs) == -1)
  {
    return;
  }
  else
  {
    free_size = (((long long)myStatfs.f_bsize * (long long)myStatfs.f_bfree) / (long long)1024 / (long long)1024);
    total_size = (((long long)myStatfs.f_bsize * (long long)myStatfs.f_blocks) / (long long)1024 / (long long)1024);
    used_size = total_size - free_size;
  }
  statfs_mutex_.unlock();
}

// 实现compareByCreationTime函数
bool LinuxStorageDevManage::compareByCreationTime(const struct dirent* a, const struct dirent* b) 
{
    struct stat stat_a, stat_b;
    stat(a->d_name, &stat_a);
    stat(b->d_name, &stat_b);
    // std::cout << "File: " << a->d_name << ", creation Time: " << stat_a.st_ctime << std::endl;
    // std::cout << "File: " << b->d_name << ", creation Time: " << stat_b.st_ctime << std::endl;
    // return stat_a.st_mtime < stat_b.st_mtime;
    return stat_a.st_ctime < stat_b.st_ctime;
}
bool LinuxStorageDevManage::removeSomeFile(std::string filepath, std::string file)
{
  DIR *pdir;
  struct dirent *pdirent;
  struct stat f_ftime;
#define FILE_IDX_LEN 12
  int i;
  long long int *pFileIdx;
  int filecnt;
  long long int rm_idx;
  int ret;
  filecnt = getFileCount(filepath);
  if (filecnt <= 3)
    return -1;

  pFileIdx = (long long int *)malloc(filecnt * sizeof(long long int));
  memset(pFileIdx, 0, filecnt * sizeof(long long int));

  i = 0;
  pdir = opendir(filepath.c_str());
  for (pdirent = readdir(pdir); pdirent != NULL; pdirent = readdir(pdir))
  {
    if ((strcmp(pdirent->d_name, ".") == 0) || (strcmp(pdirent->d_name, "..") == 0))
      continue;

    std::string file_name = filepath + "/" + pdirent->d_name;
    if (stat(file_name.c_str(), &f_ftime) != 0)
    {
      closedir(pdir);
      return -1;
    }

    if (S_ISDIR(f_ftime.st_mode))
      continue;

    if ((NULL != strstr(pdirent->d_name, file.c_str())))
    {
      std::string file_name = std::string(pdirent->d_name);
      std::string substring = file.c_str();
      std::string idx_str = file_name.substr(substring.length(), (size_t)FILE_IDX_LEN);
      pFileIdx[i] = atoll(idx_str.c_str());
      i++;
    }
  }

  switch (i)
  {
  case 0:
  case 1:
    ret = -1;
    break;
  case 2:
    ret = -1;
    break;
  case 3:
    ret = -1;
    break;
  default:
    /*total video files count: i*/
    std::sort(pFileIdx, pFileIdx + i);
    rm_idx = pFileIdx[0];
    ret = 0;
    break;
  }
  free(pFileIdx);
  closedir(pdir);
  if (ret != -1)
  {
    pdir = opendir(filepath.c_str());
    int RemoveFlag = 0;
    for (pdirent = readdir(pdir); pdirent != NULL; pdirent = readdir(pdir))
    {
      if ((strcmp(pdirent->d_name, ".") == 0) || (strcmp(pdirent->d_name, "..") == 0))
        continue;
      std::string file_name = filepath + "/" + pdirent->d_name;
      if (stat(file_name.c_str(), &f_ftime) != 0)
      {
        closedir(pdir);
        return -1;
      }

      if (S_ISDIR(f_ftime.st_mode))
        continue;

      // 2024-08-30
      std::vector<struct dirent*> files;
      struct dirent* entry;
      while ((entry = readdir(pdir)) != nullptr) 
      {
        if (entry->d_type == DT_REG) 
        { // 检查是否为普通文件
          files.push_back(entry);
        }
      }
      closedir(pdir);

      // 按创建时间排序
      std::sort(files.begin(), files.end(), compareByCreationTime);

      // 删除文件
      for (const auto& file : files) 
      {
        std::cout << "File: " << file->d_name << std::endl;
        if (removefilecnt <= 3)
        {
          if (remove(file_name.c_str()) == 0) 
          {
            eap_information_printf("delete: {}", file_name );
            
            removefilecnt++;
          } 
        }
        else 
        {
          return -1;
        }

        //如果内存超过最低阈值，停止删文件
        getMemoryCapacityStatus(_mountpoint, MemoryUse, MemoryFree, MemoryTotol);
        if (MemoryFree > DeleteThresh)
        {
          return -1;
        }
          
      }

      // if (NULL != strstr(pdirent->d_name, std::to_string(rm_idx).c_str()))
      // {
      //   if (remove(file_name.c_str()) == 0 && removefilecnt <= 2)
      //   {
      //     RemoveFlag = 1;
      //     removefilecnt++;
      //     eap_information_printf("romovefile: {}", file_name.c_str());
      //   }
      //   else
      //   {
      //     return -1;;
      //   }
      // }
      
    }
    // closedir(pdir);

    if (RemoveFlag != 1)
    {
      ret = -1;
    }
  }

  return ret;
}


int LinuxStorageDevManage::getFileCount(std::string filepath)
{
  DIR *pdir;
  struct dirent *pdirent;
  struct stat f_ftime;
  int fcnt;
  
  pdir = opendir(filepath.c_str());
  if (pdir == NULL)
  {
    return -1;
  }

  fcnt = 0;
  for (pdirent = readdir(pdir); pdirent != NULL; pdirent = readdir(pdir))
  {
    if ((strcmp(pdirent->d_name, ".") == 0) || (strcmp(pdirent->d_name, "..") == 0))
      continue;

    std::string file_name = filepath + "/" + pdirent->d_name;

    if (stat(file_name.c_str(), &f_ftime) != 0)
    {
      closedir(pdir);
      eap_information_printf("err: {}", errno);
      return -1;
    }

    if (S_ISDIR(f_ftime.st_mode))
      continue;
    fcnt++;
  }
  closedir(pdir);
  return fcnt;
}

void LinuxStorageDevManage::setautoClearfileflag(bool flag, std::string filepath, std::string filetype)
{
  _autoClearfileflag = flag;
  _autoclearfiletype = filetype;
  _autoclearfilepath = filepath + "/VIDEO"; // 自动清理文件类型
}

bool LinuxStorageDevManage::checkPathExistAndCreatedIt(std::string path)
{
  if (0 == access(path.c_str(), 00))
  {
    eap_information_printf("exist {}", path.c_str());
  }
  else
  {
    std::string cmd = "mkdir " + path;
    if (0 == system(cmd.c_str()))
    {
      if (0 == access(path.c_str(), 00))
      {
        eap_information_printf("creat %s success", path);
      }
      else
      {
        eap_information_printf("creat %s failed", path);
      }
    }
  }
}
#endif // ENABLE_AIRBORNE