#pragma once

#include <string>
#include <mutex>
#include <unordered_map>
#include <optional>

class KVStore {
public:
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    bool contains(const std::string& key) const;
    std::size_t size() const;

private:
    std::unordered_map<std::string, std::string> store_;
    mutable std::mutex mutex_;
};