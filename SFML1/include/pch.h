#pragma once

// clang-format off

// Критически важные системные заголовки - строгий порядок
#include "core/compiler/warnings.h"
#include "core/compiler/platform/windows.h"

// clang-format on

// STD

#include <algorithm>     // содержит min, max и базовые алгоритмы, н-р сортировку
#include <cstdint>       // фиксированные целочисленные типы (uint32_t и т.п.)
#include <functional>    // уже используется и точно будет расти по применению (фабрики, коллбеки)
#include <memory>        // умные указатели, make_unique, etc.
#include <optional>      // очень удобен для API, которые могут "не вернуть значение"
#include <string>        // базовые строковые и текстовые операции
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