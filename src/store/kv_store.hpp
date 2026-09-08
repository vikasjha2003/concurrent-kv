#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

class KVStore {
public:
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

    std::unordered_map<std::string, Entry> store_;
    mutable std::mutex mutex_;
};