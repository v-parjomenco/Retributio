#include "pch.h"

#include "core/resources/resource_manager.h"
#include <stdexcept>

#include "core/resources/loader/resource_loader.h"

namespace core::resources {

    // --------------------------------------------------------------------------------------------
    // Текстуры
    // --------------------------------------------------------------------------------------------

    /**
    * @brief Статический ID из enum class в resourceIDs.
    *
    * Это основной, «движковый» путь доступа к ресурсам:
    *  - код оперирует TextureID (например, TextureID::Player),
    *  - фактический путь берётся из ResourcePaths::get(id),
    *  - сами пути описаны data/definitions/resources.json.
    *
    * При опечатке в идентификаторе (TextureID::Playre) ошибка будет на этапе компиляции,
    * а не в рантайме.
    */
    const types::TextureResource& ResourceManager::getTexture(ids::TextureID id, bool smooth) {
        if (!mTextures.contains(id)) {
            const std::string path = paths::ResourcePaths::get(id); // путь берём из JSON-реестра
            mTextures.load(id, path);
            // Флаг сглаживания применяется только при первой загрузке ресурса.
            mTextures.get(id).setSmooth(smooth);
        }
        return mTextures.get(id);
    }

    /**
    * @brief Динамический «ID» в виде строки.
    *
    * Используется для ресурсов, которые:
    *  - не описаны в enum'ах (моды, редакторы, прототипы),
    *  - загружаются по строковому ключу/пути в рантайме.
    *
    * В текущей реализации строка интерпретируется как прямой путь к файлу.
    * Это удобно для быстрых экспериментов, но не является каноническим путём движка
    * (основная система — TextureID + ResourcePaths).
    */
    const types::TextureResource& ResourceManager::getTexture(const std::string& id, bool smooth) {
        if (!mDynamicTextures.contains(id)) {
            mDynamicTextures.load(id, id); // здесь id = путь к файлу
            mDynamicTextures.get(id).setSmooth(smooth);
        }
        return mDynamicTextures.get(id);
    }

    /**
    * @brief Получение текстуры по явно переданному пути (path = строка к файлу).
    *
    * Это низкоуровневая загрузка «по прямому пути» (runtime):
    *  - используется, если путь приходит из внешнего источника (мод, редактор, сетевые данные),
    *  - полностью обходит систему enum / ResourcePaths.
    *
    * Рекомендуется для кода «на краю» (инструменты, моды), а не для ядра движка:
    * в самом движке предпочтителен путь через TextureID и ResourcePaths.
    */
    const types::TextureResource& ResourceManager::getTextureByPath(const std::string& path,
                                                                    bool smooth) {
        // Если ресурс с таким путём уже загружен (в динамическом контейнере) — вернём его
        if (!mDynamicTextures.contains(path)) {
            // Используем ResourceLoader, чтобы изолировать низкоуровневую загрузку
            auto texPtr = loader::ResourceLoader::loadTexture(path, smooth);
            if (!texPtr) {
                throw std::runtime_error(
                    std::string{
                        "[ResourceManager::getTextureByPath]\nНе удалось загрузить текстуру: "} +
                    path);
            }
            // Вставляем уже загруженный ресурс в ResourceHolder, чтобы избежать двойной загрузки с
            // диска.
            mDynamicTextures.insert(path, std::move(texPtr));
            // NB: ResourceLoader уже применил флаг smooth.
            // setSmooth вызывается только для ресурсов, загружаемых через ResourceHolder напрямую.
        }
        return mDynamicTextures.get(path);
    }

    // --------------------------------------------------------------------------------------------
    // Шрифты
    // --------------------------------------------------------------------------------------------

    /**
    * @brief Статический ID, из enum class в resourceIDs.
    * 
    * Канонический путь: FontID -> ResourcePaths::get(FontID) -> путь -> ResourceHolder.
    */
    const types::FontResource& ResourceManager::getFont(ids::FontID id) {
        if (!mFonts.contains(id)) {
            const std::string path = paths::ResourcePaths::get(id); // путь берём из JSON-реестра
            mFonts.load(id, path);
        }
        return mFonts.get(id);
    }

    /**
    * @brief Динамический шрифт по строковому «ID».
    *
    * Как и для текстур, строка в текущей реализации интерпретируется как путь к файлу.
    * Подходит для модов/инструментов, но не заменяет enum-базовую систему.
    */
    const types::FontResource& ResourceManager::getFont(const std::string& id) {
        if (!mDynamicFonts.contains(id)) {
            mDynamicFonts.load(id, id); // здесь id = путь к файлу
        }
        return mDynamicFonts.get(id);
    }

    // --------------------------------------------------------------------------------------------
    // Звуки
    // --------------------------------------------------------------------------------------------

    /**
    * @brief Статический ID, из enum class в resourceIDs.
    * 
    * Аналогично текстурам/шрифтам: SoundID -> ResourcePaths::get(SoundID) -> путь.
    */
    const types::SoundBufferResource& ResourceManager::getSound(ids::SoundID id) {
        if (!mSounds.contains(id)) {
            const std::string path = paths::ResourcePaths::get(id);
            mSounds.load(id, path);
        }
        return mSounds.get(id);
    }

    /**
    * @brief Динамический звук по строковому «ID»
    * 
    * (в текущей реализации — прямой путь).
    */
    const types::SoundBufferResource& ResourceManager::getSound(const std::string& id) {
        if (!mDynamicSounds.contains(id)) {
            mDynamicSounds.load(id, id); // здесь id = путь к файлу
        }
        return mDynamicSounds.get(id);
    }

} // namespace core::resources
