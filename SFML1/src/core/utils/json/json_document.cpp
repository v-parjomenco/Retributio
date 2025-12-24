#include "pch.h"

#include "core/utils/json/json_document.h"

#include <unordered_set>

#include "core/log/log_macros.h"
#include "third_party/json/json_silent.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5045)
#endif

namespace {

    // Дубли ключей в JSON после парсинга уже не обнаружить:
    // nlohmann::json хранит только "последнее значение".
    // Поэтому ловим проблему НА ЭТАПЕ ПАРСИНГА через callback и считаем это ошибкой конфига.
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
                mObjectKeyStack.emplace_back();
                break;
            case Json::parse_event_t::object_end:
                mObjectKeyStack.pop_back();
                break;
            case Json::parse_event_t::key: {
                // nlohmann::json гарантирует, что parsed содержит string при событии key
                const auto* keyPtr = parsed.get_ptr<const std::string*>();
                
                auto& keys = mObjectKeyStack.back();
                const auto [it, inserted] = keys.emplace(*keyPtr);
                if (!inserted) {
                    throw DuplicateKeyError(*keyPtr);
                }
                break;
            }
            default:
                break;
            }

            return true;
        }

      private:
        // Хранение через string_view: строки живут в parsed объекте nlohmann::json
        // до завершения парсинга, поэтому копирование не требуется.
        struct StringViewHash {
            using is_transparent = void;
            [[nodiscard]] size_t operator()(std::string_view sv) const noexcept {
                return std::hash<std::string_view>{}(sv);
            }
        };

        std::vector<std::unordered_set<std::string_view, StringViewHash, std::equal_to<>>> mObjectKeyStack{};
    };

    [[nodiscard]] core::utils::json::json parseJsonStrict(const std::string& fileContent) {
        DuplicateKeyDetector detector{};
        return core::utils::json::json::parse(fileContent, detector);
    }

} // namespace

namespace core::utils::json {

    json parseAndValidateCritical(const std::string& fileContent,
                                  std::string_view path,
                                  const char* moduleTag,
                                  const std::vector<JsonValidator::KeyRule>& rules) {
        json data;

        // ----------------------------------------------------------------------------------------
        // Этап 1: Разбор (парсинг) JSON
        // ----------------------------------------------------------------------------------------

        try {
            data = ::parseJsonStrict(fileContent);
        } catch (const std::bad_alloc&) {
            // Нехватка памяти — это не "валидируемая" ошибка конфига.
            // Пусть пробрасывается наверх: caller может попытаться аккуратно завершиться.
            throw;
        } catch (const DuplicateKeyError& e) {
            // Дубликат ключа — ошибка конфига: иначе поведение зависит от порядка полей,
            // а валидатор структуры уже не сможет это обнаружить.
            LOG_PANIC(core::log::cat::Config,
                      "[{}] JSON содержит дублирующийся ключ '{}' в файле '{}'",
                      moduleTag,
                      e.key(),
                      path);
        } catch (const json::exception& e) {
            // JSON-специфичная ошибка парсинга (syntax error, parse error at line X, etc.).
            LOG_PANIC(core::log::cat::Config,
                      "[{}] Не удалось разобрать JSON в файле '{}': {}",
                      moduleTag,
                      path,
                      e.what());
        } catch (...) {
            // Неизвестная ошибка (не std::exception). В production это не должно происходить:
            // потенциально проблема в кастомном allocator/UB или внутренний баг библиотеки.
            LOG_PANIC(core::log::cat::Config,
                      "[{}] Неизвестная ошибка при разборе JSON в файле '{}'",
                      moduleTag,
                      path);
        }

        // ----------------------------------------------------------------------------------------
        // Этап 2: Валидация структуры
        // ----------------------------------------------------------------------------------------

        try {
            JsonValidator::validate(data, rules);
        } catch (const std::exception& e) {
            // Ошибка "схемной" валидации: missing key, wrong type, etc.
            // JsonValidator::validate бросает std::runtime_error, а не json::exception.
            LOG_PANIC(core::log::cat::Config,
                      "[{}] Неверная структура JSON в файле '{}': {}",
                      moduleTag,
                      path,
                      e.what());
        } catch (...) {
            // Страховка: не даём "не-std" исключениям утечь наружу в критическом пути.
            LOG_PANIC(core::log::cat::Config,
                      "[{}] Неизвестная ошибка при валидации структуры JSON в файле '{}'",
                      moduleTag,
                      path);
        }

        return data;
    }

    std::optional<json> parseAndValidateNonCritical(
        const std::string& fileContent,
        std::string_view path,
        const char* moduleTag,
        const std::vector<JsonValidator::KeyRule>& rules) {

        json data;

        // ----------------------------------------------------------------------------------------
        // Этап 1: Разбор (парсинг) JSON
        // ----------------------------------------------------------------------------------------

        try {
            data = ::parseJsonStrict(fileContent);
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
        // Этап 2: Валидация структуры
        // ----------------------------------------------------------------------------------------

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
            // Для NonCritical это принципиально: даже "не-std" исключение не должно валить игру.
            LOG_DEBUG(core::log::cat::Config,
                      "[{}] Неизвестная ошибка при валидации структуры JSON в файле '{}'",
                      moduleTag,
                      path);
            return std::nullopt;
        }

        return data;
    }

} // namespace core::utils::json

#ifdef _MSC_VER
#pragma warning(pop)
#endif