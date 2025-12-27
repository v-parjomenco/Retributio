// ================================================================================================
// File: core/config/loader/detail/non_critical_config_report.h
// Purpose: Shared WARN aggregation for non-critical config loaders (single summary log).
// Used by: EngineSettingsLoader, DebugOverlayLoader, WindowConfigLoader.
// Notes:
//  - Stores only string_view keys (expected to be string literals / stable storage).
//  - Avoids index-based array access to prevent MSVC /Qspectre C5045 warnings.
//  - Bounded storage by design: this is for small human-authored configs;
//    loaders must static_assert that their known-key count fits.
// ================================================================================================
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "core/utils/json/json_parsers.h"

namespace core::config::loader::detail {

    class NonCriticalConfigReport final {
      public:
        // Жёсткий лимит на количество уникальных ключей В КАЖДОЙ категории
        // (invalid/semantic/clamped).
        // Это не hot-path, но нам важны:
        //  - ограниченный стек/память,
        //  - читаемые WARN summary,
        //  - явный контракт: loader обязан влезать (static_assert в TU loader-а).
        static constexpr std::size_t kMaxFields = 16;

        using FieldList = std::array<std::string_view, kMaxFields>;

        [[nodiscard]] bool hasAnyIssues() const noexcept {
            return (mInvalidCount + mSemanticCount + mClampedCount) > 0;
        }

        void addInvalidField(std::string_view key) noexcept {
            appendUnique(mInvalidFields, mInvalidCount, key);
        }

        void addSemanticInvalidField(std::string_view key) noexcept {
            appendUnique(mSemanticFields, mSemanticCount, key);
        }

        void addClampedField(std::string_view key) noexcept {
            appendUnique(mClampedFields, mClampedCount, key);
        }

        void emitWarnNoKnownKeys(std::string_view loaderTag, std::string_view path,
                                 std::string_view knownKeysHintText) const;

        void emitWarnPartialApply(std::string_view loaderTag, std::string_view path,
                                  std::string_view knownKeysHintText) const;

      private:
        static bool appendUnique(FieldList& list, std::size_t& count,
                                 std::string_view key) noexcept;

        FieldList mInvalidFields{};
        FieldList mSemanticFields{};
        FieldList mClampedFields{};
        std::size_t mInvalidCount = 0;
        std::size_t mSemanticCount = 0;
        std::size_t mClampedCount = 0;
    };

    template <typename TUInt>
    inline void noteUIntIssue(NonCriticalConfigReport& report,
                              std::string_view key,
                              const core::utils::json::UIntParseResult<TUInt>& result) noexcept {
        using Kind = core::utils::json::UnsignedParseIssue::Kind;

        if (result.issue.kind == Kind::None || result.issue.kind == Kind::MissingKey) {
            return;
        }

        report.addInvalidField(key);
    }

} // namespace core::config::loader::detail