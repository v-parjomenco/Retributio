#include "core/resources/loader/resource_loader.h"
#include "core/resources/resource_manager.h"

namespace core::resources {

    // Текстуры

    // Статический ID, из enum class в resourceIDs

    // Используется для всего, что известно заранее, т.е. базовых ресурсов игры (иконок, фона, GUI, спрайтов врагов, и т.д.)
    // Путь берётся из ResourcePaths::get(id) — то есть, из JSON - файла resources.json, где каждому enum присвоен путь;
    // Безопасно. В случае ошибки, компилятор скажет TextureID::Playre не существует.

        // Для базовых ресуросв движка
    const types::TextureResource& ResourceManager::getTexture(ids::TextureID id, bool smooth) {
        if (!mTextures.contains(id)) {
            const std::string path = paths::ResourcePaths::get(id); // путь берём из JSON
            mTextures.load(id, path);
            mTextures.get(id).setSmooth(smooth);
        }
        return mTextures.get(id);
    }

    // Динамический ID, из JSON

    // Используется, когда ресурсы уже описаны в JSON, но ключи - строки.
    // Например, есть динамический раздел "enemy_boss": "assets/textures/enemies/boss.png";
    // В этом случае id — это ключ, а не путь, но путь из этого ключа можно получить через ResourcePaths::get(id).
    // Просто метод getTexture(string) сейчас напрямую вызывает load(id, id),
    // т.к. в текущей реализации JSON не содержит «вложенных» структур.

    // Для кастомных JSON, с ключами - строками
    const types::TextureResource& ResourceManager::getTexture(const std::string& id, bool smooth) {
        if (!mDynamicTextures.contains(id)) {
            const std::string path = id; // id здесь уже строка пути
            mDynamicTextures.load(id, path);
            mDynamicTextures.get(id).setSmooth(smooth);
        }
        return mDynamicTextures.get(id);
    }

    // Получение текстуры по явному пути (путь = строка к файлу)

    // Низкоуровневая загрузка «по прямому пути» (runtime).
    // Используется, если путь задан в другом JSON (например, player.json),
    // но ты не знаешь заранее, какой именно спрайт указал дизайнер или мод;
    // это bypass(«обход») всей системы enum / ResourcePaths, чтобы просто взять текстуру по реальному файлу;
    // в кэше всё равно хранится в mDynamicTextures, поэтому второй раз не загрузится.

    // Для данных, загружаемых из внешних конфигов (runtime).
    const types::TextureResource& ResourceManager::getTextureByPath(const std::string& path, bool smooth) {
        // Если ресурс с таким путём уже загружен (в динамическом контейнере) — вернём его
        if (!mDynamicTextures.contains(path)) {
            // Используем ResourceLoader, чтобы изолировать низкоуровневую загрузку
            auto texPtr = loader::ResourceLoader::loadTexture(path, smooth);
            if (!texPtr) {
                throw std::runtime_error(std::string("[ResourceManager::getTextureByPath]\nНе удалось загрузить текстуру: ") + path);
            }
            // Вставляем уже загруженный ресурс в ResourceHolder, чтобы избежать двойной загрузки.
            mDynamicTextures.insert(path, std::move(texPtr));
            // NB: ResourceLoader уже применил флаг smooth.
            // setSmooth вызывается только для ресурсов, загружаемых через ResourceHolder напрямую.
        }
        return mDynamicTextures.get(path);
    }

    // Шрифты

    // Статический ID, из enum class в resourceIDs
    const types::FontResource& ResourceManager::getFont(ids::FontID id) {
        if (!mFonts.contains(id)) {
            const std::string path = paths::ResourcePaths::get(id); // путь берём из JSON
            mFonts.load(id, path);
        }
        return mFonts.get(id);
    }
    // Динамический ID, из JSON
    const types::FontResource& ResourceManager::getFont(const std::string& id) {
        if (!mDynamicFonts.contains(id)) {
            mDynamicFonts.load(id, id); // id = путь к файлу
        }
        return mDynamicFonts.get(id);
    }

    // Звуки

    // Статический ID, из enum class в resourceIDs
    const types::SoundBufferResource& ResourceManager::getSound(ids::SoundID id) {
        if (!mSounds.contains(id)) {
            const std::string path = paths::ResourcePaths::get(id);
            mSounds.load(id, path);
        }
        return mSounds.get(id);
    }
    // Динамический ID, из JSON
    const types::SoundBufferResource& ResourceManager::getSound(const std::string& id) {
        if (!mDynamicSounds.contains(id)) {
            mDynamicSounds.load(id, id); // id = путь к файлу
        }
        return mDynamicSounds.get(id);
    }

} // namespace core