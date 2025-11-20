// ================================================================================================
// File: core/compiler/warnings.h
// Purpose: Centralized MSVC /Wall warning configuration for the engine.
// Used by: pch.h (indirectly by all translation units on MSVC builds)
// Notes:
//  - Disables noisy system/CRT warnings while keeping game code strict.
//  - Wrapped in _MSC_VER so other compilers ignore it cleanly.
// ================================================================================================
#pragma once

#ifdef _MSC_VER

// ================================================================================================
//  ГЛОБАЛЬНО ОТКЛЮЧАЕМЫЕ /Wall ВОРНИНГИ
// ================================================================================================

// "Подставляемая функция, удалена"
#pragma warning(disable : 4514) // unreferenced inline function removed

// "Специальный метод по умолчанию не создан / удалён" —
// часто спамит из-за =default/=delete и STL
#pragma warning(disable : 4623 4625 4626 5027)

// Макрос не определён — типичный шум от Windows.h и прочих
#pragma warning(disable : 4668)

// Padding между полями — информация, а не ошибка
#pragma warning(disable : 4820)

// "Функция/указатель с другой спецификацией исключений" —
// почти всегда шум из системных/WinAPI заголовков
#pragma warning(disable : 5039)

// Очень педантичные /Wall-ворнинги по инициализации/аннотациям
#pragma warning(disable : 5246 5267)

// Spectre info: компилятор просто сообщает, что МОГ БЫ
// вставить защиту при /Qspectre. Для игровой логики это шум.
#pragma warning(disable : 5045)

// Слишком болтливые "оптимизационные" ворнинги
// Фактически это не проблемы кода, а отчёт о том, что что-то не заинлайнилось.
#pragma warning(disable : 4710 4711)

// ================================================================================================
//  ПРИМЕР: МОЖНО ЧТО-ТО ПОВЫСИТЬ ДО ОШИБОК (ПО ЖЕЛАНИЮ)
//  (временно закрмментированно, но можно включить)
// ================================================================================================

// #pragma warning(error: 4189) // local variable initialized but not referenced
// #pragma warning(error: 4700 4701) // uninitialized local variable
// #pragma warning(error: 4706)      // assignment within conditional expression
// #pragma warning(error: 4834)      // discarding return value of 'nodiscard' function
// #pragma warning(error: 4996)      // deprecated/unsafe function (strcpy и т.п.)

// ВАЖНО: 4100, 4365, 4061 и т.п. лучше НЕ глушить глобально:
// 4100 — неиспользуемый параметр : лучше явно помечать его [[maybe_unused]] или(void) param;
// в конкретных местах(или локально push / disable / pop, например, в интерфейсных коллбеках).

// 4061 — непокрытый enum в switch.
// 4365 — signed / unsigned mismatch.

#endif // _MSC_VER