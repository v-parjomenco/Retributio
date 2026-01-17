#include "pch.h"

#include "core/config/loader/detail/non_critical_config_report.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <string_view>

#include "core/log/log_macros.h"

namespace {

    using core::config::loader::detail::NonCriticalConfigReport;

    static constexpr std::string_view kEmptyList = "-";
    static constexpr std::string_view kListSeparator = ", ";
    static constexpr std::string_view kListTruncate = "...";

    // Оценка сверху на длину имени ключа для компактного WARN summary.
    static constexpr std::size_t kEstimatedMaxKeyLength = 24;

    static constexpr std::size_t kListBufferSize =
        (NonCriticalConfigReport::kMaxFields * kEstimatedMaxKeyLength) +
        ((NonCriticalConfigReport::kMaxFields - 1) * kListSeparator.size()) + kListTruncate.size();

    static_assert(kListBufferSize <= 1024,
                  "List buffer too large; revisit kEstimatedMaxKeyLength/kMaxFields.");

    struct ListBuffer final {
        std::array<char, kListBufferSize> data{};
        std::size_t size = 0;
    };

    [[nodiscard]] bool appendToBuffer(ListBuffer& buffer, std::string_view text) noexcept {
        const std::size_t remaining = buffer.data.size() - buffer.size;
        if (remaining < text.size()) {
            return false;
        }

        std::copy_n(text.data(), text.size(), buffer.data.data() + buffer.size);
        buffer.size += text.size();
        return true;
    }

    std::string_view buildList(const NonCriticalConfigReport::FieldList& list,
                               const std::size_t count, ListBuffer& buffer) noexcept {
        if (count == 0) {
            return kEmptyList;
        }

        auto it = list.begin();
        auto end = list.begin();
        std::advance(end, static_cast<std::ptrdiff_t>(count));

        // Резервируем место для маркера усечения, чтобы гарантировать, что он всегда поместится
        static constexpr std::size_t kTruncateReserve = kListTruncate.size();
        static constexpr std::size_t kMaxUsableSize = kListBufferSize - kTruncateReserve;

        auto appendOrTruncate = [&](std::string_view text) noexcept -> bool {
            if (buffer.size + text.size() > kMaxUsableSize) {
                [[maybe_unused]] const bool truncated = appendToBuffer(buffer, kListTruncate);
                assert(truncated && "Truncate marker must fit by design");
                return false;
            }

            [[maybe_unused]] const bool appended = appendToBuffer(buffer, text);
            assert(appended && "Append must succeed within reserved capacity");
            return true;
        };

        bool first = true;

        for (; it != end; ++it) {
            if (!first) {
                if (!appendOrTruncate(kListSeparator)) {
                    break;
                }
            }
            first = false;

            if (!appendOrTruncate(*it)) {
                break;
            }
        }

        return std::string_view{buffer.data.data(), buffer.size};
    }

} // namespace

namespace core::config::loader::detail {

    bool NonCriticalConfigReport::appendUnique(FieldList& list, std::size_t& count,
                                               std::string_view key) noexcept {
        if (key.empty()) {
            return false;
        }

        // Без этого можно получить UB на std::advance/*endIt
        // при нарушении контракта (например, новый loader без static_assert).
        if (count >= list.size()) {
            assert(false && "NonCriticalConfigReport overflow: increase kMaxFields or fix loader");
            return false;
        }

        auto endIt = list.begin();
        std::advance(endIt, static_cast<std::ptrdiff_t>(count));

        if (std::find(list.begin(), endIt, key) != endIt) {
            return false;
        }

        *endIt = key;
        ++count;
        return true;
    }

    void NonCriticalConfigReport::emitWarnNoKnownKeys(std::string_view loaderTag,
                                                      std::string_view path,
                                                      std::string_view knownKeysHintText) const {
        LOG_WARN(core::log::cat::Config,
                 "[{}] Конфиг '{}' прочитан, но не содержит ни одного известного ключа ({}). "
                 "Используются значения по умолчанию.",
                 loaderTag, path, knownKeysHintText);
    }

    void NonCriticalConfigReport::emitWarnPartialApply(std::string_view loaderTag,
                                                       std::string_view path,
                                                       std::string_view knownKeysHintText) const {
        ListBuffer invalidBuffer{};
        ListBuffer semanticBuffer{};
        ListBuffer clampedBuffer{};

        const std::string_view invalidText =
            buildList(mInvalidFields, mInvalidCount, invalidBuffer);
        const std::string_view semanticText =
            buildList(mSemanticFields, mSemanticCount, semanticBuffer);
        const std::string_view clampedText =
            buildList(mClampedFields, mClampedCount, clampedBuffer);

        LOG_WARN(core::log::cat::Config,
                 "[{}] Конфиг '{}' применён частично: invalid={}, semantic={}, clamped={}. "
                 "Используются дефолты/ограничения. Ожидаемые ключи: {}.",
                 loaderTag, path, invalidText, semanticText, clampedText, knownKeysHintText);
    }

} // namespace core::config::loader::detail