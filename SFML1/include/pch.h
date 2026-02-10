// ================================================================================================
// File: pch.h
// Purpose: Global precompiled header for SFML1 engine and games
// Used by: All translation units (via /Yu) to speed up builds
// Notes:
//  - Should include only commonly used STL/SFML headers and core compiler config.
//  - Must NOT pull in any game-specific headers or heavy modules.
// ================================================================================================
#pragma once

// КРИТИЧНО:
//  - Этот define ОБЯЗАН быть ДО любых EnTT includes (иначе ODR violation)
//  - pch.h гарантированно первый include во всех TU (forced /Yu в MSVC)
//
#include <cstdint>
#ifndef ENTT_ID_TYPE
#define ENTT_ID_TYPE std::uint64_t
#endif

// ------------------------------------------------------------------------------------------------
// clang-format off
// Критически важные системные заголовки - строгий порядок
#include "core/compiler/warnings.h"
#include "core/compiler/platform/windows.h"
// clang-format on

// STD
#include <algorithm>     // содержит min, max и базовые алгоритмы, н-р сортировку
#include <functional>    // уже используется и точно будет расти по применению (фабрики, коллбеки)
#include <memory>        // умные указатели, make_unique, etc.
#include <optional>      // очень удобен для API, которые могут "не вернуть значение"
#include <string>        // базовые строковые и текстовые операции
#include <string_view>   // лёгкие "вью" на строки (JSON-ключи, ID, без копирования)
#include <type_traits>   // метапрограммирование, SFINAE, enable_if, is_*
#include <unordered_map> // один из основных STL-контейнеров
#include <utility>       // вспомогательное std::move, std::pair и т.п.
#include <vector>        // один из основных STL-контейнеров

// SFML
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4868)
#endif
#include <SFML/Graphics.hpp> // подтягивает System и Window
#ifdef _MSC_VER
#pragma warning(pop)
#endif