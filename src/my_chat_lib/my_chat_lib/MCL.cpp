#include "MCL.hpp"
#include <iomanip>
#include <iostream>
#include <string>

namespace MCL
{
    std::pair<std::string, unsigned int> parse_param(int argc, char *argv[])
    {
        if (argc < 2)
        {
            return {"", 0};
        }

        std::string ip;
        std::string port;

        if (argc == 2)
        {
            ip = "0.0.0.0";
            port = argv[1];
        }
        else if (argc == 3)
        {
            ip = argv[1];
            port = argv[2];
        }

        std::string::const_iterator it = port.begin();

        while (it != port.end() && (std::isdigit(*it) != 0))
        {
            ++it;
        }
        if (!port.empty() && it == port.end())
        {
            auto int_port = static_cast<unsigned int>(std::atol(port.c_str()));
            return {ip, int_port};
        }

        return {"", 0};
    }

    unsigned int get_port(int argc, char *argv[])
    {
        if (argc < 2)
        {
            return 0;
        }

        std::string port = argv[1];

        std::string::const_iterator it = port.begin();

        while (it != port.end() && (std::isdigit(*it) != 0))
        {
            ++it;
        }
        if (!port.empty() && it == port.end())
        {
            auto int_port = static_cast<unsigned int>(std::atol(port.c_str()));
            return int_port;
        }

        return 0;
    }

    std::ostream &operator<<(std::ostream &s, const Message &m)
    {
        if (m._type == MessageTypes::ConnectRequest)
        {
            s << "Connection Request from: " << m._from << std::endl;
        }
        else if (m._type == MessageTypes::ConnectAccept)
        {
            s << "Connection Accepted for: " << m._from << std::endl;
        }
        else if (m._type == MessageTypes::ConnectDecline)
        {
            s << "Connection Refused for: " << m._from << " because " << m._text << std::endl;
        }
        else if (m._type == MessageTypes::Text)
        {
            std::string formated_text = m._text;
            for (size_t i = 39; i < formated_text.size(); i += 40)
            {
                if (formated_text[i] != ' ')
                {
                    bool cutted = false;
                    for (size_t j = i; j > 0; j--)
                    {
                        if (formated_text[j] == ' ')
                        {
                            formated_text[j] = '\n';
                            cutted = true;
                            break;
                        }
                    }
                    if (!cutted)
                    {
                        formated_text = formated_text.substr(0, 40) + "\n" + formated_text.substr(40, formated_text.size());
                    }
                }
                else
                {
                    formated_text[i] = '\n';
                }
            }
            s << m._from << " " << std::setfill('-') << std::setw(39 - m._from.size()) << "\n"
              << formated_text << "\n"
              << std::setfill('_') << std::setw(28) << " "
              << std::setfill('0') << std::setw(2) << m._time.tm_mday << "."
              << std::setfill('0') << std::setw(2) << (m._time.tm_mon + 1) << " "
              << std::setfill('0') << std::setw(2) << m._time.tm_hour << ":"
              << std::setfill('0') << std::setw(2) << m._time.tm_min << "\n";
        }
        else if (m._type == MCL::MessageTypes::Disconnect)
        {
            s << "User: " << m._from << " disconnected\n";
        }

        return s;
    }

    std::string Message::serialize() const
    {
        std::string s;
        s += static_cast<char>(_type);
        s += '\n';
        s += _from;
        s += '\n';
        s += _text;
        s += '\n';

        return s;
    }

    Message Message::deserialize(std::string &s)
    {
        auto type = static_cast<MessageTypes>(s[0]);
        auto pos = s.find('\n', 2);
        auto from = s.substr(2, pos - 2);
        auto text = s.substr(pos + 1, s.find('\n', pos + 1) - (pos + 1));

        return Message{type, from, text};
    }

    void TCPServer::disconnect_user(intptr_t _conn_fd)
    {
        readfds_mutex.lock();
        FD_CLR(_conn_fd, &readfds);
        readfds_mutex.unlock();

        fd_to_username_mutex.lock();
        fd_to_username.erase(_conn_fd);
        fd_to_username_mutex.unlock();

        client_sockets_mutex.lock();
        max_sd_mutex.lock();
        client_sockets.erase(_conn_fd);

        if (!client_sockets.empty())
        {
            max_sd = std::max(master_socket, *client_sockets.rbegin());
        }
        else
        {
            max_sd = master_socket;
        }
        max_sd_mutex.unlock();
        client_sockets_mutex.unlock();

        std::cout << "Соединение прервано: " << _conn_fd << std::endl;

        close(_conn_fd);
    }

    TCPServer::TCPServer(unsigned int port_) : port{port_}
    {
        if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("Сервер не может открыть socket для TCP.");
            exit(1);
        }

        max_sd = master_socket;

        bzero(reinterpret_cast<char *>(&server_address), sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(port);
        server_address.sin_addr.s_addr = htonl(INADDR_ANY);
        // inet_pton(AF_INET, serverIP.c_str(), &server_address.sin_addr);

        if (bind(master_socket, reinterpret_cast<const sockaddr *>(&server_address), sizeof(server_address)) == -1)
        {
            perror("Связывание сервера неудачно.");
            exit(1);
        }

        char myIP[16];
        inet_ntop(AF_INET, &server_address.sin_addr, myIP, sizeof(myIP));
        std::cout << "СЕРВЕР:\n\tадрес: " << myIP << "\n\tномер порта: " << ntohs(server_address.sin_port) << std::endl;
    }

    struct server_thread_data
    {
        TCPServer *_server;
        int _new_sd;

        server_thread_data(TCPServer *server, int new_sd) : _server{server}, _new_sd(new_sd){};
    };

    void TCPServer::RunLoop()
    {
        if (listen(master_socket, MAX_CLIENTS) != 0)
        {
            perror("Listen error!");
            exit(1);
        }

        timeval select_delay{};
        select_delay.tv_sec = 0;
        select_delay.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);

        for (;;)
        {
            readfds_mutex.lock();
            fd_set readfds_copy = readfds;
            readfds_mutex.unlock();

            max_sd_mutex.lock();
            int activity = select(max_sd + 1, &readfds_copy, nullptr, nullptr, &select_delay);
            max_sd_mutex.unlock();
            if (activity < 0)
            {
                perror("select");
                return;
            }

            if (FD_ISSET(master_socket, &readfds_copy))
            {
                // Новое соединение
                sockaddr_in client_addr{};
                socklen_t client_addr_len = sizeof(client_addr);
                int new_sd = accept(master_socket, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);
                if (new_sd < 0)
                {
                    perror("accept");
                    continue;
                }

                // Установка неблокирующего режима
                int flags = fcntl(new_sd, F_GETFL, 0);
                if (fcntl(new_sd, F_SETFL, flags | O_NONBLOCK) == -1)
                {
                    perror("fcntl");
                    close(new_sd);
                    continue;
                }

                FD_SET(new_sd, &readfds);
                max_sd_mutex.lock();
                std::max(max_sd, new_sd);
                max_sd_mutex.unlock();

                client_sockets_mutex.lock();
                client_sockets.insert(new_sd);
                client_sockets_mutex.unlock();

                std::cout << "Новое соединение: " << new_sd << std::endl;

                pthread_t thread_id;
                server_thread_data td{this, new_sd};
                int ret = pthread_create(&thread_id, nullptr, thread_func, reinterpret_cast<void *>(&td));
                if (ret != 0)
                {
                    printf("Error from pthread: %d\n", ret);
                    exit(1);
                }
            }
        }

        close(master_socket);
    }

    bool TCPServer::check_username(const std::string &username)
    {
        fd_to_username_mutex.lock();
        for (const auto &[fd, name] : fd_to_username)
        {
            if (username == name)
            {
                fd_to_username_mutex.unlock();
                return true;
            }
        }
        fd_to_username_mutex.unlock();
        return false;
    }

    void TCPServer::push_message_to_clients(std::string &buffer, int conn_fd)
    {
        client_sockets_mutex.lock();
        for (int client_sd : client_sockets)
        {
            if (client_sd != master_socket && client_sd != conn_fd)
            {
                if (write(client_sd, buffer.data(), 256) < 0)
                {
                    perror("write");
                }
            }
        }
        client_sockets_mutex.unlock();
    }

    void *TCPServer::thread_func(void *arg)
    {
        pthread_t id = pthread_self();
        auto *td = reinterpret_cast<server_thread_data *>(arg);
        auto *self = static_cast<TCPServer *>(td->_server);
        auto conn_fd = static_cast<intptr_t>(td->_new_sd);
        printf("thread: %ld - serving fd %ld\n", id, conn_fd);

        std::string buff;
        buff.resize(256);

        if (read(conn_fd, buff.data(), 256) < 0)
        {
            perror("read");
        }

        auto reg_message = MCL::Message::deserialize(buff);

        std::string nickname = reg_message._from;

        if (self->check_username(nickname))
        {
            auto *response = new MCL::Message{MCL::MessageTypes::ConnectDecline, nickname, "Username already taken"};

            if (write(conn_fd, response->serialize().c_str(), sizeof(MCL::Message)) == -1)
            {
                perror("write");
            }

            self->disconnect_user(conn_fd);

            return nullptr;
        }

        std::cout << "Connected: " << nickname << std::endl;

        self->fd_to_username_mutex.lock();
        self->fd_to_username[conn_fd] = nickname;
        self->fd_to_username_mutex.unlock();

        auto *response = new MCL::Message{MCL::MessageTypes::ConnectAccept, nickname, "Connected"};

        if (write(conn_fd, response->serialize().c_str(), sizeof(MCL::Message)) == -1)
        {
            perror("write");
        }

        auto resp = response->serialize();
        self->push_message_to_clients(resp, conn_fd);

        for (;;)
        {
            auto activity = read(conn_fd, buff.data(), 256);
            if (activity > 0)
            {
                auto msg = MCL::Message::deserialize(buff);
                if (msg._type == MCL::MessageTypes::Text)
                {
                    self->push_message_to_clients(buff, conn_fd);
                }
                else if (msg._type == MCL::MessageTypes::Disconnect)
                {
                    self->push_message_to_clients(buff, conn_fd);
                    break;
                }
            }
        }

        self->disconnect_user(conn_fd);

        return nullptr;
    }

    TCPClient::TCPClient(const std::string &ip_, unsigned int port_) : server_port{port_}
    {
        nickname = get_nickname();

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("He могу получить socket\n");
            exit(1);
        }
        bzero(reinterpret_cast<char *>(&server_address), sizeof(server_address));

        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(server_port);

        if (inet_pton(AF_INET, ip_.c_str(), &server_address.sin_addr) != 1)
        {
            perror("Invalid IP");
            exit(1);
        }

        container = ftxui::CatchEvent(container, [&](const ftxui::Event &event)
                                      {
            if (event == ftxui::Event::Return) {
                SendMessage();
                return true;
            }
            return false; });
    }

    std::string TCPClient::get_nickname()
    {
        std::string nickname;
        std::cout << "Choose your nickname:" << std::endl;
        std::getline(std::cin, nickname);
        if (nickname.size() > MAX_NICK_SIZE)
        {
            perror("Nickname must be less or equival to 40");
            exit(1);
        }
        return nickname;
    }

    void TCPClient::ConnectToServer()
    {
        if (connect(sock, reinterpret_cast<const sockaddr *>(&server_address), sizeof(server_address)) != 0)
        {
            perror("Connection error");
            exit(1);
        }
        std::cout << "Connected to server\n";

        MCL::Message reg_message{MCL::MessageTypes::ConnectRequest, nickname, ""};

        std::cout << reg_message << std::endl;

        if (write(sock, reg_message.serialize().c_str(), 256) < 0)
        {
            perror("Write error");
        };

        std::string response_buff;
        response_buff.resize(256);

        if (read(sock, &*response_buff.begin(), 256) < 0)
        {
            perror("Read error");
        }

        reg_message = MCL::Message::deserialize(response_buff);

        std::cout << reg_message << "\n";

        switch (reg_message._type)
        {
        case MCL::MessageTypes::ConnectAccept:
        {
            break;
        }
        case MCL::MessageTypes::ConnectDecline:
        {
            std::cout << reg_message._text << std::endl;
            close(sock);
            exit(0);
        }
        default:
        {
            std::cout << "Unknown server response" << std::endl;
            close(sock);
            exit(0);
        }
        }
    }

    struct client_thread_data
    {
        TCPClient *_client;
        int _sock;

        client_thread_data(TCPClient *client, int sock) : _client{client}, _sock(sock){};
    };

    void *TCPClient::start_listening(void *arg)
    {
        auto *td = reinterpret_cast<client_thread_data *>(arg);
        auto *self = (td->_client);
        auto sock = (td->_sock);
        fd_set readfds{};
        FD_SET(sock, &readfds);
        delete td;

        std::string buff;
        buff.resize(256);

        for (;;)
        {
            fd_set readfds_copy = readfds;

            if (select(sock + 1, &readfds_copy, nullptr, nullptr, nullptr) < 0)
            {
                perror("select");
            }

            if (FD_ISSET(sock, &readfds_copy))
            {
                bzero(buff.data(), sizeof(MCL::Message));

                if (read(sock, buff.data(), 256) < 0)
                {
                    perror("read");
                }

                auto t = std::time(nullptr);
                auto *now = std::localtime(&t);
                auto msg = MCL::Message::deserialize(buff);

                msg._time = *now;

                self->ReceiveMessage(msg);
            }
        }

        return nullptr;
    }

    void TCPClient::RunLoop()
    {
        auto *td = new client_thread_data{this, sock};
        int ret = pthread_create(&listener_thread_id, nullptr, start_listening, reinterpret_cast<void *>(td));
        if (ret != 0)
        {
            perror("pthread");
        }
        // std::cout << "Thread started\n";
    }

    void TCPClient::SendMessage(std::string &message)
    {
        auto *msg = new MCL::Message{};

        msg->_from = nickname;
        msg->_type = MCL::MessageTypes::Text;
        msg->_text = message;

        auto t = std::time(nullptr);
        auto *now = std::localtime(&t);

        msg->_time = *now;

        if (write(sock, msg->serialize().data(), 256) < 0)
        {
            delete msg;
            perror("write");
        }

        delete msg;
    }

    void TCPClient::Disconnect()
    {
        auto *msg = new MCL::Message{};
        msg->_from = nickname;
        msg->_type = MCL::MessageTypes::Disconnect;

        if (write(sock, msg->serialize().data(), 256) < 0)
        {
            perror("write");
        }

        delete msg;

        pthread_cancel(listener_thread_id);

        close(sock);
    }

    void TCPClient::SendMessage()
    {
        // Отправка сообщения через client
        message.erase(message.begin(), std::find_if(message.begin(), message.end(), [](int c)
                                                    { return std::isspace(c) == 0; }));
        if (!message.empty())
        {
            SendMessage(message);

            Message msg{MessageTypes::Text, nickname, message};
            auto t = std::time(nullptr);
            auto *now = std::localtime(&t);
            msg._time = *now;

            ReceiveMessage(std::move(msg));
            message.clear();
        }
    }

    void TCPClient::ExitChat()
    {
        // Закрытие соединения и выход из чата
        Disconnect();
        screen.ExitLoopClosure()();
    }

    void TCPClient::ReceiveMessage(MCL::Message message)
    {
        // Добавление нового сообщения в историю
        messages.push_back(std::move(message));
        // Обновление интерфейса
        screen.PostEvent(ftxui::Event::Custom);
    }

} // namespace MCL