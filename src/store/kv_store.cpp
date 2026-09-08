#include "kv_store.hpp"

void KVStore::set(
    const std::string& key,
    const std::string& value
) {
    std::lock_guard<std::mutex> lock(mutex_);

    Entry entry;
    entry.value = value;
    entry.expires_at = std::nullopt;

    store_.insert_or_assign(key, entry);
}

std::optional<std::string> KVStore::get(
    const std::string& key
) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = store_.find(key);

    if (it == store_.end()) {
        return std::nullopt;
    }

    if (isExpired(it->second)) {
        return std::nullopt;
    }

    return it->second.value;
}

bool KVStore::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = store_.find(key);

    if (it == store_.end()) {
        return false;
    }

    if (isExpired(it->second)) {
        store_.erase(it);
        return false;
    }

    store_.erase(it);
    return true;
}

bool KVStore::contains(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = store_.find(key);

    if (it == store_.end()) {
        return false;
    }

    if (isExpired(it->second)) {
        return false;
    }

    return true;
}

std::size_t KVStore::size() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::size_t count = 0;

    for (const auto& [key, entry] : store_) {
        if (!isExpired(entry)) {
            ++count;
        }
    }

    return count;
}

bool KVStore::expire(
    const std::string& key,
    std::chrono::seconds ttl
) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = store_.find(key);

    if (it == store_.end()) {
        return false;
    }

    if (isExpired(it->second)) {
        store_.erase(it);
        return false;
    }

    if (ttl <= std::chrono::seconds::zero()) {
        store_.erase(it);
        return true;
    }

    it->second.expires_at =
        std::chrono::steady_clock::now() + ttl;

    return true;
}

bool KVStore::isExpired(const Entry& entry) const {
    if (!entry.expires_at.has_value()) {
        return false;
    }

    return std::chrono::steady_clock::now() >=
           entry.expires_at.value();
}