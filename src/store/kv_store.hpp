#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

class KVStore {
public:
    explicit KVStore(
        std::size_t capacity = 10000,
        std::chrono::milliseconds sweep_interval =
            std::chrono::seconds(1),
        std::string snapshot_path = "snapshot.db"
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

    // Writes all non-expired entries to snapshot_path_.
    // Returns false if the file could not be opened for writing.
    bool saveToFile() const;

    // Loads entries from snapshot_path_, if it exists.
    // Returns false if the file exists but could not be parsed;
    // returns true (no-op) if the file simply doesn't exist yet.
    bool loadFromFile();

private:
    using RecencyList = std::list<std::string>;

    struct Entry {
        std::string value;
        std::optional<std::chrono::steady_clock::time_point> expires_at;
        RecencyList::iterator recency_it;
    };

    bool isExpired(const Entry& entry) const;
    bool eraseKey(const std::string& key) const;
    void touch(Entry& entry, const std::string& key) const;
    void activeExpirationLoop();

    std::size_t capacity_;
    std::string snapshot_path_;

    // Both mutable: logically these are cache/bookkeeping
    // structures. Const public methods (get/contains) need to
    // update recency and lazily erase expired entries without
    // that being considered a violation of logical constness
    // from the caller's perspective.
    mutable std::unordered_map<std::string, Entry> store_;
    mutable RecencyList recency_;
    mutable std::mutex mutex_;

    std::chrono::milliseconds sweep_interval_;
    std::thread sweeper_;
    std::atomic<bool> stop_{false};
    std::mutex cv_mutex_;
    std::condition_variable cv_;
};