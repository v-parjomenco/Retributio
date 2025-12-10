#pragma once

#include <type_traits>

#include "core/resources/ids/resource_id_utils.h"
#include "core/resources/ids/resource_ids.h"

namespace core::resources::holders {

    // --------------------------------------------------------------------------------------------
    // load(...)
    // --------------------------------------------------------------------------------------------
    // Загружаем ресурс из файла, с произвольными дополнительными параметрами,
    // которые форвардятся в Resource::loadFromFile(filename, args...).
    //
    // Возвращает:
    //  - true  — если ресурс был загружен впервые;
    //  - false — если ресурс с таким ID уже присутствовал и загрузка не потребовалась.
    // --------------------------------------------------------------------------------------------
    template <typename Resource, typename Identifier>
    template <typename... Args>
    [[nodiscard]] bool ResourceHolder<Resource, Identifier>::load(const Identifier& id,
                                                                  const std::string& filename,
                                                                  Args&&... args) {

        // Защита от дубликатов ID: если ресурс уже есть — не загружаем его повторно.
        if (mResourceMap.find(id) != mResourceMap.end()) {
            return false; // ресурс уже был в кэше
        }

        auto resource = std::make_unique<Resource>();

        // Унифицированный вызов loadFromFile:
        //  - при пустом Args... будет вызвана версия loadFromFile(const std::string&);
        //  - при наличии аргументов — соответствующая перегрузка/шаблон ресурса.
        if (!resource->loadFromFile(filename, std::forward<Args>(args)...)) {
            throw std::runtime_error(
                std::string("[ResourceHolder::load]\nНе удалось загрузить файл: ") + filename +
                "\nID: " + core::resources::ids::idToString(id));
        }

        // Вставляем загруженный ресурс в map:
        // std::move(resource) передаёт владение в mResourceMap.
        mResourceMap.emplace(id, std::move(resource));
        return true;
    }

    // --------------------------------------------------------------------------------------------
    // get(...)
    // --------------------------------------------------------------------------------------------

    // Константная версия метода get.
    // Возвращает ссылку на ресурс или выбрасывает std::runtime_error, если ресурс не найден.
    template <typename Resource, typename Identifier>
    const Resource& ResourceHolder<Resource, Identifier>::get(const Identifier& id) const {
        auto found = mResourceMap.find(id);
        if (found == mResourceMap.end()) {
            throw std::runtime_error(std::string("[ResourceHolder::get]\nРесурс не найден: ") +
                                     core::resources::ids::idToString(id));
        }
        return *found->second;
    }

    // Неконстантная версия get реализована через константную (DRY).
    template <typename Resource, typename Identifier>
    Resource& ResourceHolder<Resource, Identifier>::get(const Identifier& id) {
        return const_cast<Resource&>(static_cast<const ResourceHolder&>(*this).get(id));
    }

    // --------------------------------------------------------------------------------------------
    // contains(...)
    // --------------------------------------------------------------------------------------------

    // Проверить наличие ресурса по ID.
    // noexcept подчёркивает отсутствие исключений: только поиск в unordered_map.
    template <typename Resource, typename Identifier>
    [[nodiscard]] bool
    ResourceHolder<Resource, Identifier>::contains(const Identifier& id) const noexcept {        
        const auto it = mResourceMap.find(id);
        return it != mResourceMap.end();
    }

    // --------------------------------------------------------------------------------------------
    // unload(...)
    // --------------------------------------------------------------------------------------------

    // Удалить ресурс по ID (если его нет — ничего не происходит).
    template <typename Resource, typename Identifier>
    void ResourceHolder<Resource, Identifier>::unload(const Identifier& id) {
        mResourceMap.erase(id);
    }

    // --------------------------------------------------------------------------------------------
    // clear(...)
    // --------------------------------------------------------------------------------------------

    // Полная очистка хранилища.
    template <typename Resource, typename Identifier>
    void ResourceHolder<Resource, Identifier>::clear() noexcept {
        mResourceMap.clear();
    }

    // --------------------------------------------------------------------------------------------
    // insert(...)
    // --------------------------------------------------------------------------------------------

    // Вставка уже загруженного ресурса через unique_ptr.
    // Используется, например, ResourceLoader'ом, чтобы избежать двойной загрузки с диска.
    // После вставки ResourceHolder будет владеть ресурсом.
    //
    // Поведение при дубликате:
    //  - если ID уже существует, в Debug-сборке срабатывает assert (это подозрительная ситуация);
    //  - в Release-сборке дубликат тихо игнорируется, первый ресурс сохраняется.
    template <typename Resource, typename Identifier>
    void ResourceHolder<Resource, Identifier>::insert(const Identifier& id,
                                                      std::unique_ptr<Resource> resource) {
        if (!resource) {
            return; // ничего не вставляем, если nullptr
        }

        auto [it, inserted] = mResourceMap.emplace(id, std::move(resource));
        if (!inserted) {
            // В больших проектах дубликаты ID могут указывать на ошибки пайплайна ресурсов,
            // поэтому в Debug мы явно подсвечиваем это место.
            assert(false && "[ResourceHolder::insert] Resource with this ID already exists. "
                            "Repeated insertion ignored.");
        }
    }

} // namespace core::resources::holders