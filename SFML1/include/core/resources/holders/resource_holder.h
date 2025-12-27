// ================================================================================================
// File: core/resources/holders/resource_holder.h
// Purpose: Generic in-memory cache for SFML-based resources keyed by an ID type.
// Used by: ResourceManager and any engine subsystems that want per-ID resource caches.
// Notes:
//  - Stores resources as std::unique_ptr<Resource> to avoid accidental copies.
//  - Identifier is typically an enum class (TextureID, FontID, SoundID) or std::string.
//  - ResourceHolder does NOT log recoverable errors (avoids log duplication).
//    Fallback/LOG_WARN/LOG_PANIC policies are the responsibility of the ResourceManager.
//  - API getOrLoad(...) avoids redundant lookups: 1 lookup per hit.
// ================================================================================================
#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace core::resources::holders {

    /**
     * @brief Универсальный хранитель ресурсов (кэш по ID).
     *
     * Хранит ресурсы как std::unique_ptr<Resource> в
     * std::unordered_map<Identifier, std::unique_ptr<Resource>>.
     *
     * Требования к Resource:
     *  - должен предоставлять метод loadFromFile(const std::string& [, Args...]) -> bool.
     *
     * Требования к Identifier:
     *  - пригоден для std::unordered_map (std::hash + ==).
     */
    template <typename Resource, typename Identifier> class ResourceHolder {
      public:
        ResourceHolder() = default;
        ~ResourceHolder() = default;

        ResourceHolder(const ResourceHolder&) = delete;
        ResourceHolder& operator=(const ResourceHolder&) = delete;
        ResourceHolder(ResourceHolder&&) noexcept = default;
        ResourceHolder& operator=(ResourceHolder&&) noexcept = default;

        /**
         * @brief Получить ресурс или (при необходимости) загрузить его из файла.
         *
         * Семантика:
         *  - Если ресурс уже есть в кэше -> возвращаем его (loadedNow=false).
         *  - Если ресурса нет -> пробуем загрузить:
         *      - успех: вставляем в кэш, возвращаем указатель (loadedNow=true)
         *      - провал: кэш не меняем, возвращаем nullptr (loadedNow=false)
         *
         * Контракт:
         *  - Метод НЕ логирует ошибки загрузки (чтобы не было дубля логов).
         *  - Решение "warn/fallback/panic" принимает вызывающий код (обычно ResourceManager).
         */
        template <typename... Args>
        [[nodiscard]] std::pair<Resource*, bool>
        getOrLoad(const Identifier& id, const std::string& filename, Args&&... args);

        /**
         * @brief Загрузить ресурс из файла и привязать к идентификатору.
         *
         * Возвращает:
         *  - true  — ресурс был реально загружен впервые и добавлен в map;
         *  - false — ресурс уже был в кэше ИЛИ загрузка провалилась.
         *
         * ВАЖНО: чтобы отличить "уже был" от "провал загрузки" — используй tryGet(...) или
         * getOrLoad(...).
         */
        template <typename... Args>
        [[nodiscard]] bool load(const Identifier& id, const std::string& filename, Args&&... args);

        /**
         * @brief Вставить уже созданный ресурс (обычно после кастомной загрузки).
         *
         * Если resource == nullptr — вставка игнорируется.
         * Если ресурс с таким ID уже есть — в Debug срабатывает assert,
         * в Release — вставка игнорируется (сохраняется первый ресурс).
         */
        void insert(const Identifier& id, std::unique_ptr<Resource> resource);

        /**
         * @brief Быстрый "безопасный" доступ: вернуть указатель или nullptr.
         *
         * Это каноничный путь для менеджера, когда нужен fallback без исключений/UB.
         */
        [[nodiscard]] Resource* tryGet(const Identifier& id) noexcept;
        [[nodiscard]] const Resource* tryGet(const Identifier& id) const noexcept;

        /**
         * @brief Контрактный доступ (trust on read): вернуть ссылку или LOG_PANIC на ошибке.
         *
         * Используй только там, где отсутствие ресурса = programmer error.
         * ResourceManager обычно предпочитает tryGet/getOrLoad ради fallback-логики.
         */
        [[nodiscard]] Resource& get(const Identifier& id);
        [[nodiscard]] const Resource& get(const Identifier& id) const;

        [[nodiscard]] bool contains(const Identifier& id) const noexcept;

        void unload(const Identifier& id);
        void clear() noexcept;

        [[nodiscard]] std::size_t size() const noexcept {
            return mResourceMap.size();
        }

      private:
        std::unordered_map<Identifier, std::unique_ptr<Resource>> mResourceMap;
    };

} // namespace core::resources::holders

#include "resource_holder.inl"