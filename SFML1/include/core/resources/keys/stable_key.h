// ================================================================================================
// File: core/resources/keys/stable_key.h
// Purpose: Stable resource key computation (xxHash3_64).
//
// Link-seam architecture (mirrors the panic_sink pattern):
//   computeStableKey64 is defined in sfml1_core (stable_key.cpp) and delegates to
//   detail::hashCanonicalKey, which is declared here but NOT defined in sfml1_core.
//
//   Definition is provided by a separate link unit:
//     production : core/resources/keys/stable_key_impl_xxhash.cpp  → sfml1_runtime / tools
//     tests      : tests/engine/src/stable_key_impl_seam.cpp       → sfml1_engine_tests
//
//   This allows tests to inject a deterministic constant hash and exercise the
//   StableKey64 collision-detection path without DI hooks inside sfml1_core.
// ================================================================================================
#pragma once

#include <cstdint>
#include <string_view>

namespace core::resources {

    /// Seed зафиксирован на 0 и является частью публичного контракта.
    /// Изменение ломает совместимость сейвов/модов — никогда не менять без версионирования.
    inline constexpr std::uint64_t StableKeySeed = 0u;

    /// Публичный API: вычислить стабильный 64-битный ключ для канонического имени ресурса.
    /// Делегирует в detail::hashCanonicalKey (резолвится в момент линковки).
    [[nodiscard]] std::uint64_t computeStableKey64(std::string_view canonicalKey) noexcept;

    namespace detail {

        /// Низкоуровневый хэш-примитив.
        ///
        /// Объявлен здесь, определяется link-time seam-юнитом — см. заголовок файла.
        /// Контракт:
        ///   - Чистая функция: одинаковый вход → одинаковый выход, без состояния.
        ///   - Использует StableKeySeed в качестве xxHash seed.
        ///   - noexcept: никогда не бросает исключения и не вызывает abort.
        ///   - Вызывается только из computeStableKey64 и тестового seam'а.
        [[nodiscard]] std::uint64_t hashCanonicalKey(std::string_view key) noexcept;

    } // namespace detail

} // namespace core::resources
