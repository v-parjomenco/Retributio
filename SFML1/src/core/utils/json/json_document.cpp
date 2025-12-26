#include "pch.h"

#include "core/utils/json/json_document.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/log/log_macros.h"
#include "third_party/json/json_silent.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5045)
#endif

namespace {

    class DuplicateKeyError final : public std::runtime_error {
      public:
        explicit DuplicateKeyError(std::string key)
            : std::runtime_error("Duplicate JSON key: " + key),
              mKey(std::move(key)) {
        }

        [[nodiscard]] std::string_view key() const noexcept {
            return mKey;
        }

      private:
        std::string mKey;
    };

    class DuplicateKeyDetector final {
      public:
        using Json = core::utils::json::json;

        DuplicateKeyDetector() {
            mObjectKeyStack.reserve(8);
        }

        bool operator()(int /*depth*/, Json::parse_event_t event, Json& parsed) {
            switch (event) {
            case Json::parse_event_t::object_start:
                if (mDepth == mObjectKeyStack.size()) {
                    mObjectKeyStack.emplace_back();
                }
                mObjectKeyStack[mDepth].beginObject();
                ++mDepth;
                break;

            case Json::parse_event_t::object_end:
#if !defined(NDEBUG)
                assert(mDepth > 0);
#endif
                --mDepth;
                break;

            case Json::parse_event_t::key: {
                const auto* keyPtr = parsed.get_ptr<const std::string*>();
#if !defined(NDEBUG)
                assert(keyPtr != nullptr);
                assert(!mObjectKeyStack.empty());
#endif
                mObjectKeyStack[mDepth - 1].insertOrThrow(*keyPtr);
                break;
            }

            default:
                break;
            }

            return true;
        }

      private:
        struct KeySet final {
            // Переключаемся на hash после 32 ключей:
            // - до этого линейный поиск выигрывает за счёт cache locality и отсутствия хешинга/нод
            // - 32 * sizeof(std::string) ~ 1KB (порядка L1-friendly, зависит от STL)
            static constexpr std::size_t kLinearLimit = 32;

            std::vector<std::string> linear{};
            std::unordered_set<std::string> hashed{};
            bool useHash = false;

            KeySet() {
                // Резервируем один раз на всю жизнь KeySet (реюз по глубине).
                linear.reserve(16);
            }

            void beginObject() {
                // Capacity намеренно сохраняем: если в JSON много похожих объектов,
                // мы избегаем повторных аллокаций.
                linear.clear();
                // Без ветвлений: состояние объекта всегда сбрасываем одинаково.
                // clear() сохраняет bucket'ы, это и есть desired reuse.
                hashed.clear();
                useHash = false;
            }

            void insertOrThrow(const std::string& key) {
                if (!useHash) {
                    if (std::find(linear.begin(), linear.end(), key) != linear.end()) {
                        throw DuplicateKeyError(key);
                    }

                    linear.push_back(key);

                    if (linear.size() > kLinearLimit) {
                        switchToHash();
                    }

                    return;
                }

                const auto [it, inserted] = hashed.emplace(key);
                if (!inserted) {
                    throw DuplicateKeyError(key);
                }
            }

            void switchToHash() {
                useHash = true;

                // Резервируем с запасом: переключаемся при ~33 ключах, но большие объекты
                // часто имеют 50–150 ключей. Цель — избежать rehash в процессе дальнейшего парсинга.
                const std::size_t expected = std::max<std::size_t>(128, linear.size() * 4);
                hashed.reserve(expected);

                for (std::string& s : linear) {
                    hashed.emplace(std::move(s));
                }

                // Обнуляем size, но сохраняем capacity как feature (повторное использование буфера).
                // Деструкция moved-from строк корректна и обычно дешева (часто SSO).
                linear.clear();
            }
        };

        std::vector<KeySet> mObjectKeyStack{};
        std::size_t mDepth = 0;
    };

    [[nodiscard]] core::utils::json::json parseJsonWithPolicies(
        const std::string& fileContent,
        core::utils::json::DuplicateKeyPolicy dupPolicy) {

        using Json = core::utils::json::json;

        if (dupPolicy == core::utils::json::DuplicateKeyPolicy::Ignore) {
            return Json::parse(fileContent);
        }

        DuplicateKeyDetector detector{};
        return Json::parse(fileContent, detector);
    }

} // namespace

namespace core::utils::json {

    json parseAndValidateCritical(const std::string& fileContent,
                                  std::string_view path,
                                  std::string_view moduleTag,
                                  std::span<const JsonValidator::KeyRule> rules,
                                  JsonParseOptions options) {
        json data;

        // ----------------------------------------------------------------------------------------
        // Этап 1: Разбор (парсинг) JSON
        // ----------------------------------------------------------------------------------------

        try {
            data = ::parseJsonWithPolicies(fileContent, options.duplicateKeys);
        } catch (const std::bad_alloc&) {
            // Нехватка памяти — не "валидируемая" ошибка конфига.
            throw;
        } catch (const DuplicateKeyError& e) {
            LOG_PANIC(core::log::cat::Config,
                      "[{}] JSON содержит дублирующийся ключ '{}' в файле '{}'",
                      moduleTag,
                      e.key(),
                      path);
        } catch (const json::exception& e) {
            LOG_PANIC(core::log::cat::Config,
                      "[{}] Не удалось разобрать JSON в файле '{}': {}",
                      moduleTag,
                      path,
                      e.what());
        } catch (...) {
            LOG_PANIC(core::log::cat::Config,
                      "[{}] Неизвестная ошибка при разборе JSON в файле '{}'",
                      moduleTag,
                      path);
        }

        // ----------------------------------------------------------------------------------------
        // Этап 2: Валидация структуры (опционально)
        // ----------------------------------------------------------------------------------------

        if (options.schema == SchemaPolicy::Validate) {
            try {
                JsonValidator::validate(data, rules);
            } catch (const std::exception& e) {
                LOG_PANIC(core::log::cat::Config,
                          "[{}] Неверная структура JSON в файле '{}': {}",
                          moduleTag,
                          path,
                          e.what());
            } catch (...) {
                LOG_PANIC(core::log::cat::Config,
                          "[{}] Неизвестная ошибка при валидации структуры JSON в файле '{}'",
                          moduleTag,
                          path);
            }
        }

        return data;
    }

    std::optional<json> parseAndValidateNonCritical(const std::string& fileContent,
                                                    std::string_view path,
                                                    std::string_view moduleTag,
                                                    std::span<const JsonValidator::KeyRule> rules,
                                                    JsonParseOptions options) {
        json data;

        // ----------------------------------------------------------------------------------------
        // Этап 1: Разбор (парсинг) JSON
        // ----------------------------------------------------------------------------------------

        try {
            data = ::parseJsonWithPolicies(fileContent, options.duplicateKeys);
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const DuplicateKeyError& e) {
            LOG_DEBUG(core::log::cat::Config,
                      "[{}] JSON содержит дублирующийся ключ '{}' в файле '{}'",
                      moduleTag,
                      e.key(),
                      path);
            return std::nullopt;
        } catch (const json::exception& e) {
            LOG_DEBUG(core::log::cat::Config,
                      "[{}] Не удалось разобрать JSON в файле '{}': {}",
                      moduleTag,
                      path,
                      e.what());
            return std::nullopt;
        } catch (...) {
            LOG_DEBUG(core::log::cat::Config,
                      "[{}] Неизвестная ошибка при разборе JSON в файле '{}'",
                      moduleTag,
                      path);
            return std::nullopt;
        }

        // ----------------------------------------------------------------------------------------
        // Этап 2: Валидация структуры (опционально)
        // ----------------------------------------------------------------------------------------

        if (options.schema == SchemaPolicy::Validate) {
            try {
                JsonValidator::validate(data, rules);
            } catch (const std::exception& e) {
                LOG_DEBUG(core::log::cat::Config,
                          "[{}] Неверная структура JSON в файле '{}': {}",
                          moduleTag,
                          path,
                          e.what());
                return std::nullopt;
            } catch (...) {
                LOG_DEBUG(core::log::cat::Config,
                          "[{}] Неизвестная ошибка при валидации структуры JSON в файле '{}'",
                          moduleTag,
                          path);
                return std::nullopt;
            }
        }

        return data;
    }

} // namespace core::utils::json

#ifdef _MSC_VER
#pragma warning(pop)
#endif
