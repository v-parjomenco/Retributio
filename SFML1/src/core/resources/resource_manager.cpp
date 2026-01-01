#include "pch.h"

#include "core/resources/resource_manager.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

#include "core/log/log_macros.h"
#include "core/resources/paths/resource_paths.h"

namespace core::resources {

    namespace {

        using core::resources::config::FontResourceConfig;
        using core::resources::config::SoundResourceConfig;
        using core::resources::config::TextureResourceConfig;

#if defined(SFML1_PROFILE)
        // Генерация уникального ключа для дубликатов текстур в режиме стресс-теста.
        // Используем charconv для zero-allocation форматирования чисел.
        [[nodiscard]] std::string makeProfileStressTextureKey(ids::TextureID id) {
            using U = std::underlying_type_t<ids::TextureID>;
            static_assert(std::is_unsigned_v<U>, "TextureID underlying type must be unsigned.");

            constexpr std::size_t kPrefixLen = 3; // "psT"
            constexpr std::size_t kDigits    =
                static_cast<std::size_t>(std::numeric_limits<U>::digits10) + 1;
            std::array<char, kPrefixLen + kDigits> buf{};

            char* out = buf.data();
            *out++ = 'p';
            *out++ = 's';
            *out++ = 'T';

            const auto raw = static_cast<U>(id);
            const auto [ptr, ec] = std::to_chars(out, buf.data() + buf.size(), raw);

    #ifndef NDEBUG
            assert(ec == std::errc{} && 
                "[makeProfileStressTextureKey] to_chars failed unexpectedly.");
    #endif

            return std::string(buf.data(), ptr);
        }
#endif

        // --------------------------------------------------------------------------------------------
        // Generic helper для детерминированной предзагрузки ресурсов.
        // 
        // Назначение:
        //  - Извлекает все ID из конфигурационной мапы
        //  - Сортирует их по underlying enum value (детерминированный порядок)
        //  - Вызывает loadFunc для каждого ID в отсортированном порядке
        //
        // Плюсы детерминизма:
        //  - Стабильные логи загрузки (упрощает сравнение между запусками)
        //  - Воспроизводимые баги (одинаковый порядок аллокаций памяти)
        //  - Предсказуемое поведение loading screen
        //
        // Performance:
        //  - Вызывается 1 раз при старте (не hot path)
        //  - Overhead: allocate + sort — микросекунды даже на 1000 ресурсов
        //  - Runtime lookup остаётся O(1) через unordered_map
        // --------------------------------------------------------------------------------------------
        template <typename EnumID, typename ConfigMap, typename LoadFunc>
        void preloadAllSorted(const ConfigMap& configs, LoadFunc&& loadFunc) {
            std::vector<EnumID> sortedIds;
            sortedIds.reserve(configs.size());
            
            for (const auto& [id, _] : configs) {
                sortedIds.push_back(id);
            }
            
            // Сортировка по underlying enum value гарантирует детерминированный порядок.
            std::sort(sortedIds.begin(), sortedIds.end());
            
            for (const auto id : sortedIds) {
                loadFunc(id);
            }
        }

    } // namespace

    // --------------------------------------------------------------------------------------------
    // Bootstrapping (реестр ресурсов)
    // --------------------------------------------------------------------------------------------

    void ResourceManager::loadRegistryFromJson(std::string_view filename) {
        // Boundary: validate on write. Повторный вызов — ошибка жизненного цикла движка.
        if (mRegistryLoaded) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::loadRegistryFromJson] Registry уже загружен; "
                      "повторный вызов запрещён. filename='{}'",
                      filename);
        }

        // Критические ошибки (I/O, битый JSON) обрабатываются внутри ResourcePaths (LOG_PANIC).
        paths::ResourcePaths::loadFromJSON(std::string(filename));
        mRegistryLoaded = true;
    }

    // --------------------------------------------------------------------------------------------
    // Текстуры
    // --------------------------------------------------------------------------------------------

    const types::TextureResource& ResourceManager::getTexture(ids::TextureID id) {
#if defined(SFML1_PROFILE)
        // PROFILE stress mode:
        // При включении разрешаем "виртуальные" TextureID, которых нет в реестре.
        // Они становятся копиями sourceId в mDynamicTextures под уникальным ключом.
        // Важно: проверка должна быть ДО getTextureConfig(id), иначе ResourcePaths выдаст PANIC.
        if (mProfileStressTexturesEnabled &&
            id != ids::TextureID::Unknown &&
            id != mProfileStressSourceTextureId &&
            !paths::ResourcePaths::contains(id)) {

            const auto& srcCfg =
                paths::ResourcePaths::getTextureConfig(mProfileStressSourceTextureId);

            const std::string key = makeProfileStressTextureKey(id);

            auto [ptr, loadedNow] = mDynamicTextures.getOrLoad(key, srcCfg.path);
            if (!ptr) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceManager::getTexture(ProfileStress)] "
                          "Не удалось создать stress-текстуру key='{}' от source='{}'. path='{}'",
                          key,
                          ids::toString(mProfileStressSourceTextureId),
                          srcCfg.path);
            }

            if (loadedNow) {
                ptr->setSmooth(srcCfg.smooth);
                ptr->setRepeated(srcCfg.repeated);

                if (srcCfg.generateMipmap) {
                    if (!ptr->generateMipmap()) {
                        LOG_WARN(core::log::cat::Resources,
                                 "[ResourceManager::getTexture(ProfileStress)] "
                                 "Не удалось сгенерировать mipmap: {}",
                                 srcCfg.path);
                    }
                }
            }

            return *ptr;
        }
#endif

        // Контракт: TextureID должен быть зарегистрирован.
        const auto& cfg = paths::ResourcePaths::getTextureConfig(id);

        auto [ptr, loadedNow] = mTextures.getOrLoad(id, cfg.path);
        
        if (!ptr) {
            // Ошибка I/O или декодирования.
            // Принимаем решение о fallback или панике здесь (и только здесь).
            if (mHasMissingTextureFallback && id != mMissingTextureID) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getTexture(TextureID)] "
                         "Не удалось загрузить текстуру для ID: {}. path='{}'. "
                         "Используется fallback: {}",
                         ids::toString(id),
                         cfg.path,
                         ids::toString(mMissingTextureID));
                return getTexture(mMissingTextureID);
            }

            if (mHasMissingTextureFallback && id == mMissingTextureID) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceManager::getTexture(TextureID)] "
                          "Fallback-текстура не загрузилась. ID: {}, path='{}'",
                          ids::toString(id),
                          cfg.path);
            }

            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getTexture(TextureID)] "
                      "Не удалось загрузить текстуру. ID: {}, path='{}'. Fallback не задан.",
                      ids::toString(id),
                      cfg.path);
        }

        // Настройки применяем только при первой загрузке.
        if (loadedNow) {
            ptr->setSmooth(cfg.smooth);
            ptr->setRepeated(cfg.repeated);

            if (cfg.generateMipmap) {
                if (!ptr->generateMipmap()) {
                    LOG_WARN(core::log::cat::Resources,
                             "[ResourceManager::getTexture(TextureID)] "
                             "Не удалось сгенерировать mipmap: {}",
                             cfg.path);
                }
            }
        }

        return *ptr;
    }

    const types::TextureResource& ResourceManager::getTexture(const std::string& id) {
        // Строковый ID используется как путь к файлу.
        return getTextureByPath(id);
    }

    const types::TextureResource& ResourceManager::getTextureByPath(const std::string& path) {
        // Dynamic loading: загружаем текстуру по произвольному пути.
        auto [ptr, loadedNow] = mDynamicTextures.getOrLoad(path, path);
        
        if (!ptr) {
            if (mHasMissingTextureFallback) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getTextureByPath] "
                         "Не удалось загрузить текстуру по пути: '{}'. Используется fallback: {}",
                         path,
                         ids::toString(mMissingTextureID));
                return getTexture(mMissingTextureID);
            }

            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getTextureByPath] "
                      "Не удалось загрузить текстуру по пути: '{}'. Fallback не задан.",
                      path);
        }

        // Политика по умолчанию для динамических текстур: smooth=true, repeated=false.
        if (loadedNow) {
            ptr->setSmooth(true);
            ptr->setRepeated(false);
        }

        return *ptr;
    }

    // --------------------------------------------------------------------------------------------
    // Шрифты
    // --------------------------------------------------------------------------------------------

    const types::FontResource& ResourceManager::getFont(ids::FontID id) {
        const auto& cfg = paths::ResourcePaths::getFontConfig(id);

        auto [ptr, loadedNow] = mFonts.getOrLoad(id, cfg.path);
        (void)loadedNow;

        if (!ptr) {
            if (mHasMissingFontFallback && id != mMissingFontID) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getFont(FontID)] "
                         "Не удалось загрузить шрифт для ID: {}. path='{}'. "
                         "Используется fallback: {}",
                         ids::toString(id),
                         cfg.path,
                         ids::toString(mMissingFontID));
                return getFont(mMissingFontID);
            }

            if (mHasMissingFontFallback && id == mMissingFontID) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceManager::getFont(FontID)] "
                          "Fallback-шрифт не загрузился. ID: {}, path='{}'",
                          ids::toString(id),
                          cfg.path);
            }

            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getFont(FontID)] "
                      "Не удалось загрузить шрифт. ID: {}, path='{}'. Fallback не задан.",
                      ids::toString(id),
                      cfg.path);
        }

        return *ptr;
    }

    const types::FontResource& ResourceManager::getFont(const std::string& id) {
        auto [ptr, loadedNow] = mDynamicFonts.getOrLoad(id, id);
        (void)loadedNow;

        if (!ptr) {
            if (mHasMissingFontFallback) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getFont(std::string)] "
                         "Не удалось загрузить шрифт по пути: '{}'. "
                         "Используется fallback: {}",
                         id,
                         ids::toString(mMissingFontID));
                return getFont(mMissingFontID);
            }

            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getFont(std::string)] "
                      "Не удалось загрузить шрифт по пути: '{}'. Fallback не задан.",
                      id);
        }

        return *ptr;
    }

    // --------------------------------------------------------------------------------------------
    // Звуки
    // --------------------------------------------------------------------------------------------

    const types::SoundBufferResource& ResourceManager::getSound(ids::SoundID id) {
        if (auto* cached = mSounds.tryGet(id)) {
            return *cached;
        }

        if (!paths::ResourcePaths::contains(id)) {
            LOG_WARN(core::log::cat::Resources,
                     "[ResourceManager::getSound(SoundID)] "
                     "SoundID не зарегистрирован в реестре: {}. Возвращается пустой буфер.",
                     ids::toString(id));
            mSounds.insert(id, std::make_unique<types::SoundBufferResource>());
            return mSounds.get(id);
        }
        const auto& cfg = paths::ResourcePaths::getSoundConfig(id);

        auto [ptr, loadedNow] = mSounds.getOrLoad(id, cfg.path);
        (void)loadedNow;

        if (!ptr) {
            LOG_WARN(core::log::cat::Resources,
                     "[ResourceManager::getSound(SoundID)] "
                     "Не удалось загрузить звук для ID: {}. path='{}'. "
                     "Возвращается пустой буфер.",
                     ids::toString(id),
                     cfg.path);
            mSounds.insert(id, std::make_unique<types::SoundBufferResource>());
            return mSounds.get(id);
        }

        return *ptr;
    }

    const types::SoundBufferResource& ResourceManager::getSound(const std::string& id) {
        auto [ptr, loadedNow] = mDynamicSounds.getOrLoad(id, id);
        (void)loadedNow;

        if (!ptr) {
             LOG_WARN(core::log::cat::Resources,
                     "[ResourceManager::getSound(std::string)] "
                     "Не удалось загрузить звук по пути: '{}'. Возвращается пустой буфер.",
                     id);
            mDynamicSounds.insert(id, std::make_unique<types::SoundBufferResource>());
            return mDynamicSounds.get(id);
        }

        return *ptr;
    }

    // --------------------------------------------------------------------------------------------
    // Настройки Fallback
    // --------------------------------------------------------------------------------------------

    void ResourceManager::setMissingTextureFallback(ids::TextureID id) {
        mMissingTextureID          = id;
        mHasMissingTextureFallback = true;
        // Validate on write: fallback должен существовать.
        (void)getTexture(id);
    }

    void ResourceManager::setMissingFontFallback(ids::FontID id) {
        mMissingFontID          = id;
        mHasMissingFontFallback = true;
        (void)getFont(id);
    }

    // --------------------------------------------------------------------------------------------
    // Метрики
    // --------------------------------------------------------------------------------------------

    ResourceManager::ResourceMetrics ResourceManager::getMetrics() const noexcept {
        return ResourceMetrics{
            .textureCount        = mTextures.size(),
            .dynamicTextureCount = mDynamicTextures.size(),
            .fontCount           = mFonts.size(),
            .dynamicFontCount    = mDynamicFonts.size(),
            .soundCount          = mSounds.size(),
            .dynamicSoundCount   = mDynamicSounds.size(),
        };
    }

    // --------------------------------------------------------------------------------------------
    // API для предзагрузки ресурсов
    // --------------------------------------------------------------------------------------------

    void ResourceManager::preloadTexture(ids::TextureID id) { (void)getTexture(id); }
    void ResourceManager::preloadFont(ids::FontID id) { (void)getFont(id); }
    void ResourceManager::preloadSound(ids::SoundID id) { (void)getSound(id); }

    void ResourceManager::preloadAllTextures() {
        preloadAllSorted<ids::TextureID>(
            paths::ResourcePaths::getAllTextureConfigs(),
            [this](auto id) { (void)getTexture(id); }
        );
    }

    void ResourceManager::preloadAllFonts() {
        preloadAllSorted<ids::FontID>(
            paths::ResourcePaths::getAllFontConfigs(),
            [this](auto id) { (void)getFont(id); }
        );
    }

    void ResourceManager::preloadAllSounds() {
        preloadAllSorted<ids::SoundID>(
            paths::ResourcePaths::getAllSoundConfigs(),
            [this](auto id) { (void)getSound(id); }
        );
    }

    // --------------------------------------------------------------------------------------------
    // Управление жизненным циклом ресурсов
    // --------------------------------------------------------------------------------------------

    void ResourceManager::unloadTexture(ids::TextureID id) noexcept { mTextures.unload(id); }
    void ResourceManager::unloadTexture(const std::string& id) noexcept { 
        mDynamicTextures.unload(id);
    }
    void ResourceManager::unloadFont(ids::FontID id) noexcept { mFonts.unload(id); }
    void ResourceManager::unloadFont(const std::string& id) noexcept { mDynamicFonts.unload(id); }
    void ResourceManager::unloadSound(ids::SoundID id) noexcept { mSounds.unload(id); }
    void ResourceManager::unloadSound(const std::string& id) noexcept { mDynamicSounds.unload(id); }

    void ResourceManager::clearAll() noexcept {
        mTextures.clear();
        mFonts.clear();
        mSounds.clear();
        mDynamicTextures.clear();
        mDynamicFonts.clear();
        mDynamicSounds.clear();
    }

#if defined(SFML1_PROFILE)
    void ResourceManager::enableProfileStressTextureDuplication(ids::TextureID sourceId) noexcept {
        mProfileStressSourceTextureId = sourceId;
        mProfileStressTexturesEnabled = true;
    }

    void ResourceManager::disableProfileStressTextureDuplication() noexcept {
        mProfileStressTexturesEnabled = false;
    }
#endif

} // namespace core::resources