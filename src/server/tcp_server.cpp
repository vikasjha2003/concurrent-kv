#include "tcp_server.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

TcpServer::TcpServer(std::uint16_t port, CommandProcessor& processor)
    : port_(port), processor_(processor) {
}

void TcpServer::run() {

    // Create listening socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        std::cerr << "ERROR socket: "
                  << std::strerror(errno) << '\n';
        return;
    }

    // Allow quick restart after shutting down the server
    int opt = 1;

    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)) == -1) {

        std::cerr << "ERROR setsockopt: "
                  << std::strerror(errno) << '\n';

        close(server_fd);
        return;
    }

    // Configure address
    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port_);

    // Bind
    if (bind(
            server_fd,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) == -1) {

        std::cerr << "ERROR bind: "
                  << std::strerror(errno) << '\n';

        close(server_fd);
        return;
    }

    // Listen
    if (listen(server_fd, 10) == -1) {

        std::cerr << "ERROR listen: "
                  << std::strerror(errno) << '\n';

        close(server_fd);
        return;
    }

    std::cout << "Server listening on port "
              << port_ << '\n';

    // Main server loop
    while (true) {

        std::cout << "Waiting for client...\n";

        sockaddr_in client_address{};
        socklen_t client_address_length =
            sizeof(client_address);

        int client_fd = accept(
            server_fd,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_address_length
        );

        if (client_fd == -1) {

            if (errno == EINTR) {
                // accept() was interrupted by a signal.
                // Nothing is wrong with the server; try again.
                continue;
            }

            std::cerr << "ERROR accept: "
                      << std::strerror(errno) << '\n';

            continue;
        }

        std::cout << "Client connected.\n";

        std::string input_buffer;
        char buffer[4096];

        // Read from this client until it disconnects
        while (true) {

            ssize_t bytes_received = recv(
                client_fd,
                buffer,
                sizeof(buffer),
                0
            );

            if (bytes_received == 0) {
                // Client disconnected normally
                std::cout << "Client disconnected.\n";
                break;
            }

            if (bytes_received == -1) {

                if (errno == EINTR) {
                    // recv() was interrupted by a signal.
                    // Retry the receive operation.
                    continue;
                }

                std::cerr << "ERROR recv: "
                          << std::strerror(errno) << '\n';

                break;
            }

            input_buffer.append(buffer, bytes_received);

            // Process every complete line currently in the buffer
            while (true) {

                std::size_t newline_pos =
                    input_buffer.find('\n');

                if (newline_pos == std::string::npos) {
                    break;
                }

                std::string line =
                    input_buffer.substr(0, newline_pos);

                // Remove the processed line, including '\n'
                input_buffer.erase(0, newline_pos + 1);

                // Handle CRLF (\r\n) as well as LF (\n)
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                std::string response =
                    processor_.process(line);

                response += '\n';

                // Send the response
                std::size_t total_sent = 0;

                while (total_sent < response.size()) {

                    ssize_t bytes_sent = send(
                        client_fd,
                        response.data() + total_sent,
                        response.size() - total_sent,
                        0
                    );

                    if (bytes_sent == -1) {
                        std::cerr << "ERROR send: "
                                  << std::strerror(errno) << '\n';

                        break;
                    }

                    total_sent += bytes_sent;
                }
            }
        }

        close(client_fd);
    }

    // Currently unreachable because the server loop is infinite.
    // Graceful shutdown will be handled in a future milestone.
    close(server_fd);
}