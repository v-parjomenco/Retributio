#pragma once

#include <type_traits>
#include <utility>
#include <stdexcept>
#include <string>

#include "core/resources/ids/resource_ids.h"
#include "core/resources/ids/resource_id_utils.h"

namespace core::resources::holders {
    // Загружаем ресурс из файла, с произвольными дополнительными параметрами,
    // которые могут форвардиться (передаваться дальше в том же виде, в котором получены)
    // в Resource::loadFromFile(filename, args...).
    template <typename Resource, typename Identifier>
    template <typename... Args> // вариативный шаблон typename... (число и тип аргументов заранее неизвестны)
    void ResourceHolder<Resource, Identifier>::load(const Identifier& id,
                                                    const std::string& filename,
                                                    Args&&... args) {

        // Проверка на дубликаты ID: если уже есть ресурс с таким ID — не загружаем снова.
        if (mResourceMap.find(id) != mResourceMap.end()) {
            // если поиск по id нашёл дубликат до конца контейнера mResourceMap, выходим, код ниже return не выполняется
            return;
        }

        // Создание уникального указателя на ресурс
        auto resource = std::make_unique<Resource>();

        // Попытка открыть файл, содержащий ресурс, с дополнительными параметрами:
        // loadFromFile() может принимать filename и, например, дополнительные настройки (например, sf::SoundBuffer).
        bool success = false;
        if constexpr (sizeof...(Args) == 0) {
            success = resource->loadFromFile(filename);
        } else {
            success = resource->loadFromFile(filename, std::forward<Args>(args)...);
        }

        if (!success) {
            throw std::runtime_error(
                std::string{ "[ResourceHolder::load]\nНе удалось загрузить файл: " } + filename
            );
        }

        // Вставляем загруженный ресурс в карту.
        // std::move(resource) передаёт владение ресурсом в контейнер mResouceMap.
        mResourceMap.emplace(id, std::move(resource));
    }


    // Получить ресурс (неконстантная версия)
    // Возвращает ссылку на сам объект (не указатель).
    template <typename Resource, typename Identifier>
    Resource& ResourceHolder<Resource, Identifier>::get(const Identifier& id) {
        auto found = mResourceMap.find(id);
        if (found == mResourceMap.end()) {
            throw std::runtime_error(
                std::string{ "[ResourceHolder::get]\nРесурс не найден: " } +
                core::resources::ids::idToString(id)
            );
        }
        return *found->second;
    }

    // Константная версия метода get
    template <typename Resource, typename Identifier>
    const Resource& ResourceHolder<Resource, Identifier>::get(const Identifier& id) const {
        auto found = mResourceMap.find(id);
        if (found == mResourceMap.end()) {
            throw std::runtime_error(
                std::string{ "[ResourceHolder::get]\nРесурс не найден: " } +
                core::resources::ids::idToString(id)
            );
        }
        return *found->second;
    }

    // Проверить наличие ресурса по ID.
    // noexcept подчёркивает, что метод не выбрасывает исключений и только делает поиск.
    template <typename Resource, typename Identifier>
    bool ResourceHolder<Resource, Identifier>::contains(const Identifier& id) const noexcept {
        return mResourceMap.find(id) != mResourceMap.end();
    }

    // Метод для удаления ресурса
    template <typename Resource, typename Identifier>
    void ResourceHolder<Resource, Identifier>::unload(const Identifier& id) {
        auto it = mResourceMap.find(id);
        if (it != mResourceMap.end()) {
            mResourceMap.erase(it);
        }
    }

    // Метод для полной очистки ресурсов mResourceMap
    template <typename Resource, typename Identifier>
    void ResourceHolder<Resource, Identifier>::clear() noexcept {
        mResourceMap.clear();
    }

    // Вставка уже загруженного ресурса через unique_ptr.
    // Используется, например, ResourceLoader'ом, чтобы избежать двойной загрузки с диска.
    // После вставки ResourceHolder будет владеть ресурсом.
    template <typename Resource, typename Identifier>
    void ResourceHolder<Resource, Identifier>::insert(const Identifier& id, std::unique_ptr<Resource> resource) {
        if (!resource) {
            return; // ничего не вставляем, если nullptr
        }
        if (mResourceMap.find(id) != mResourceMap.end()) {
            return; // уже есть — не перезаписываем текущий ресурс
        }
        mResourceMap.emplace(id, std::move(resource));
    }

} // namespace core::resources::holders