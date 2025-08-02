/********************************************************
 * Description : daemon class
 * Data        : 2015-02-05 09:30:00
 * Author      : yanrk
 * Email       : yanrkchina@hotmail.com
 * Blog        : blog.csdn.net/cxxmaker
 * Version     : 1.0
 * History     :
 * Copyright(C): 2015 - 2017
 ********************************************************/

#include <fstream>
#include "net/utility/tcp.h"
#include "net/utility/utility.h"
#include "daemon.h"
#include "utility.h"
#include "base/log/log.h"
#include "base/time/time.h"
#include "base/config/xml.h"
#include "base/string/string.h"
#include "base/filesystem/directory.h"

struct ServiceInfo
{
    bool                     show;
    std::string              host;
    std::list<std::string>   ports;
    std::string              path;
    std::string              file;
    std::list<std::string>   params;
    std::string              cmdl;
};

static bool parse_item(const std::string & service_conf, ServiceInfo & service_info)
{
    Stupid::Base::Xml xml;

    if (!xml.set_document(service_conf.c_str()))
    {
        RUN_LOG_CRI("set document failed, content:{%s}", service_conf.c_str());
        return false;
    }

    if (!xml.into_element("service"))
    {
        RUN_LOG_CRI("into element <%s> failed", "service");
        return false;
    }

    std::string show;
    if (!xml.get_element("show", show) || !Stupid::Base::stupid_string_to_type(show, service_info.show))
    {
        service_info.show = false;
    }

    if (!xml.get_element("host", service_info.host) || service_info.host.empty())
    {
        service_info.host = "127.0.0.1";
    }

    xml.get_element_block("ports", "port", true, service_info.ports);

    if (!xml.get_element("path", service_info.path))
    {
        RUN_LOG_ERR("get element <%s> failed", "path");
        return false;
    }
    Stupid::Base::stupid_string_trim(service_info.path, "\"");
    if (service_info.path.empty())
    {
        RUN_LOG_ERR("<path> is empty");
        return false;
    }
    if ('/' != *service_info.path.rbegin() && '\\' != *service_info.path.rbegin())
    {
        service_info.path += '/';
    }

    if (!xml.get_element("file", service_info.file))
    {
        RUN_LOG_ERR("get element <%s> failed", "file");
        return false;
    }
    Stupid::Base::stupid_string_trim(service_info.file, "\"");
    if (service_info.file.empty())
    {
        RUN_LOG_ERR("<file> is empty");
        return false;
    }

    service_info.cmdl = service_info.path + service_info.file;

#ifdef _MSC_VER
    if (std::string::npos != service_info.cmdl.find(" "))
    {
        service_info.cmdl = "\"" + service_info.cmdl + "\"";
    }
#endif // _MSC_VER

    xml.get_element_block("params", "param", true, service_info.params);
    std::string params;
    Stupid::Base::stupid_piece_together(service_info.params.begin(), service_info.params.end(), " ", params);
    if (!params.empty())
    {
        service_info.cmdl += " " + params;
    }

    return true;
}

static bool load_services(const std::string & root_directory, std::list<ServiceInfo> & service_info_list)
{
    const std::string config_file(root_directory + "cfg/config.xml");

    Stupid::Base::Xml xml;

    if (!xml.load(config_file.c_str()))
    {
        RUN_LOG_CRI("load failed, filename:{%s}", config_file.c_str());
        return false;
    }

    if (!xml.into_element("root"))
    {
        RUN_LOG_CRI("into element <%s> failed", "root");
        return false;
    }

    if (!xml.into_element("services"))
    {
        RUN_LOG_CRI("into element <%s> failed", "services");
        return false;
    }

    std::string sub_document;
    while (xml.get_sub_document(sub_document))
    {
        ServiceInfo service_info;
        if (parse_item(sub_document, service_info))
        {
            service_info_list.push_back(service_info);
        }
    }

    if (!xml.outof_element())
    {
        RUN_LOG_ERR("out of element failed");
        return false;
    }

    if (!xml.outof_element())
    {
        RUN_LOG_ERR("out of element failed");
        return false;
    }

    return true;
}

static void get_check_interval(const std::string & root_directory, uint64_t & check_interval_seconds)
{
    const uint64_t min_interval_seconds = 3;
    const uint64_t def_interval_seconds = 30;
    const uint64_t max_interval_seconds = 300;

    check_interval_seconds = def_interval_seconds;

    const std::string config_file(root_directory + "cfg/config.xml");

    Stupid::Base::Xml xml;

    if (!xml.load(config_file.c_str()))
    {
        RUN_LOG_ERR("load failed, filename:{%s}", config_file.c_str());
        return;
    }

    if (!xml.find_element("root"))
    {
        RUN_LOG_ERR("find element <%s> failed", "root");
        return;
    }

    std::string check_interval;
    xml.get_child_element("check_interval", check_interval);
    if (!check_interval.empty())
    {
        Stupid::Base::stupid_string_to_type(check_interval, check_interval_seconds);
    }

    if (check_interval_seconds < min_interval_seconds)
    {
        check_interval_seconds = min_interval_seconds;
    }
    else if (check_interval_seconds > max_interval_seconds)
    {
        check_interval_seconds = max_interval_seconds;
    }
}

static void append_record_content(const std::string & record_file, const std::string & record_content)
{
    std::ofstream ofs(record_file.c_str(), std::ios::app);
    if (ofs.is_open())
    {
        ofs << Stupid::Base::stupid_get_datetime() << " " << record_content << std::endl;
        ofs.close();
    }
    else
    {
        RUN_LOG_ERR("append record content failed: {%s}", record_content.c_str());
    }
}

Daemon::Daemon()
    : m_running(false)
    , m_root_directory()
    , m_record_file()
    , m_last_check_time(0)
    , m_check_interval(0)
    , m_process_info_map()
    , m_check_timer()
{

}

Daemon::~Daemon()
{

}

bool Daemon::init(const std::string & current_work_directory)
{
    exit();

    m_running = true;

    m_root_directory = current_work_directory;

    m_record_file = m_root_directory + "log/record/";
    Stupid::Base::stupid_create_directory_recursive(m_record_file);
    m_record_file += Stupid::Base::stupid_get_date() + ".txt";

    get_check_interval(m_root_directory, m_check_interval);

    if (!m_check_timer.init(this, 30))
    {
        RUN_LOG_CRI("check timer init failed");
        return false;
    }

    append_record_content(m_record_file, "--------- daemon init ---------");

    RUN_LOG_DBG("daemon init success");

    return true;
}

void Daemon::exit()
{
    if (!m_running)
    {
        return;
    }

    m_running = false;

    m_check_timer.exit();

    append_record_content(m_record_file, "--------- daemon exit ---------");

    RUN_LOG_DBG("daemon exit success");
}

void Daemon::on_timer(bool first_time, size_t index)
{
    if (Stupid::Base::stupid_time() < m_last_check_time + m_check_interval)
    {
        return;
    }

    std::list<ServiceInfo> service_info_list;
    if (!load_services(m_root_directory, service_info_list))
    {
        RUN_LOG_ERR("load services failed");
        return;
    }

    for (std::list<ServiceInfo>::const_iterator iter = service_info_list.begin(); service_info_list.end() != iter; ++iter)
    {
        std::map<std::string, ProcessInfo>::iterator iter_proc = m_process_info_map.find(iter->cmdl);
        bool service_is_ok = true;

        if (iter->ports.empty())
        {
            std::string process_name;
            if (m_process_info_map.end() != iter_proc)
            {
                process_name = iter_proc->second.name;
            }
            else
            {
#ifdef _MSC_VER
                process_name = iter->file;
#else
                process_name = iter->cmdl;
#endif // _MSC_VER
            }
            if (!is_process_alive(process_name))
            {
                RUN_LOG_DBG("service {%s} is not alive", iter->cmdl.c_str());
                service_is_ok = false;
            }
        }
        else
        {
            for (std::list<std::string>::const_iterator iter_port = iter->ports.begin(); iter->ports.end() != iter_port; ++iter_port)
            {
                socket_t connecter = BAD_SOCKET;
                if (!Stupid::Net::tcp_connect(iter->host.c_str(), iter_port->c_str(), connecter))
                {
                    RUN_LOG_DBG("service {%s} can not be connected on port %s", iter->cmdl.c_str(), iter_port->c_str());
                    service_is_ok = false;
                    break;
                }
                Stupid::Net::tcp_close(connecter);
            }
        }

        if (!service_is_ok)
        {
            if (m_process_info_map.end() != iter_proc)
            {
                RUN_LOG_DBG("stop service {%s} begin", iter->cmdl.c_str());
                kill_process(iter_proc->second.id, iter_proc->second.name);
                m_process_info_map.erase(iter_proc);
                RUN_LOG_DBG("stop service {%s} end", iter->cmdl.c_str());
                append_record_content(m_record_file, "process {" + iter->cmdl + "} is stop");
            }

            size_t process_id = 0;
            std::string process_name;
            if (create_process(iter->path, iter->cmdl, iter->show, process_id, process_name))
            {
                RUN_LOG_DBG("start service {%s} success", iter->cmdl.c_str());
                ProcessInfo process_info = { process_id, process_name };
                m_process_info_map[iter->cmdl] = process_info;
                append_record_content(m_record_file, "process {" + iter->cmdl + "} is start");
            }
            else
            {
                RUN_LOG_ERR("start service {%s} failure", iter->cmdl.c_str());
                append_record_content(m_record_file, "start process {" + iter->cmdl + "} failed");
            }
        }
    }

    m_last_check_time = Stupid::Base::stupid_time();
}
