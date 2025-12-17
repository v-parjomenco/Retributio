#include "pch.h"

#include "core/utils/json/json_document.h"

#include "core/log/log_macros.h"
#include "third_party/json/json_silent.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5045)
#endif

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
            data = json::parse(fileContent);
        } catch (const std::bad_alloc&) {
            // Нехватка памяти — это не "валидируемая" ошибка конфига.
            // Пусть пробрасывается наверх: caller может попытаться аккуратно завершиться.
            throw;
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
        } catch (const std::bad_alloc&) {
            throw;
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
            data = json::parse(fileContent);
        } catch (const std::bad_alloc&) {
            throw;
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
        } catch (const std::bad_alloc&) {
            throw;
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