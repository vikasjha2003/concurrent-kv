#pragma once

#include <cstdint>
#include <thread>

#include "../command/command_processor.hpp"

class TcpServer {
public:
    TcpServer(std::uint16_t port, CommandProcessor& processor);

    void run();

private:
    void handleClient(int client_fd);

    std::uint16_t port_;
    CommandProcessor& processor_;
};