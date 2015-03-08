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

#include "net/utility/tcp.h"
#include "net/utility/utility.h"
#include "daemon.h"
#include "utility.h"
#include "markup.h"
#include "base/log/log.h"
#include "base/string/string.h"
#include "base/time/time.h"

static bool markup_get_element(CMarkup & markup, const char * element_name, std::string & element_value, bool ignore_not_exist_error)
{
    if (nullptr == element_name)
    {
        RUN_LOG_ERR("element_name is nullptr");
        return(false);
    }

    if (!markup.FindChildElem(element_name))
    {
        if (!ignore_not_exist_error)
        {
            RUN_LOG_ERR("get element <%s> failed", element_name);
        }
        return(false);
    }

    element_value = markup.GetChildData();
    markup.ResetChildPos();

    Stupid::Base::stupid_string_trim(element_value);

    return(true);
}

static bool markup_get_block(CMarkup & markup, const char * block_name, const char * item_name, bool ignore_not_exist_error, bool ignore_empty_item, std::list<std::string> & item_value_list)
{
    if (nullptr == block_name)
    {
        RUN_LOG_ERR("block_name is nullptr");
        return(false);
    }

    if (nullptr == item_name)
    {
        RUN_LOG_ERR("item_name is nullptr");
        return(false);
    }

    if (!markup.FindChildElem(block_name))
    {
        if (!ignore_not_exist_error)
        {
            RUN_LOG_ERR("get block <%s> failed", block_name);
        }
        return(false);
    }

    if (!markup.IntoElem())
    {
        RUN_LOG_ERR("markup.intoelem failed");
        return(false);
    }

    while (markup.FindChildElem(item_name))
    {
        std::string item_value = markup.GetChildData();
        Stupid::Base::stupid_string_trim(item_value);
        if (!ignore_empty_item || !item_value.empty())
        {
            item_value_list.push_back(item_value);
        }
    }

    if (!markup.OutOfElem())
    {
        RUN_LOG_ERR("markup.outofelem failed");
        return(false);
    }

    markup.ResetChildPos();

    return(true);
}

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
    CMarkup markup;
    if (!markup.SetDoc(service_conf))
    {
        RUN_LOG_CRI("markup.setdoc failed, content:{%s}", service_conf.c_str());
        return(false);
    }

    if (!markup.FindElem("service"))
    {
        RUN_LOG_CRI("get root <%s> failed", "service");
        return(false);
    }

    std::string show;
    if (!markup_get_element(markup, "show", show, true) || !Stupid::Base::stupid_string_to_type(show, service_info.show))
    {
        service_info.show = false;
    }

    if (!markup_get_element(markup, "host", service_info.host, true) || service_info.host.empty())
    {
        service_info.host = "127.0.0.1";
    }

    markup_get_block(markup, "ports", "port", true, true, service_info.ports);

    if (!markup_get_element(markup, "path", service_info.path, false))
    {
        RUN_LOG_ERR("markup_get_element(path) failed");
        return(false);
    }
    Stupid::Base::stupid_string_trim(service_info.path, "\"");
    if (service_info.path.empty())
    {
        RUN_LOG_ERR("<path> is empty");
        return(false);
    }
    if ('/' != *service_info.path.rbegin() && '\\' != *service_info.path.rbegin())
    {
        service_info.path += '/';
    }

    if (!markup_get_element(markup, "file", service_info.file, false))
    {
        RUN_LOG_ERR("markup_get_element(file) failed");
        return(false);
    }
    Stupid::Base::stupid_string_trim(service_info.file, "\"");
    if (service_info.file.empty())
    {
        RUN_LOG_ERR("<file> is empty");
        return(false);
    }

    service_info.cmdl = service_info.path + service_info.file;

#ifdef _MSC_VER
    if (std::string::npos != service_info.cmdl.find(" "))
    {
        service_info.cmdl = "\"" + service_info.cmdl + "\"";
    }
#endif // _MSC_VER

    markup_get_block(markup, "params", "param", true, true, service_info.params);
    std::string params;
    Stupid::Base::stupid_piece_together(service_info.params.begin(), service_info.params.end(), " ", params);
    if (!params.empty())
    {
        service_info.cmdl += " " + params;
    }

    return(true);
}

static bool load_services(const std::string & root_directory, std::list<ServiceInfo> & service_info_list)
{
    const std::string config_file(root_directory + "cfg/config.xml");

    CMarkup markup;
    if (!markup.Load(config_file))
    {
        RUN_LOG_CRI("markup.load failed, filename:{%s}", config_file.c_str());
        return(false);
    }

    if (!markup.FindElem("root"))
    {
        RUN_LOG_CRI("get root <%s> failed", "root");
        return(false);
    }

    if (!markup.IntoElem())
    {
        RUN_LOG_ERR("markup.intoelem failed");
        return(false);
    }

    if (!markup.FindElem("services"))
    {
        RUN_LOG_CRI("get block <%s> failed", "services");
        return(false);
    }

    if (!markup.IntoElem())
    {
        RUN_LOG_ERR("markup.intoelem failed");
        return(false);
    }

    while (0 != markup.FindNode(CMarkup::MNT_ELEMENT))
    {
        ServiceInfo service_info;
        if (parse_item(markup.GetSubDoc(), service_info))
        {
            service_info_list.push_back(service_info);
        }
    }

    if (!markup.OutOfElem())
    {
        RUN_LOG_ERR("markup.outofelem failed");
        return(false);
    }

    if (!markup.OutOfElem())
    {
        RUN_LOG_ERR("markup.outofelem failed");
        return(false);
    }

    return(true);
}

static void construct_mail_user(std::string & mail_user)
{
    mail_user = "<" + mail_user + ">";
}

static void construct_mail_user_list(std::list<std::string> & mail_user_list)
{
    for (std::list<std::string>::iterator iter = mail_user_list.begin(); mail_user_list.end() != iter; ++iter)
    {
        construct_mail_user(*iter);
    }
}

static bool load_mail_info(const std::string & root_directory, bool & need_send_mail, Stupid::Tool::MailInfo & mail_info)
{
    const std::string config_file(root_directory + "cfg/config.xml");

    CMarkup markup;
    if (!markup.Load(config_file))
    {
        RUN_LOG_CRI("markup.load failed, filename:{%s}", config_file.c_str());
        return(false);
    }

    if (!markup.FindElem("root"))
    {
        RUN_LOG_CRI("get root <%s> failed", "root");
        return(false);
    }

    if (!markup.IntoElem())
    {
        RUN_LOG_ERR("markup.intoelem failed");
        return(false);
    }

    if (!markup.FindElem("mail"))
    {
        RUN_LOG_ERR("get block <%s> failed", "mail");
        return(false);
    }

    std::string send_mail;
    if (!markup_get_element(markup, "send_mail", send_mail, false))
    {
        return(false);
    }
    if (!Stupid::Base::stupid_string_to_type(send_mail, need_send_mail))
    {
        RUN_LOG_ERR("<send_mail> is invalid");
        return(false);
    }

    if (need_send_mail)
    {
        mail_info.m_verbose = false;

        if (!markup_get_element(markup, "username", mail_info.m_username, false))
        {
            return(false);
        }

        if (!markup_get_element(markup, "password", mail_info.m_password, false))
        {
            return(false);
        }

        if (!markup_get_element(markup, "nickname", mail_info.m_nickname, false))
        {
            return(false);
        }

        if (!markup_get_element(markup, "smtp_host", mail_info.m_smtp_host, false))
        {
            return(false);
        }

        std::string smtp_port;
        if (!markup_get_element(markup, "smtp_port", smtp_port, false))
        {
            return(false);
        }
        if (!Stupid::Base::stupid_string_to_type(smtp_port, mail_info.m_smtp_port))
        {
            RUN_LOG_ERR("<smtp_port> is invalid");
            return(false);
        }

        if (!markup_get_element(markup, "subject", mail_info.m_mail_subject, false))
        {
            return(false);
        }

        if (!markup_get_element(markup, "from", mail_info.m_mail_from, false))
        {
            return(false);
        }
        construct_mail_user(mail_info.m_mail_from);

        if (!markup_get_block(markup, "tos", "to", true, true, mail_info.m_mail_to_list))
        {
            return(false);
        }
        construct_mail_user_list(mail_info.m_mail_to_list);

        if (!markup_get_block(markup, "ccs", "cc", true, true, mail_info.m_mail_cc_list))
        {
            return(false);
        }
        construct_mail_user_list(mail_info.m_mail_cc_list);

        if (!markup_get_block(markup, "bccs", "bcc", true, true, mail_info.m_mail_bcc_list))
        {
            return(false);
        }
        construct_mail_user_list(mail_info.m_mail_bcc_list);

        if (mail_info.m_mail_to_list.empty() && mail_info.m_mail_cc_list.empty() && mail_info.m_mail_bcc_list.empty())
        {
            RUN_LOG_ERR("no mail receiver");
            return(false);
        }
    }

    if (!markup.OutOfElem())
    {
        RUN_LOG_ERR("markup.outofelem failed");
        return(false);
    }

    return(true);
}

static void get_check_interval(const std::string & root_directory, uint64_t & check_interval_seconds)
{
    const uint64_t min_interval_seconds = 3;
    const uint64_t def_interval_seconds = 30;
    const uint64_t max_interval_seconds = 300;

    check_interval_seconds = def_interval_seconds;

    const std::string config_file(root_directory + "cfg/config.xml");

    CMarkup markup;
    if (!markup.Load(config_file))
    {
        RUN_LOG_ERR("markup.load failed, filename:{%s}", config_file.c_str());
        return;
    }

    if (!markup.FindElem("root"))
    {
        RUN_LOG_ERR("get root <%s> failed", "root");
        return;
    }

    std::string check_interval;
    markup_get_element(markup, "check_interval", check_interval, true);
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

Daemon::Daemon()
    : m_running(false)
    , m_root_directory()
    , m_last_check_time(0)
    , m_check_interval(0)
    , m_process_info_map()
    , m_send_mail(false)
    , m_mail_info()
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

    get_check_interval(m_root_directory, m_check_interval);

    if (!load_mail_info(m_root_directory, m_send_mail, m_mail_info))
    {
        RUN_LOG_CRI("load mail info failed");
        return(false);
    }

    if (!m_check_timer.init(this, 30))
    {
        RUN_LOG_CRI("check timer init failed");
        return(false);
    }

    RUN_LOG_DBG("daemon init success");

    return(true);
}

void Daemon::exit()
{
    if (!m_running)
    {
        return;
    }

    m_running = false;

    m_check_timer.exit();

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
        const ServiceInfo & service_info = *iter;
        std::map<std::string, ProcessInfo>::iterator iter_proc = m_process_info_map.find(service_info.cmdl);
        bool service_is_ok = true;

        if (service_info.ports.empty())
        {
            std::string process_name;
            if (m_process_info_map.end() != iter_proc)
            {
                process_name = iter_proc->second.name;
            }
            else
            {
#ifdef _MSC_VER
                process_name = service_info.file;
#else
                process_name = service_info.cmdl;
#endif // _MSC_VER
            }
            if (!is_process_alive(process_name))
            {
                RUN_LOG_DBG("service {%s} is not alive", service_info.cmdl.c_str());
                service_is_ok = false;
            }
        }
        else
        {
            for (std::list<std::string>::const_iterator iter_port = service_info.ports.begin(); service_info.ports.end() != iter_port; ++iter_port)
            {
                socket_t connecter = BAD_SOCKET;
                if (!Stupid::Net::tcp_connect(service_info.host.c_str(), iter_port->c_str(), connecter))
                {
                    RUN_LOG_DBG("service {%s} can not be connected on port %s", service_info.cmdl.c_str(), iter_port->c_str());
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
                RUN_LOG_DBG("stop service {%s} begin", service_info.cmdl.c_str());
                kill_process(iter_proc->second.id, iter_proc->second.name);
                RUN_LOG_DBG("stop service {%s} end", service_info.cmdl.c_str());
            }

            size_t process_id = 0;
            std::string process_name;
            if (create_process(iter->path, iter->cmdl, iter->show, process_id, process_name))
            {
                RUN_LOG_DBG("start service {%s} success", service_info.cmdl.c_str());
                ProcessInfo process_info = { false, process_id, process_name };
                m_process_info_map[iter->cmdl] = process_info;
            }
            else
            {
                RUN_LOG_DBG("start service {%s} failure", service_info.cmdl.c_str());
            }
        }

        if (m_send_mail && m_process_info_map.end() != iter_proc && iter_proc->second.work != service_is_ok)
        {
            iter_proc->second.work = service_is_ok;

            if (service_is_ok)
            {
                m_mail_info.m_mail_content = "<DIV> {" + iter->cmdl + "} is restart </DIV>";
            }
            else
            {
                m_mail_info.m_mail_content = "<DIV> {" + iter->cmdl + "} is stop </DIV>";
            }

            if (!Stupid::Tool::MailHelper::send_mail(m_mail_info))
            {
                RUN_LOG_ERR("send mail failed, content : {%s}", m_mail_info.m_mail_content.c_str());
            }
        }
    }

    m_last_check_time = Stupid::Base::stupid_time();
}
