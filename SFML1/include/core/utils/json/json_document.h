// ================================================================================================
// File: core/utils/json/json_document.h
// Purpose: JSON document parsing + validation pipeline
// (NOT a DOM wrapper — just utility functions).
//          Provides parseAndValidateCritical/NonCritical for config loading.
// Used by: ConfigLoader, DebugOverlayLoader, WindowConfigLoader, EngineSettingsLoader.
// Notes:
//  - This is NOT a document class — just parsing+validation helper functions.
//  - "Document" refers to the conceptual JSON config file being loaded.
// ================================================================================================
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/utils/json/json_common.h"
#include "core/utils/json/json_validator.h"

namespace core::utils::json {

    // Парсинг JSON-текста + проверка структуры
    // (критический режим: LOG_PANIC (включающий в себя std::exit(EXIT_FAILURE) при ошибке).
    [[nodiscard]] json
    parseAndValidateCritical(const std::string& fileContent,
                             std::string_view path,
                             const char* moduleTag,
                             const std::vector<JsonValidator::KeyRule>& rules);

    // Парсинг JSON-текста + проверка структуры
    // (некритический режим: LOG_DEBUG + возврат nullopt при ошибке).
    [[nodiscard]] std::optional<json>
    parseAndValidateNonCritical(const std::string& fileContent,
                                std::string_view path,
                                const char* moduleTag,
                                const std::vector<JsonValidator::KeyRule>& rules);

} // namespace core::utils::json