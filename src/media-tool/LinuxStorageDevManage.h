#ifdef ENABLE_AIRBORNE

#pragma once
#ifndef _LinuxStorageDevManage_
#define _LinuxStorageDevManage_
#include <string.h>
#include <sstream>
#include <iostream>
#include <dirent.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <memory>
#include <thread>
#include <atomic>
/**
 * @brief linux存储设备管理
 * @todo:
 *        1.挂载存储设备
 *        2.实时查询剩余容量
 *        3.容量不足时删除旧文件
 * @author iux
 */
enum class StoredStatus
{
  Ok,
  NotHaveSDCardDevice,
  MountSucceedButDeleteFileFailedAndLessThan200M,
  MountSucceedButDeleteFileFailedAndGreaterThan200MAndLessThan8G,
  HaveDeviceButMountFailed
};

class LinuxStorageDevManage
{
  enum FORMAT_TYPE
  {
    ext4,
    ntfs
  };

public:
  /**
   * @brief 构造函数
   * @param dev 存储设备名
   * @param mountpoint 挂载点(路径)
   */
  LinuxStorageDevManage(std::string dev, std::string mountpoint);

  /**
   * @brief Destroy the Storage Dev Manage object
   *
   */
  ~LinuxStorageDevManage();

  /**
   * @brief 存储设备挂载
   * @param dev        存储设备
   * @param mountpoint 挂载点
   */
  bool mountDev(std::string dev, std::string mountpoint);

  /**
   * @brief 获取存储卡存储容量状态
   *
   * @param mountpoint 存储卡挂载点
   * @param free_size  剩余容量
   * @param total_size 总容量
   * @param used_size  已经使用容量
   */
  void getMemoryCapacityStatus(std::string mountpoint, int &used_size, int &free_size, int &total_size);

  /**
   * @brief 内存卡格式化
   *
   * @param dev 存储卡设备名
   * @param format  格式化ext4 ntfs等格式
   * @return true
   * @return false
   */
  bool memoryFormat(std::string dev, FORMAT_TYPE format);

  /**
   * @brief 设置自动文件清理功能
   *
   * @param flag 自动清理标识 true 开启清理  false 无需清理
   * @param filetype 文件类型
   */
  void setautoClearfileflag(bool flag, std::string filepath, std::string filetype);

  /**
   * @brief 检测存储设备挂载状态
   *
   * @param device_name  存储设备
   * @param mount_path   挂载点
   * @return true
   * @return false
   */
  bool checkMountStatus(std::string dev, std::string mountpoint);

public:
  bool MemoryReady = false; // 存储卡是否可用
  int MemoryTotol{};        // 存储设备总容量
  int MemoryUse{};          // 已经使用容量
  int MemoryFree{};         // 剩余容量
  int MemoryLimit = 8096;    // 容量限制 , 8096 1530
  int DeleteThresh = 200;

private:
  bool MemoryMountStatus = false;   // flase:未挂载，true:已经挂载
  std::string _dev{};               // 存储设备
  std::string _mountpoint{};        // 挂载点
  FORMAT_TYPE _format{};            // 存储卡格式
  bool _autoClearfileflag = false;  // 自动清理文件标识
  std::string _autoclearfilepath{}; // 自动清理文件路径
  std::string _autoclearfiletype{}; // 自动清理文件类型
  std::shared_ptr<std::thread> monitor_thread;
  std::string _mk_path_video = "/mnt/sdcard/VIDEO";
  std::string _mk_path_query = "/mnt/sdcard/query";
  int removefilecnt = 1; //删除的文件个数

private:
  

  /**
   * @brief 存储卡读写检测
   *
   * @param mountpoint 挂载点
   * @return true
   * @return false
   */
  bool memoryReadWriteTest(std::string mountpoint);

  /**
   * @brief 存储卡状态监控
   *
   * @param dev 存储卡设备名
   */
  void memoryStatusMonitor(std::string dev);

  /**
   * @brief 删除某类型文件
   *
   * @param filepath 文件路径
   * @param filename 文件名(通配名)
   * @return true
   * @return false
   */
  bool removeSomeFile(std::string filepath, std::string filename);

  /**
   * @brief 获取某路径下的文件数量
   *
   * @param filepath 文件路径
   * @return int 文件数量
   */
  int getFileCount(std::string filepath);
  bool checkPathExistAndCreatedIt(std::string path);
  // 比较函数，用于按创建时间排序
  static bool compareByCreationTime(const struct dirent* a, const struct dirent* b);
  bool remountry(std::string dev, std::string mountpoint);
  int retrytime = 2; //重新尝试挂载的时间，单位：分钟
  // std::shared_ptr<std::thread> remountry_thread_;
  std::thread remountry_thread_;
};
#endif

#endif // ENABLE_AIRBORNE