#include <sys/types.h>
#include <sys/socket.h>
#include <cstdio>
#include <unistd.h>
#include <my_chat_lib/MCL.hpp>
#include <netinet/in.h>
#include <netdb.h>
#include <cerrno>
#include <strings.h>
#include <cstdlib>
#include <arpa/inet.h>
#include <string>
#include <iostream>

void clear()
{
    // CSI[2J clears screen, CSI[H moves the cursor to top-left corner
    std::cout << "\x1B[2J\x1B[H";
}

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

    MCL::TCPClient client{port};

    client.ConnectToServer();

    // clear();

    client.RunLoop();

    return 0;
}