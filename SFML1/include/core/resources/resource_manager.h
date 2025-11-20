// ================================================================================================
// File: core/resources/resource_manager.h
// Purpose: High-level interface for loading and caching engine resources
//          (textures, fonts, sounds) on top of ResourceHolder + ResourcePaths.
// Used by: Game, ECS systems (PlayerInitSystem, DebugOverlaySystem, UI, etc.)
// Related headers: i_resource.h, resource_holder.h, resource_ids.h, resource_paths.h
// ================================================================================================
#pragma once

#include <string>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include "core/resources/holders/resource_holder.h"
#include "core/resources/ids/resource_ids.h"
#include "core/resources/paths/resource_paths.h"
#include "core/resources/types/font_resource.h"
#include "core/resources/types/soundbuffer_resource.h"
#include "core/resources/types/texture_resource.h"

namespace core::resources {

    /**
     * @brief Высокоуровневый фасад над ResourceHolder + ResourcePaths.
     *
     * Обязанности:
     *  - Предоставляет простой API для запроса ресурсов по enum-ID
     *    (TextureID / FontID / SoundID).
     *  - Использует ResourcePaths, чтобы сопоставить enum-ID с реальным путём на диске
     *    (через assets/game/skyguard/config/resources.json).
     *  - Кэширует загруженные ресурсы и гарантирует, что каждый файл грузится с диска
     *    не более одного раза.
     *
     * Поддерживаются три режима доступа:
     *  1) Статические enum-ID       (TextureID / FontID / SoundID) — каноничный путь для движка.
     *  2) Динамические строковые ID (std::string)                  — для тулов, модов, прототипов.
     *  3) Явные пути к файлам       (getTextureByPath)             — низкоуровневый escape hatch.
     *
     * Ядро движка (геймплей, ECS-системы и т.п.) должно по максимуму использовать
     * вариант №1 — enum-ID + ResourcePaths. Остальные методы — вспомогательные.
     */
    class ResourceManager {
      public:
        ResourceManager() = default;
        ~ResourceManager() = default;

        // ----------------------------------------------------------------------------------------
        // Текстуры
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Каноничный способ получить текстуру: по статическому enum-ID.
         *
         *  - Путь берётся через ResourcePaths::get(TextureID) из assets/game/skyguard/config/resources.json.
         *  - Ресурс кэшируется во внутреннем ResourceHolder (mTextures).
         *
         * @param id     Идентификатор текстуры (например, TextureID::Player).
         * @param smooth Если true, к текстуре применяется setSmooth(true) сразу после загрузки.
         * 
         * Параметр smooth учитывается только при ПЕРВОЙ загрузке текстуры с данным ID:
         *  - если текстура ещё не загружена → будет загружена и к ней применится smooth;
         *  - если уже загружена → флаг smooth игнорируется, возвращается кэшированный ресурс.
         *
         * Если позже нужно изменить сглаживание, это делается напрямую через setSmooth(...)
         * на уже полученной sf::Texture (например, getTexture(id).get().setSmooth(false)).
         */
        const types::TextureResource& getTexture(ids::TextureID id, bool smooth = true);

        /**
         * @brief Получить текстуру по динамическому строковому ID.
         *
         * Текущее поведение:
         *  - Строка трактуется как прямой путь к файлу (id == filename).
         *  - Ресурс кэшируется в mDynamicTextures по этому строковому ключу.
         *
         * Предполагаемое использование:
         *  - редакторы, тулзы, прототипы, моддинг;
         *  - не предназначен как основной путь для геймплейного/движкового кода.
         */
        const types::TextureResource& getTexture(const std::string& id, bool smooth = true);

        /**
         * @brief Получить текстуру по явному пути к файлу.
         *
         * Это самый низкоуровневый вариант:
         *  - полностью обходится без ResourcePaths и enum-ID;
         *  - удобен, когда путь пришёл из внешнего источника (моды, сеть, временные JSON и т.п.).
         *
         * При этом текстура всё равно кэшируется в mDynamicTextures по строке-пути,
         * чтобы повторные вызовы с тем же путём не ходили на диск.
         *
         * В коде движка по возможности лучше использовать getTexture(TextureID).
         */
        const types::TextureResource& getTextureByPath(const std::string& path, bool smooth = true);

        // ----------------------------------------------------------------------------------------
        // Шрифты
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Получить шрифт по статическому enum-ID.
         *
         *  - Путь берётся через ResourcePaths::get(FontID).
         *  - Ресурс кэшируется в mFonts.
         */
        const types::FontResource& getFont(ids::FontID id);

        /**
         * @brief Получить шрифт по динамическому строковому ID
         *        (сейчас строка трактуется как путь к файлу).
         *
         * Логика аналогична getTexture(const std::string&):
         *  - полезно для тулов/модов;
         *  - движковому коду предпочтительнее enum-ID.
         */
        const types::FontResource& getFont(const std::string& id);

        // ----------------------------------------------------------------------------------------
        // Звуки
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Получить звуковой буфер по статическому enum-ID.
         *
         *  - Путь берётся через ResourcePaths::get(SoundID).
         *  - Ресурс кэшируется в mSounds.
         */
        const types::SoundBufferResource& getSound(ids::SoundID id);

        /**
         * @brief Получить звуковой буфер по динамическому строковому ID
         *        (сейчас строка трактуется как путь к файлу).
         *
         * Та же философия, что и для текстур/шрифтов: больше для модов и внешних данных.
         */
        const types::SoundBufferResource& getSound(const std::string& id);

      private:
        // Хранилища для статических enum-ID (основной путь для движка).
        holders::ResourceHolder<types::TextureResource, ids::TextureID> mTextures;
        holders::ResourceHolder<types::FontResource, ids::FontID> mFonts;
        holders::ResourceHolder<types::SoundBufferResource, ids::SoundID> mSounds;

        // Хранилища для динамических строковых ключей (тулы / моддинг / явные пути).
        holders::ResourceHolder<types::TextureResource, std::string> mDynamicTextures;
        holders::ResourceHolder<types::FontResource, std::string> mDynamicFonts;
        holders::ResourceHolder<types::SoundBufferResource, std::string> mDynamicSounds;
    };

} // namespace core::resources