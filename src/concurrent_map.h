#pragma once

#include <map>
#include <vector>
#include <mutex>
#include <string>

template <typename Key, typename Value>
class ConcurrentMap {
public:

    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : concurrent_map_(bucket_count) {}

    Access operator[](const Key& key) {
        auto& bucket_for_return = concurrent_map_[static_cast<uint64_t>(key) % concurrent_map_.size()];
        return { std::lock_guard(bucket_for_return.mut), bucket_for_return.sub_map[key] };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mutex, map] : concurrent_map_) {
            std::lock_guard g(mutex);
            result.insert(map.begin(), map.end());
        }
        return result;
    }

    void erase(const Key& key) {
        auto& bucket_for_erase = concurrent_map_[static_cast<uint64_t>(key) % concurrent_map_.size()];
        std::lock_guard g(bucket_for_erase.mut);
        bucket_for_erase.sub_map.erase(key);
    }
private:
    struct Bucket {
        std::mutex mut;
        std::map<Key, Value> sub_map;
    };

    std::vector<Bucket> concurrent_map_;
};
