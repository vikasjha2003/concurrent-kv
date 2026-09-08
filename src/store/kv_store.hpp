#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

class KVStore {
public:
    explicit KVStore(
        std::chrono::milliseconds sweep_interval =
            std::chrono::seconds(1)
    );

    ~KVStore();

    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    bool contains(const std::string& key) const;
    std::size_t size() const;

    bool expire(
        const std::string& key,
        std::chrono::seconds ttl
    );

private:
    struct Entry {
        std::string value;
        std::optional<std::chrono::steady_clock::time_point> expires_at;
    };

    bool isExpired(const Entry& entry) const;

    // Background sweep loop: runs on its own thread, periodically
    // removing expired entries so memory isn't held indefinitely
    // by keys nobody ever accesses again.
    void activeExpirationLoop();

    std::unordered_map<std::string, Entry> store_;
    mutable std::mutex mutex_;

    // --- Active expiration ---
    std::chrono::milliseconds sweep_interval_;
    std::thread sweeper_;

    // Dedicated mutex/cv pair for shutdown signaling only —
    // deliberately separate from mutex_ (which guards store_),
    // so the sweeper's sleep/wake logic never entangles with
    // client-facing store operations.
    std::atomic<bool> stop_{false};
    std::mutex cv_mutex_;
    std::condition_variable cv_;
};