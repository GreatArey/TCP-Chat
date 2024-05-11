#include <iostream>
#include <my_chat_lib/MCL.hpp>

int main(int argc, char *argv[])
{
    unsigned int port = MCL::get_port(argc, argv);

    if (port == 0)
    {
        std::cout << "Usage:\n\n"
                  << "./server port\n\n"
                  << "port: unsigned int value, greater than 0\n";
        return 0;
    }

    MCL::TCPServer server{port};

    server.RunLoop();

    return 0;
}
