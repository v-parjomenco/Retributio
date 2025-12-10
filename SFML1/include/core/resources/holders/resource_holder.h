// ================================================================================================
// File: core/resources/holders/resource_holder.h
// Purpose: Generic in-memory cache for SFML-based resources keyed by an ID type.
// Used by: ResourceManager and any engine subsystems that want per-ID resource caches.
// Notes:
//  - Stores resources as std::unique_ptr<Resource> to avoid accidental copies.
//  - Identifier is typically an enum class (TextureID, FontID, SoundID) or std::string.
//  - Designed to be lightweight and ready for future streaming/lazy-loading strategies.
//  - Fail-fast by default: missing resources приводят к std::runtime_error.
//    Fallback/“розовый квадрат” должен реализовываться уровнем выше (ResourceManager / игра).
// ================================================================================================
#pragma once

#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace core::resources::holders {

    /**
     * @brief Универсальный хранитель ресурсов.
     *
     * Хранит ресурсы как std::unique_ptr<Resource> в
     * std::unordered_map<Identifier, std::unique_ptr<Resource>>.
     *
     * Требования к Resource:
     *  - должен предоставлять метод loadFromFile(const std::string& [, Args...]);
     *  - обычно является тонкой обёрткой вокруг SFML-типа (sf::Texture, sf::Font, sf::SoundBuffer).
     *
     * Требования к Identifier:
     *  - должен быть пригоден для использования в std::unordered_map (т.е. иметь std::hash);
     *  - чаще всего это enum class (TextureID, FontID, SoundID) или std::string.
     *
     * Поведение:
     *  - load(...) загружает ресурс по ID и пути; при повторном вызове с тем же ID
     *    возвращает false и ничего не делает;
     *  - get(...) возвращает ссылку на ресурс или выбрасывает std::runtime_error, если его нет;
     *  - insert(...) вставляет уже созданный ресурс; дубликаты в Debug-сборке подсвечиваются assert'ом.
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
         * @brief Загрузить ресурс из файла и привязать к идентификатору.
         *
         * @tparam Args Дополнительные аргументы, которые будут проброшены в Resource::loadFromFile.
         *
         * @param id       Идентификатор ресурса.
         * @param filename Путь к файлу.
         * @param args     Дополнительные параметры для loadFromFile (если нужны конкретному типу).
         *
         * @return true  — ресурс был реально загружен впервые -> он добавляется в map.
         * @return false — ресурс с таким ID уже существовал -> загрузка не выполнялась,
         *                 map не изменяется.
         *
         * Если Resource::loadFromFile(...) вернул false (ошибка чтения/парсинга файла):
         *      * генерируется std::runtime_error и ресурс не добавляется в map.
         * 
         * ВАЖНО:
         *  - ResourceHolder сам ничего не логирует;
         *  - ответственность за логирование/фоллбеки лежит на вызывающем коде
         *    (обычно ResourceManager).
         */
        template <typename... Args>
        [[nodiscard]] bool load(const Identifier& id, const std::string& filename, Args&&... args);

        /**
         * @brief Вставить уже созданный ресурс (обычно после кастомной загрузки).
         *
         * Используется, когда загрузка производится через отдельный loader
         * (например, ResourceLoader::loadTexture) и мы хотим избежать повторного I/O.
         *
         * Если resource == nullptr — вставка игнорируется.
         * Если ресурс с таким ID уже есть — в Debug-сборке срабатывает assert,
         * в Release — вставка тихо игнорируется (сохраняется первый ресурс).
         */
        void insert(const Identifier& id, std::unique_ptr<Resource> resource);

        /// @brief Получить ресурс по ID (выбрасывает std::runtime_error, если не найден).
        [[nodiscard]] Resource& get(const Identifier& id);
        /// @brief Константная версия get.
        [[nodiscard]] const Resource& get(const Identifier& id) const;

        /// @brief Проверить, загружен ли ресурс с данным ID.
        [[nodiscard]] bool contains(const Identifier& id) const noexcept;

        /// @brief Удалить конкретный ресурс из кэша (если он есть).
        void unload(const Identifier& id);

        /// @brief Очистить все ресурсы данного хранилища.
        void clear() noexcept;

        /// @brief Количество ресурсов в хранилище (для отладки/профилинга).
        [[nodiscard]] std::size_t size() const noexcept {
            return mResourceMap.size();
        }

      private:
        std::unordered_map<Identifier, std::unique_ptr<Resource>> mResourceMap;
    };

} // namespace core::resources::holders

// Реализация шаблонов
#include "resource_holder.inl"