#include "pch.h"

#include "core/resources/registry/string_pool.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace core::resources::registry {

    std::string_view StringPool::intern(std::string_view sv) {
        const auto it = mLookup.find(sv);
        if (it != mLookup.end()) {
            return *it;
        }

        const std::size_t neededBytes = sv.size() + 1u;
        if (mRemaining < neededBytes) {
            allocateChunk(neededBytes);
        }

        char* const dest = mCurrentPtr;
        if (!sv.empty()) {
            std::memcpy(dest, sv.data(), sv.size());
        }
        dest[sv.size()] = '\0';

        mCurrentPtr += neededBytes;
        mRemaining -= neededBytes;

        const std::string_view stored(dest, sv.size());
        mLookup.insert(stored);
        return stored;
    }

    void StringPool::clearLookup() {
        std::unordered_set<std::string_view, StringViewHash, StringViewEqual> empty;
        mLookup.swap(empty);
    }

    void StringPool::allocateChunk(std::size_t neededBytes) {
        const std::size_t capacity = std::max(DefaultChunkSize, neededBytes);

#if defined(__cpp_lib_make_unique) && (__cpp_lib_make_unique >= 201811L)
        auto chunk = std::make_unique_for_overwrite<char[]>(capacity);
#else
        auto chunk = std::unique_ptr<char[]>(new char[capacity]);
#endif

        mCurrentPtr = chunk.get();
        mRemaining = capacity;
        mChunks.push_back(std::move(chunk));
    }


} // namespace core::resources::registry