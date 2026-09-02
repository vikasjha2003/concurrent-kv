#pragma once

#include <cstdint>

#include "../command/command_processor.hpp"

class TcpServer {
public:
    TcpServer(std::uint16_t port, CommandProcessor& processor);

    void run();

private:
    std::uint16_t port_;
    CommandProcessor& processor_;
};