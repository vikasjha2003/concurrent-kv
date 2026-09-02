#pragma once // pragma once is just the modern version of ifndef, def. It ensures that imported only once

#include<string>
#include<unordered_map>
#include<optional> // I might have a value, or I might have nothing

class KVStore {
    public: 
        void set(const std::string& key, const std::string& value);
        std::optional<std::string>  get(const std::string& key) const;
        bool del(const std::string& key);
        bool contains(const std::string& key) const; // ensures that we won't modify KVStore inside this function
        std::size_t size() const;

    private:
        std::unordered_map<std::string, std::string> store_;
};