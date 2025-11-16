#include "pch.h"
#include "core/resources/loader/resource_loader.h"

#include "core/utils/message.h"

namespace core::resources::loader {

    std::unique_ptr<types::TextureResource> ResourceLoader::loadTexture(const std::string& path,
                                                                        bool smooth) {
        auto resource = std::make_unique<types::TextureResource>();
        // Попытка загрузки из файла — используем универсальный метод loadFromFile у ресурса.
        if (!resource->loadFromFile(path)) {
            // В debug логируем подробности. В обычном потоке мы не показываем messagebox — только при серьёзной ошибке.
            utils::message::logDebug(
                std::string("[ResourceLoader]\nНе удалось загрузить текстуру: ") + path);
            return nullptr;
        }
        // Устанавливаем сглаживание по желанию (внутри TextureResource это действует на sf::Texture).
        resource->setSmooth(smooth);
        return resource;
    }

    std::unique_ptr<types::FontResource> ResourceLoader::loadFont(const std::string& path) {
        auto resource = std::make_unique<types::FontResource>();
        if (!resource->loadFromFile(path)) {
            utils::message::logDebug(std::string("[ResourceLoader]\nНе удалось загрузить шрифт: ") +
                                     path);
            return nullptr;
        }
        return resource;
    }

    std::unique_ptr<types::SoundBufferResource>
    ResourceLoader::loadSoundBuffer(const std::string& path) {
        auto resource = std::make_unique<types::SoundBufferResource>();
        if (!resource->loadFromFile(path)) {
            utils::message::logDebug(
                std::string("[ResourceLoader]\nНе удалось загрузить звуковой буфер: ") + path);
            return nullptr;
        }
        return resource;
    }

} // namespace core::resources::loader