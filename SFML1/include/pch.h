#pragma once

// Всегда первым — правила по ворнингам
#include "core/compiler/warnings.h"

// STD

#include <algorithm>		// содержит min, max и базовые алгоритмы, н-р сортировку
#include <cstdint>			// фиксированные целочисленные типы (uint32_t и т.п.)
#include <memory>			// умные указатели, make_unique, etc.
#include <string>			// базовые строковые и текстовые операции
#include <vector>			// один из основных STL-контейнеров
#include <unordered_map>	// один из основных STL-контейнеров
#include <utility>			// вспомогательное std::move, std::pair и т.п.
#include <type_traits>		// метапрограммирование, SFINAE, enable_if, is_*
#include <functional>		// уже используется и точно будет расти по применению (фабрики, коллбеки)
#include <optional>			// очень удобен для API, которые могут "не вернуть значение"

// SFML
#include <SFML/Graphics.hpp>	// подтягивает System и Window

// Глобальная конфигурация окна и базовых параметров игры
#include "core/config.h"