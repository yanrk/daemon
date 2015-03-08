/********************************************************
 * Description : utility of daemon
 * Data        : 2015-02-04 09:30:00
 * Author      : yanrk
 * Email       : yanrkchina@hotmail.com
 * Blog        : blog.csdn.net/cxxmaker
 * Version     : 1.0
 * History     :
 * Copyright(C): 2015 - 2017
 ********************************************************/

#ifndef DAEMON_UTILITY_H
#define DAEMON_UTILITY_H


#include <list>
#include <string>

extern bool exclusive_init(const char * exclusive_unique_name, size_t & unique_id);
extern void exclusive_exit(size_t & unique_id);

extern bool create_process(const std::string & path, const std::string & command_line, bool show_window, size_t & process_id, std::string & process_name);
extern bool kill_process(size_t process_id, const std::string & process_name);
extern bool is_process_alive(const std::string & process_name);


#endif // DAEMON_UTILITY_H
