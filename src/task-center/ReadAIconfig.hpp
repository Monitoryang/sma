#ifndef READAICONFIG_HPP
#define READAICONFIG_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

// 添加缺失的using声明或者使用std::
using std::string;
using std::fstream;
using std::cout;
using std::endl;

struct DetectAiCfg
{
    std::string EnginePath = "/home/jouav/JOUAV/AiConfig/JoSmaDetectAiConfig.engine";
    std::string YoloVersion = "Yolov8";
    std::string InputWidth = "1280";
    std::string InputHeight = "736";
    std::string ClassNumber = "2";
    std::string ConfThresh = "0.35";
    std::string FireConfThresh = "0.35";
    std::string SmokeConfThresh = "0.35";
    std::string NmsThresh = "0.45";


    void print() const {
        eap_information_printf("EnginePath: %s", EnginePath);
        eap_information_printf("YoloVersion: %s", YoloVersion);
        eap_information_printf("InputWidth: %s", InputWidth);
        eap_information_printf("InputHeight: %s", InputHeight);
        eap_information_printf("ClassNumber: %s", ClassNumber);
        eap_information_printf("ConfThresh: %s", ConfThresh);
        eap_information_printf("FireConfThresh: %s", FireConfThresh);
        eap_information_printf("SmokeConfThresh: %s", SmokeConfThresh);
        eap_information_printf("NmsThresh: %s", NmsThresh);

        // std::cout << "EnginePath: " << EnginePath << std::endl;
        // std::cout << "YoloVersion: " << YoloVersion << std::endl;
        // std::cout << "InputWidth: " << InputWidth << std::endl;
        // std::cout << "InputHeight: " << InputHeight << std::endl;
        // std::cout << "ClassNumber: " << ClassNumber << std::endl;
        // std::cout << "ConfThresh: " << ConfThresh << std::endl;
        // std::cout << "FireConfThresh: " << FireConfThresh << std::endl;
        // std::cout << "SmokeConfThresh: " << SmokeConfThresh << std::endl;
        // std::cout << "NmsThresh: " << NmsThresh << std::endl;
    }
};

// 内联函数避免多重定义
inline bool readConfigFile(string &key, string &value, const char *path)
{
    const char *cfgfilepath = path;
    fstream cfgFile;
    cfgFile.open(cfgfilepath);
    if (!cfgFile.is_open())
    {
        cout << "can not open base cfg file!" << endl;
        return false;
    }

    char tmp[1000];
    while (!cfgFile.eof())
    {
        cfgFile.getline(tmp, 1000);
        string line(tmp);
        size_t pos = line.find('=');
        if (pos == string::npos)
            continue;  // 改为continue，不是return false
        
        string tmpKey = line.substr(0, pos);
        // 去除key前后的空白字符
        size_t start = tmpKey.find_first_not_of(" \t");
        size_t end = tmpKey.find_last_not_of(" \t");
        if (start != string::npos && end != string::npos) {
            tmpKey = tmpKey.substr(start, end - start + 1);
        }
        
        if (key == tmpKey)
        {
            value = line.substr(pos + 1);
            // 去除value前后的空白字符和回车
            size_t val_start = value.find_first_not_of(" \t\r");
            size_t val_end = value.find_last_not_of(" \t\r");
            if (val_start != string::npos && val_end != string::npos) {
                value = value.substr(val_start, val_end - val_start + 1);
            }
            return true;
        }
    }
    return false;
}

// 内联函数避免多重定义
inline DetectAiCfg getDetectAiCfg()
{
    std::string cfgfilepath = "/home/jouav/JOUAV/AiConfig/JoSmaDetectAiConfig.ini";
    DetectAiCfg txCfg;
    bool rdflag = false;
    std::string KEY;

    // 使用lambda函数减少重复代码
    auto readConfig = [&](const std::string& key, std::string& value, const std::string& description) {
        rdflag = readConfigFile(KEY, value, cfgfilepath.c_str());
    };

    KEY = "EnginePath";
    readConfig(KEY, txCfg.EnginePath, "EnginePath");

    KEY = "YoloVersion";
    readConfig(KEY, txCfg.YoloVersion, "YoloVersion");

    KEY = "InputWidth";
    readConfig(KEY, txCfg.InputWidth, "InputWidth");

    KEY = "InputHeight";
    readConfig(KEY, txCfg.InputHeight, "InputHeight");

    KEY = "ClassNumber";
    readConfig(KEY, txCfg.ClassNumber, "ClassNumber");

    KEY = "ConfThresh";
    readConfig(KEY, txCfg.ConfThresh, "ConfThresh");

    KEY = "FireConfThresh";
    readConfig(KEY, txCfg.FireConfThresh, "FireConfThresh");

    KEY = "SmokeConfThresh";
    readConfig(KEY, txCfg.SmokeConfThresh, "SmokeConfThresh");

    KEY = "NmsThresh";
    readConfig(KEY, txCfg.NmsThresh, "NmsThresh");

    return txCfg;
}

#endif // READAICONFIG_HPP