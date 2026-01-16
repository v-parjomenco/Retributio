// ================================================================================================
// File: core/resources/registry/string_pool.h
// Purpose: Chunked string pool with load-time deduplication.
// ================================================================================================
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace core::resources::registry {

    class StringPool {
      public:
        static constexpr std::size_t DefaultChunkSize = 4u * 1024u * 1024u;

        StringPool() = default;
        ~StringPool() = default;

        StringPool(const StringPool&) = delete;
        StringPool& operator=(const StringPool&) = delete;
        StringPool(StringPool&&) noexcept = default;
        StringPool& operator=(StringPool&&) noexcept = default;

        [[nodiscard]] std::string_view intern(std::string_view sv);
        void clearLookup();

      private:
        struct StringViewHash {
            using is_transparent = void;

            [[nodiscard]] std::size_t operator()(std::string_view value) const noexcept {
                return std::hash<std::string_view>{}(value);
            }
        };

        struct StringViewEqual {
            using is_transparent = void;

            [[nodiscard]] bool operator()(std::string_view lhs,
                                          std::string_view rhs) const noexcept {
                return lhs == rhs;
            }
        };

        void allocateChunk(std::size_t neededBytes);

        std::vector<std::unique_ptr<char[]>> mChunks;
        char* mCurrentPtr = nullptr;
        std::size_t mRemaining = 0u;
        std::unordered_set<std::string_view, StringViewHash, StringViewEqual> mLookup;
    };

} // namespace core::resources::registry