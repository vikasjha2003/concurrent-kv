#include "kv_store.hpp"

void KVStore::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    store_.insert_or_assign(key, value);
}

std::optional<std::string> KVStore::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = store_.find(key);

    if (it == store_.end()) {
        return std::nullopt;
    }

    return it->second;
}

bool KVStore::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    return store_.erase(key) > 0;
}

bool KVStore::contains(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    return store_.find(key) != store_.end();
}

std::size_t KVStore::size() const {
    std::lock_guard<std::mutex> lock(mutex_);

    return store_.size();
}