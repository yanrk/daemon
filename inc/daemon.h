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

#ifndef DAEMON_DAEMON_H
#define DAEMON_DAEMON_H


#include <cstdint>
#include <string>
#include <map>
#include "base/time/single_timer.h"
#include "base/utility/uncopy.h"
#include "base/utility/singleton.h"
#include "tool/mail/mail.h"

class Daemon : public Stupid::Base::ISingleTimerSink, private Stupid::Base::Uncopy
{
private:
    Daemon();
    virtual ~Daemon();

public:
    bool init(const std::string & current_work_directory);
    void exit();

public:
    virtual void on_timer(bool first_time, size_t index);

private:
    friend class Stupid::Base::Singleton<Daemon>;

private:
    struct ProcessInfo
    {
        bool          work;
        size_t        id;
        std::string   name;
    };

private:
    volatile bool                        m_running;
    std::string                          m_root_directory;
    uint64_t                             m_last_check_time;
    uint64_t                             m_check_interval;
    std::map<std::string, ProcessInfo>   m_process_info_map;
    bool                                 m_send_mail;
    Stupid::Tool::MailInfo               m_mail_info;
    Stupid::Base::SingleTimer            m_check_timer;
};


#endif // DAEMON_DAEMON_H
