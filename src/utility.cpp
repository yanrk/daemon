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

#include "net/common/common.h"

#ifdef _MSC_VER
    #include <windows.h>
    #include <tlhelp32.h>
#else
    #include <errno.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <signal.h>
    #include <sys/wait.h>
    #include <cstdio>
    #include <cstdlib>
#endif // _MSC_VER

#include <cstdio>
#include <cstring>
#include <list>
#include <sstream>
#include <iomanip>

#include "base/log/log.h"
#include "base/string/string.h"
#include "base/filesystem/directory.h"
#include "base/utility/utility.h"
#include "utility.h"

bool exclusive_init(const char * exclusive_unique_name, size_t & unique_id)
{
    if (nullptr == exclusive_unique_name)
    {
        printf("exclusive_unique_name is nullptr\n");
        return false;
    }

#ifdef _MSC_VER
    unique_id = reinterpret_cast<size_t>(nullptr);

    std::string exclusive_file;
    {
        std::ostringstream oss;
        oss << "Global\\mutex_" << exclusive_unique_name;
        exclusive_file = oss.str();
    }

    HANDLE mutex = ::CreateMutex(nullptr, FALSE, exclusive_file.c_str());
    if (ERROR_ALREADY_EXISTS == stupid_system_error() || ERROR_ACCESS_DENIED == stupid_system_error())
    {
        printf("another process has started\n");
        if (nullptr != mutex)
        {
            ::CloseHandle(mutex);
        }
        return false;
    }
    else if (nullptr == mutex)
    {
        printf("create mutex %s failed: %d\n", exclusive_file.c_str(), stupid_system_error());
        return false;
    }

    unique_id = reinterpret_cast<size_t>(mutex);

    return true;
#else
    unique_id = static_cast<size_t>(-1);

    std::string exclusive_file;
    {
        std::ostringstream oss;
        oss << "/var/run/" << exclusive_unique_name << ".pid";
        exclusive_file = oss.str();
    }

    int fd = ::open(exclusive_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
    {
        printf("open %s failed: %d\n", exclusive_file.c_str(), stupid_system_error());
        return false;
    }

    struct flock locker;
    locker.l_type = F_WRLCK;
    locker.l_start = 0;
    locker.l_whence = SEEK_SET;
    locker.l_len = 0;
    if (::fcntl(fd, F_SETLK, &locker) < 0)
    {
        if (EACCES == stupid_system_error() || EAGAIN == stupid_system_error())
        {
            printf("another process has started\n");
        }
        else
        {
            printf("lock %s failed: %d\n", exclusive_file.c_str(), stupid_system_error());
        }
        ::close(fd);
        return false;
    }

    std::string pid;
    {
        std::ostringstream oss;
        oss << static_cast<size_t>(getpid());
        pid = oss.str();
    }

    if (::ftruncate(fd, 0) < 0)
    {
        printf("truncate %s failed: %d\n", exclusive_file.c_str(), stupid_system_error());
    }

    if (::write(fd, pid.c_str(), pid.size()) < 0)
    {
        printf("write %s failed: %d\n", exclusive_file.c_str(), stupid_system_error());
    }

    unique_id = static_cast<size_t>(fd);

    return true;
#endif // _MSC_VER
}

void exclusive_exit(size_t & unique_id)
{
#ifdef _MSC_VER
    HANDLE mutex = reinterpret_cast<HANDLE>(unique_id);
    if (nullptr != mutex)
    {
        ::CloseHandle(mutex);
    }
    unique_id = reinterpret_cast<size_t>(nullptr);
#else
    int fd = static_cast<int>(unique_id);
    if (fd >= 0)
    {
        ::close(fd);
    }
    unique_id = static_cast<size_t>(-1);
#endif // _MSC_VER
}

struct PROCESS_INFO
{
    size_t        pid;  /* process id   */
    std::string   cmd;  /* command line */
};

static bool get_all_process(std::list<PROCESS_INFO> & process_list)
{
    process_list.clear();

#ifdef _MSC_VER
    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snapshot)
    {
        RUN_LOG_ERR("CreateToolhelp32Snapshot failed: %d", stupid_system_error());
        return false;
    }

    PROCESSENTRY32 pe = { sizeof(PROCESSENTRY32) };

    for (BOOL ok = ::Process32First(snapshot, &pe); TRUE == ok; ok = Process32Next(snapshot, &pe))
    {
        PROCESS_INFO process_info;
        process_info.pid = static_cast<size_t>(pe.th32ProcessID);
        process_info.cmd = pe.szExeFile;
        process_list.push_back(process_info);
    }

    ::CloseHandle(snapshot);
#else
    const char * command = "ps -eo pid,cmd";
    FILE * file = ::popen(command, "r");
    if (nullptr == file)
    {
        RUN_LOG_ERR("popen(%s) failed: %d", command, stupid_system_error());
        return false;
    }

    const std::string spaces(" ");
    const size_t buff_size = 2048;
    char buff[buff_size] = { 0 };
    while (nullptr != ::fgets(buff, buff_size, file))
    {
        std::string line(buff);

        PROCESS_INFO process_info;

        std::string::size_type pid_b = line.find_first_not_of(spaces, 0);
        if (std::string::npos == pid_b)
        {
            continue;
        }
        std::string::size_type pid_e = line.find_first_of(spaces, pid_b);
        if (std::string::npos == pid_e)
        {
            continue;
        }
        std::string pid_value(line.begin() + pid_b, line.begin() + pid_e);
        if (!Stupid::Base::stupid_string_to_type(pid_value, process_info.pid))
        {
            continue;
        }

        std::string command_line(line.begin() + pid_e, line.end());
        Stupid::Base::stupid_string_trim(command_line);
        command_line.swap(process_info.cmd);

        process_list.push_back(process_info);
    }

    ::pclose(file);
#endif // _MSC_VER

    return true;
}

static bool get_process_name(size_t process_id, std::string & process_name)
{
    std::list<PROCESS_INFO> process_list;
    get_all_process(process_list);

    for (std::list<PROCESS_INFO>::const_iterator iter = process_list.begin(); process_list.end() != iter; ++iter)
    {
        if (iter->pid == process_id)
        {
            process_name = iter->cmd;
            return true;
        }
    }

    process_name.clear();
    return false;
}

bool create_process(const std::string & path, const std::string & command_line, bool show_window, size_t & process_id, std::string & process_name)
{
    if (command_line.empty())
    {
        return true;
    }

    RUN_LOG_DBG("try to create process with command line: {%s}", command_line.c_str());

#ifdef _MSC_VER
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi = { 0x00 };

    DWORD creation_flags = (show_window ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW);

    if (!::CreateProcess(nullptr, reinterpret_cast<LPSTR>(const_cast<char *>(command_line.c_str())), nullptr, nullptr, false, creation_flags, nullptr, nullptr, &si, &pi))
    {
        RUN_LOG_ERR("create process failed: command(%s), errno(%d)", command_line.c_str(), stupid_system_error());
        return false;
    }
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);

    process_id = static_cast<size_t>(pi.dwProcessId);
#else
    const size_t argc = 1;
    const char * argv[argc + 1] = { command_line.c_str(), nullptr };

    pid_t pid = ::fork();
    if (pid < 0)
    {
        RUN_LOG_ERR("fork failed: command(%s), errno(%d)", command_line.c_str(), stupid_system_error());
        return false;
    }
    else if (0 == pid)
    {
        if (!Stupid::Base::stupid_set_current_work_directory(path))
        {
            RUN_LOG_ERR("set current work directory(%s) failed for command(%s)", path.c_str(), command_line.c_str());
        }
        ::close(STDIN_FILENO);
        ::close(STDOUT_FILENO);
        ::close(STDERR_FILENO);
        /*
         * if execv() success, will not execute the next exit()
         */
        if (::execv(argv[0], const_cast<char **>(argv)) < 0)
        {
            RUN_LOG_ERR("execv failed: command(%s), errno(%d)", command_line.c_str(), stupid_system_error());
            ::exit(201); // should not be here
        }
        ::exit(202); // should never be here
    }

    process_id = static_cast<size_t>(pid);
#endif // _MSC_VER

    if (!get_process_name(process_id, process_name))
    {
        RUN_LOG_ERR("get process name failed");
    }

    RUN_LOG_DBG("create process success with command line: {%s}", command_line.c_str());

    return true;
}

static bool process_is_alive(const std::list<PROCESS_INFO> & process_list, size_t process_id, const std::string & process_name)
{
    for (std::list<PROCESS_INFO>::const_iterator iter = process_list.begin(); process_list.end() != iter; ++iter)
    {
        if (iter->pid == process_id)
        {
            return iter->cmd == process_name;
        }
    }
    return false;
}

static bool kill_process(size_t process_id, size_t exit_code = 9)
{
    if (0 == process_id)
    {
        return true;
    }

    if (Stupid::Base::get_pid() == process_id)
    {
        RUN_LOG_CRI("kill process exception: why do you kill current process?");
        return true;
    }

#ifdef _MSC_VER
    HANDLE process = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, static_cast<DWORD>(process_id));
    if (nullptr == process)
    {
        RUN_LOG_ERR("open process %u failed: %d", process_id, stupid_system_error());
        return false;
    }
    if (!::TerminateProcess(process, exit_code))
    {
        RUN_LOG_ERR("kill process %u failed: %d", process_id, stupid_system_error());
        return false;
    }
#else
    pid_t pid = static_cast<pid_t>(process_id);
    if (::kill(pid, SIGKILL) < 0)
    {
        RUN_LOG_ERR("kill process %u failed: %d", process_id, stupid_system_error());
    }
    if (::waitpid(pid, nullptr, 0) != pid)
    {
        RUN_LOG_ERR("wait process %u failed: %d", process_id, stupid_system_error());
    }
#endif // _MSC_VER
    return true;
}

bool kill_process(size_t process_id, const std::string & process_name)
{
    if (Stupid::Base::get_pid() == process_id || 0 == process_id)
    {
        return true;
    }

    RUN_LOG_DBG("kill process: [%u:%s] begin", process_id, process_name.c_str());

    bool ret = false;
    do
    {
        std::list<PROCESS_INFO> process_list;
        get_all_process(process_list);

        if (!process_is_alive(process_list, process_id, process_name))
        {
            RUN_LOG_DBG("process [%u:%s] is not exist", process_id, process_name.c_str());
            ret = true;
            break;
        }

        if (kill_process(process_id))
        {
            RUN_LOG_DBG("kill process %u success", process_id);
        }
        else
        {
            RUN_LOG_ERR("kill process %u failure", process_id);
        }

        process_list.clear();
        get_all_process(process_list);

        if (process_is_alive(process_list, process_id, process_name))
        {
            RUN_LOG_DBG("process [%u:%s] is still exist", process_id, process_name.c_str());
            break;
        }

        ret = true;
    } while (false);

    RUN_LOG_DBG("kill process: [%u:%s] end", process_id, process_name.c_str());

    return ret;
}

bool is_process_alive(const std::string & process_name)
{
    std::list<PROCESS_INFO> process_list;
    get_all_process(process_list);

    for (std::list<PROCESS_INFO>::const_iterator iter = process_list.begin(); process_list.end() != iter; ++iter)
    {
        if (iter->cmd == process_name)
        {
            return true;
        }
    }

    return false;
}
