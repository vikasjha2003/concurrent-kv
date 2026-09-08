#include "kv_store.hpp"

KVStore::KVStore(std::chrono::milliseconds sweep_interval)
    : sweep_interval_(sweep_interval) {

    // Start the background sweeper only after all other members
    // are constructed, since the thread body touches store_ and
    // the mutexes.
    sweeper_ = std::thread(&KVStore::activeExpirationLoop, this);
}

KVStore::~KVStore() {
    stop_.store(true);

    // Wake the sweeper immediately rather than waiting out its
    // current sleep interval.
    cv_.notify_all();

    if (sweeper_.joinable()) {
        sweeper_.join();
    }
}

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

void KVStore::activeExpirationLoop() {

    while (!stop_.load()) {

        {
            std::unique_lock<std::mutex> cv_lock(cv_mutex_);

            // Sleep until either sweep_interval_ elapses, or
            // stop_ becomes true (destructor calls notify_all()).
            cv_.wait_for(
                cv_lock,
                sweep_interval_,
                [this] { return stop_.load(); }
            );
        }

        if (stop_.load()) {
            break;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        for (auto it = store_.begin(); it != store_.end();) {
            if (isExpired(it->second)) {
                it = store_.erase(it);
            } else {
                ++it;
            }
        }
    }
}