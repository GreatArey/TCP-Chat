#include <my_chat_lib/MCL.hpp>
#include <iostream>

int main(int argc, char *argv[])
{
    auto parsed_data = MCL::parse_param(argc, argv);
    unsigned int port = parsed_data.second;
    std::string ip = parsed_data.first;

    if (port == 0)
    {
        std::cout << "Usage:\n\n"
                  << "./server <ip> port\n\n"
                  << "ip: IPv4 format"
                  << "port: unsigned int value, greater than 0\n";
        return 0;
    }

    MCL::TCPClient client{ip, port};

    client.ConnectToServer();
    client.Run();

    return 0;
}