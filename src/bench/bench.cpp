#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Options {
    int thread_count = 4;
    int ops_per_thread = 1000;
    std::string host = "127.0.0.1";
    int port = 6380;
};

Options parseArgs(int argc, char** argv) {
    Options opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        auto nextArg = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << flag << '\n';
                std::exit(1);
            }
            return argv[++i];
        };

        if (arg == "--threads") {
            opts.thread_count = std::stoi(nextArg("--threads"));
        } else if (arg == "--ops") {
            opts.ops_per_thread = std::stoi(nextArg("--ops"));
        } else if (arg == "--host") {
            opts.host = nextArg("--host");
        } else if (arg == "--port") {
            opts.port = std::stoi(nextArg("--port"));
        } else {
            std::cerr << "Unknown argument: " << arg << '\n';
            std::exit(1);
        }
    }

    return opts;
}

// Opens a blocking TCP connection to host:port.
// Returns -1 on failure (and prints why).
int connectToServer(const std::string& host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        std::cerr << "socket() failed: " << std::strerror(errno) << '\n';
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "invalid host address: " << host << '\n';
        close(fd);
        return -1;
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        std::cerr << "connect() failed: " << std::strerror(errno) << '\n';
        close(fd);
        return -1;
    }

    return fd;
}

// Sends one line (appending '\n'), then blocks reading until a
// full line (up to '\n') has been received. Returns false on any
// I/O failure or unexpected disconnect.
bool sendAndReceive(int fd, const std::string& command, std::string& response_buffer) {
    std::string request = command + "\n";

    std::size_t total_sent = 0;
    while (total_sent < request.size()) {
        ssize_t sent = send(fd, request.data() + total_sent,
                             request.size() - total_sent, 0);
        if (sent == -1) {
            if (errno == EINTR) continue;
            return false;
        }
        total_sent += static_cast<std::size_t>(sent);
    }

    // response_buffer may already hold leftover bytes from a
    // previous call (pipelining isn't used here, but we keep this
    // robust in case a response and the next line arrive together).
    while (true) {
        auto newline_pos = response_buffer.find('\n');
        if (newline_pos != std::string::npos) {
            response_buffer.erase(0, newline_pos + 1);
            return true;
        }

        char buf[4096];
        ssize_t received = recv(fd, buf, sizeof(buf), 0);

        if (received == 0) {
            return false; // server closed connection
        }
        if (received == -1) {
            if (errno == EINTR) continue;
            return false;
        }

        response_buffer.append(buf, static_cast<std::size_t>(received));
    }
}

// Runs on its own thread. Opens one connection, performs `ops`
// SET/GET operations against thread-unique keys, and records each
// operation's round-trip latency in milliseconds.
void runClientWorker(
    int thread_id,
    int ops,
    const Options& opts,
    std::vector<double>& out_latencies_ms
) {
    int fd = connectToServer(opts.host, opts.port);
    if (fd == -1) {
        std::cerr << "Thread " << thread_id << ": failed to connect\n";
        return;
    }

    std::string response_buffer;
    out_latencies_ms.reserve(static_cast<std::size_t>(ops));

    for (int i = 0; i < ops; ++i) {
        std::string key = "bench_t" + std::to_string(thread_id) + "_k" + std::to_string(i);

        // Alternate SET then GET on the same key.
        std::string command = (i % 2 == 0)
            ? ("SET " + key + " value" + std::to_string(i))
            : ("GET bench_t" + std::to_string(thread_id) + "_k" + std::to_string(i - 1));

        auto start = std::chrono::steady_clock::now();

        if (!sendAndReceive(fd, command, response_buffer)) {
            std::cerr << "Thread " << thread_id << ": I/O failure at op " << i << '\n';
            break;
        }

        auto end = std::chrono::steady_clock::now();

        double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
        out_latencies_ms.push_back(latency_ms);
    }

    close(fd);
}

double percentile(const std::vector<double>& sorted_latencies, double p) {
    if (sorted_latencies.empty()) return 0.0;

    // Nearest-rank method: index = p/100 * (N - 1), rounded.
    double idx = (p / 100.0) * static_cast<double>(sorted_latencies.size() - 1);
    std::size_t lower = static_cast<std::size_t>(idx);
    std::size_t upper = std::min(lower + 1, sorted_latencies.size() - 1);
    double frac = idx - static_cast<double>(lower);

    return sorted_latencies[lower] +
           frac * (sorted_latencies[upper] - sorted_latencies[lower]);
}

} // namespace

int main(int argc, char** argv) {
    Options opts = parseArgs(argc, argv);

    std::cout << "Benchmarking " << opts.host << ":" << opts.port
              << " with " << opts.thread_count << " threads, "
              << opts.ops_per_thread << " ops/thread\n";

    // Each thread gets its own latency vector — avoids benchmark
    // threads contending on a shared vector, which would measure
    // the benchmark's own locking, not the server's.
    std::vector<std::vector<double>> per_thread_latencies(
        static_cast<std::size_t>(opts.thread_count)
    );

    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(opts.thread_count));

    auto wall_start = std::chrono::steady_clock::now();

    for (int t = 0; t < opts.thread_count; ++t) {
        workers.emplace_back(
            runClientWorker,
            t,
            opts.ops_per_thread,
            std::cref(opts),
            std::ref(per_thread_latencies[static_cast<std::size_t>(t)])
        );
    }

    // join(), not detach(): main() cannot compute results until
    // every worker has actually finished producing them, and there
    // is no reason for the benchmark process to keep running once
    // all workers are done.
    for (auto& w : workers) {
        w.join();
    }

    auto wall_end = std::chrono::steady_clock::now();
    double wall_seconds = std::chrono::duration<double>(wall_end - wall_start).count();

    // Merge all latencies into one vector for percentile reporting.
    std::vector<double> all_latencies;
    for (auto& v : per_thread_latencies) {
        all_latencies.insert(all_latencies.end(), v.begin(), v.end());
    }

    if (all_latencies.empty()) {
        std::cerr << "No operations completed successfully.\n";
        return 1;
    }

    std::sort(all_latencies.begin(), all_latencies.end());

    std::size_t completed_ops = all_latencies.size();
    double throughput = static_cast<double>(completed_ops) / wall_seconds;

    std::cout << "\n--- Results ---\n";
    std::cout << "Completed ops:  " << completed_ops << '\n';
    std::cout << "Wall time:      " << wall_seconds << " s\n";
    std::cout << "Throughput:     " << throughput << " ops/sec\n";
    std::cout << "Latency min:    " << all_latencies.front() << " ms\n";
    std::cout << "Latency p50:    " << percentile(all_latencies, 50) << " ms\n";
    std::cout << "Latency p95:    " << percentile(all_latencies, 95) << " ms\n";
    std::cout << "Latency p99:    " << percentile(all_latencies, 99) << " ms\n";
    std::cout << "Latency max:    " << all_latencies.back() << " ms\n";

    return 0;
}