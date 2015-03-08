/********************************************************
 * Description : main of daemon
 * Data        : 2015-02-06 11:30:00
 * Author      : yanrk
 * Email       : yanrkchina@hotmail.com
 * Blog        : blog.csdn.net/cxxmaker
 * Version     : 1.0
 * History     :
 * Copyright(C): 2015 - 2017
 ********************************************************/

#include <string>
#include <iostream>
#include "net/utility/net_switch.h"
#include "daemon.h"
#include "utility.h"
#include "base/filesystem/directory.h"
#include "base/log/log.h"
#include "base/utility/log_switch.h"
#include "tool/utility/curl_switch.h"

static bool get_root_directory(const char * file, std::string & path)
{
    if (!Stupid::Base::stupid_extract_directory(file, path, true))
    {
        std::cout << "extract directory failed" << std::endl;
        return(false);
    }

    if (!Stupid::Base::stupid_set_current_work_directory(path))
    {
        std::cout << "set current work directory failed" << std::endl;
        return(false);
    }

    if (!Stupid::Base::stupid_get_current_work_directory(path))
    {
        std::cout << "get current work directory failed" << std::endl;
        return(false);
    }

    return(true);
}

int main(int argc, char * argv[])
{
    size_t unique_id = 0;
    if (!exclusive_init("daemon", unique_id))
    {
        return(false);
    }

    std::string current_work_directory;
    if (!get_root_directory(argv[0], current_work_directory))
    {
        std::cout << "get root directory failed" << std::endl;
        return(1);
    }

    const std::string log_config(current_work_directory + "cfg/log.ini");
    if (!Stupid::Base::Singleton<Stupid::Base::LogSwitch>::instance().init(log_config.c_str()))
    {
        std::cout << "log init failed" << std::endl;
        return(2);
    }

    if (!Stupid::Base::Singleton<Stupid::Net::NetSwitch>::instance().init())
    {
        RUN_LOG_ERR("net switch init failed");
        return(3);
    }

    Stupid::Base::Singleton<Stupid::Tool::CurlSwitch>::instance().work();

    if (!Stupid::Base::Singleton<Daemon>::instance().init(current_work_directory))
    {
        RUN_LOG_ERR("daemon init failed");
        return(4);
    }

    std::cout << "daemon start success, input \"exit\" to stop it" << std::endl;

    while (true)
    {
        std::cin.sync();
        std::cin.clear();
        std::string command;
        std::cin >> command;
        if ("exit" == command)
        {
            break;
        }
    }

    Stupid::Base::Singleton<Daemon>::instance().exit();
    Stupid::Base::Singleton<Stupid::Net::NetSwitch>::instance().exit();
    Stupid::Base::Singleton<Stupid::Base::LogSwitch>::instance().exit();

    exclusive_exit(unique_id);

    return(0);
}
