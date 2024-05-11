#pragma once
#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <my_chat_lib/MCL.hpp>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <set>
#include <string>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace MCL
{
    const char abmons[12][4] = {"JAN", "FEB", "MAR", "APR", "JUN", "JUL", "AUG", "SEP", "OKT", "NOV", "DEC"};

    unsigned int get_port(int argc, char *argv[]);

    enum MessageTypes
    {
        ConnectRequest,
        ConnectAccept,
        ConnectDecline,
        Disconnect,
        Text
    };

    struct Message
    {
        MessageTypes _type;
        std::string _from;
        std::string _text;
        std::tm _time;

        Message()
        {
            _from.reserve(40);
            _text.reserve(128);
        }
        Message(const std::string &name, const std::string &text) : _type(Text), _from(name), _text(text)
        {
            _from.reserve(40);
            _text.reserve(128);
        }
        Message(const MessageTypes &type, const std::string &name, const std::string &text) : _type(type), _from(name), _text(text)
        {
            _from.reserve(40);
            _text.reserve(128);
        }

        std::string serialize() const;
        static Message deserialize(std::string &s);

        friend std::ostream &operator<<(std::ostream &s, const Message &m);
    };

    enum ServerResponse
    {
        CONN_OK,
        ERROR_USER_ALREADY_EXISTS,
        ERROR_SERVER_OVERLOAD
    };

    class TCPServer
    {
    private:
        const std::string serverIP{"127.0.0.1"};
        unsigned int port;

        int master_socket;
        struct sockaddr_in server_address
        {
        };

        const char *log_filename = "log.log";

        int outfd = open(log_filename, O_WRONLY | O_CLOEXEC);

        std::mutex outfd_mutex;

        int max_sd;
        std::mutex max_sd_mutex;

        fd_set readfds{};
        std::mutex readfds_mutex;

        std::set<int> client_sockets;
        std::mutex client_sockets_mutex;

        std::map<int, std::string> fd_to_username;
        std::mutex fd_to_username_mutex;

        enum LogMode
        {
            LOG_CONN_TRY,
            LOG_CONN_FAIL,
            LOG_CONN_OK,
            LOG_MESSAGE,
        };

        enum ServerConsts
        {
            REQ_BUFFSIZE = 64,
            LOG_BUFFER_SIZE = 512,
            NICK_BUFFER_SIZE = 40,
            BUFFLEN = 128,
            MAX_CLIENTS = 10,
        };

        bool check_username(const std::string &username);
        static void *thread_func(void *arg);
        void disconnect_user(intptr_t _conn_fd);
        void push_message_to_clients(std::string &buffer, int conn_fd);

    public:
        TCPServer(unsigned int port_ = 54010);
        void RunLoop();
    };

    class TCPClient
    {
    private:
        enum ClientConsts
        {
            USER_LIST_SIZE = 640,
            MAX_NICK_SIZE = 40,
        };

        std::string serverIP = "0.0.0.0";
        unsigned int server_port;

        std::string nickname;

        int sock;
        struct sockaddr_in server_address
        {
        };

        std::string get_nickname();
        static void *start_listening(void *arg);

    public:
        TCPClient(unsigned int port_ = 54010);
        void ConnectToServer();
        void RunLoop();
    };

} // namespace MCL
