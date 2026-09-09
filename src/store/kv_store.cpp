#include "kv_store.hpp"

#include <cstdint>
#include <fstream>
#include<vector>

namespace {

// --- Small binary I/O helpers for the snapshot format ---
// Format per entry:
//   uint32_t key_len,   then key_len bytes
//   uint32_t value_len, then value_len bytes
//   uint8_t  has_expiry
//   int64_t  remaining_ms   (only present if has_expiry == 1)
// File begins with:
//   uint32_t entry_count

void writeString(std::ofstream& out, const std::string& s) {
    std::uint32_t len = static_cast<std::uint32_t>(s.size());
    out.write(reinterpret_cast<const char*>(&len), sizeof(len));
    out.write(s.data(), len);
}

bool readString(std::ifstream& in, std::string& out) {
    std::uint32_t len = 0;
    in.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (!in) return false;

    out.resize(len);
    if (len > 0) {
        in.read(&out[0], len);
        if (!in) return false;
    }
    return true;
}

} // namespace

KVStore::KVStore(
    std::size_t capacity,
    std::chrono::milliseconds sweep_interval,
    std::string snapshot_path
)
    : capacity_(capacity),
      snapshot_path_(std::move(snapshot_path)),
      sweep_interval_(sweep_interval) {

    sweeper_ = std::thread(&KVStore::activeExpirationLoop, this);
}

KVStore::~KVStore() {
    stop_.store(true);
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

    auto it = store_.find(key);

    if (it != store_.end()) {
        it->second.value = value;
        it->second.expires_at = std::nullopt;
        touch(it->second, key);
        return;
    }

    recency_.push_front(key);

    Entry entry;
    entry.value = value;
    entry.expires_at = std::nullopt;
    entry.recency_it = recency_.begin();

    store_.emplace(key, std::move(entry));

    if (store_.size() > capacity_) {
        const std::string& victim = recency_.back();
        store_.erase(victim);
        recency_.pop_back();
    }
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
        eraseKey(key);
        return std::nullopt;
    }

    touch(it->second, key);
    return it->second.value;
}

bool KVStore::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return eraseKey(key);
}

bool KVStore::contains(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = store_.find(key);

    if (it == store_.end()) {
        return false;
    }

    if (isExpired(it->second)) {
        eraseKey(key);
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
        eraseKey(key);
        return false;
    }

    if (ttl <= std::chrono::seconds::zero()) {
        eraseKey(key);
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

bool KVStore::eraseKey(const std::string& key) const {
    auto it = store_.find(key);
    if (it == store_.end()) {
        return false;
    }
    recency_.erase(it->second.recency_it);
    store_.erase(it);
    return true;
}

void KVStore::touch(Entry& entry, const std::string& key) const {
    recency_.splice(recency_.begin(), recency_, entry.recency_it);
    entry.recency_it = recency_.begin();
}

void KVStore::activeExpirationLoop() {
    while (!stop_.load()) {
        {
            std::unique_lock<std::mutex> cv_lock(cv_mutex_);
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
                recency_.erase(it->second.recency_it);
                it = store_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

bool KVStore::saveToFile() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream out(snapshot_path_, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }

    // Count non-expired entries first, since we skip expired
    // ones and need an accurate header count.
    std::uint32_t count = 0;
    for (const auto& key : recency_) {
        const auto& entry = store_.at(key);
        if (!isExpired(entry)) {
            ++count;
        }
    }

    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    // Write MRU-first (recency_ is already in that order).
    auto now = std::chrono::steady_clock::now();

    for (const auto& key : recency_) {
        const auto& entry = store_.at(key);
        if (isExpired(entry)) {
            continue;
        }

        writeString(out, key);
        writeString(out, entry.value);

        if (entry.expires_at.has_value()) {
            std::uint8_t has_expiry = 1;
            out.write(reinterpret_cast<const char*>(&has_expiry), sizeof(has_expiry));

            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                entry.expires_at.value() - now
            );
            std::int64_t remaining_ms = remaining.count();
            out.write(reinterpret_cast<const char*>(&remaining_ms), sizeof(remaining_ms));
        } else {
            std::uint8_t has_expiry = 0;
            out.write(reinterpret_cast<const char*>(&has_expiry), sizeof(has_expiry));
        }
    }

    return static_cast<bool>(out);
}

bool KVStore::loadFromFile() {
    std::ifstream in(snapshot_path_, std::ios::binary);
    if (!in) {
        // No snapshot file yet — not an error, just nothing to load.
        return true;
    }

    std::uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in) {
        return false;
    }

    struct Loaded {
        std::string key;
        std::string value;
        bool has_expiry;
        std::int64_t remaining_ms;
    };

    std::vector<Loaded> loaded;
    loaded.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        Loaded item;

        if (!readString(in, item.key)) return false;
        if (!readString(in, item.value)) return false;

        std::uint8_t has_expiry = 0;
        in.read(reinterpret_cast<char*>(&has_expiry), sizeof(has_expiry));
        if (!in) return false;
        item.has_expiry = (has_expiry != 0);

        if (item.has_expiry) {
            in.read(reinterpret_cast<char*>(&item.remaining_ms), sizeof(item.remaining_ms));
            if (!in) return false;
        }

        loaded.push_back(std::move(item));
    }

    // File was written MRU-first. Insert in reverse (LRU-first)
    // so that repeated set() calls (each pushing to front) end
    // with the original MRU key at the front again.
    for (auto it = loaded.rbegin(); it != loaded.rend(); ++it) {
        set(it->key, it->value);

        if (it->has_expiry) {
            if (it->remaining_ms <= 0) {
                // Already expired relative to now — drop it.
                del(it->key);
            } else {
                expire(
                    it->key,
                    std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::milliseconds(it->remaining_ms)
                    )
                );
            }
        }
    }

    return true;
}