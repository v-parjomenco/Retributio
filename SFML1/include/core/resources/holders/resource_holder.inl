#pragma once

#include <type_traits>
#include <utility>

#include "core/log/log_macros.h"

namespace core::resources::holders {

    // --------------------------------------------------------------------------------------------
    // getOrLoad(...)
    // --------------------------------------------------------------------------------------------

    template <typename Resource, typename Identifier>
    template <typename... Args>
    [[nodiscard]] std::pair<Resource*, bool>
    ResourceHolder<Resource, Identifier>::getOrLoad(const Identifier& id,
                                                    const std::string& filename,
                                                    Args&&... args) {

        // 1 lookup на hit:
        //  - try_emplace не конструирует value, если ключ уже существует
        //  - создает "пустой" слот (nullptr), если ключа нет (мы обязаны его заполнить или удалить)
        auto [it, inserted] = mResourceMap.try_emplace(id, nullptr);

        // Если вставка не произошла — значит ресурс уже был
        if (!inserted) {
            // Инвариант: в кэше не должно быть nullptr.
            // Если это случилось — значит кто-то нарушил контракт (например, insert(id, nullptr)).
            assert(it->second && 
                   "[ResourceHolder::getOrLoad] Invariant broken: nullptr stored in cache.");
            return {it->second.get(), false};
        }

        // inserted == true: ключ добавлен, value пока nullptr.
        // Пытаемся загрузить ресурс.
        auto resource = std::make_unique<Resource>();

        if (!resource->loadFromFile(filename, std::forward<Args>(args)...)) {
            // Ошибка I/O или парсинга.
            // Откатываем вставку ключа, чтобы не держать мусор (nullptr) в map.
            mResourceMap.erase(it);
            // Логирование/фоллбек/паника — строго на уровне ResourceManager.
            return {nullptr, false};
        }

        // Загрузка успешна — перемещаем владение в map.
        it->second = std::move(resource);
        return {it->second.get(), true};
    }

    // --------------------------------------------------------------------------------------------
    // load(...)
    // --------------------------------------------------------------------------------------------

    template <typename Resource, typename Identifier>
    template <typename... Args>
    [[nodiscard]] bool ResourceHolder<Resource, Identifier>::load(const Identifier& id,
                                                                  const std::string& filename,
                                                                  Args&&... args) {

        auto [ptr, loadedNow] = getOrLoad(id, filename, std::forward<Args>(args)...);
        // Возвращаем true только если загрузка прошла успешно И это был новый ресурс.
        return ptr && loadedNow;
    }

    // --------------------------------------------------------------------------------------------
    // insert(...)
    // --------------------------------------------------------------------------------------------

    template <typename Resource, typename Identifier>
    void ResourceHolder<Resource, Identifier>::insert(const Identifier& id,
                                                      std::unique_ptr<Resource> resource) {
        if (!resource) {
            return;
        }

        // try_emplace: один lookup, mapped_value не трогаем на hit.
        auto [it, inserted] = mResourceMap.try_emplace(id, nullptr);

        if (!inserted) {
            // В Debug подсвечиваем попытку перезаписать существующий ресурс.
            // В Release просто игнорируем (first wins).
            assert(false && "[ResourceHolder::insert] Resource with this ID already exists.");
            return;
        }

        // Успешно зарезервировали слот — заполняем.
        it->second = std::move(resource);
    }

    // --------------------------------------------------------------------------------------------
    // tryGet(...)
    // --------------------------------------------------------------------------------------------

    template <typename Resource, typename Identifier>
    [[nodiscard]] Resource*
    ResourceHolder<Resource, Identifier>::tryGet(const Identifier& id) noexcept {
        const auto it = mResourceMap.find(id);
        return (it != mResourceMap.end()) ? it->second.get() : nullptr;
    }

    template <typename Resource, typename Identifier>
    [[nodiscard]] const Resource*
    ResourceHolder<Resource, Identifier>::tryGet(const Identifier& id) const noexcept {
        const auto it = mResourceMap.find(id);
        return (it != mResourceMap.end()) ? it->second.get() : nullptr;
    }

    // --------------------------------------------------------------------------------------------
    // get(...)
    // --------------------------------------------------------------------------------------------

    template <typename Resource, typename Identifier>
    const Resource& ResourceHolder<Resource, Identifier>::get(const Identifier& id) const {
        const Resource* ptr = tryGet(id);
        assert(ptr && "[ResourceHolder::get] Missing resource (contract violation).");

        if (!ptr) {
            // Trust-on-Read нарушен: это фатальная ошибка логики.
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceHolder::get] Ресурс не найден (нарушение контракта).");
        }

        return *ptr;
    }

    template <typename Resource, typename Identifier>
    Resource& ResourceHolder<Resource, Identifier>::get(const Identifier& id) {
        return const_cast<Resource&>(static_cast<const ResourceHolder&>(*this).get(id));
    }

    // --------------------------------------------------------------------------------------------
    // contains(...)
    // --------------------------------------------------------------------------------------------

    template <typename Resource, typename Identifier>
    [[nodiscard]] bool
    ResourceHolder<Resource, Identifier>::contains(const Identifier& id) const noexcept {
        return (mResourceMap.find(id) != mResourceMap.end());
    }

    // --------------------------------------------------------------------------------------------
    // unload(...)
    // --------------------------------------------------------------------------------------------

    template <typename Resource, typename Identifier>
    void ResourceHolder<Resource, Identifier>::unload(const Identifier& id) {
        mResourceMap.erase(id);
    }

    // --------------------------------------------------------------------------------------------
    // clear(...)
    // --------------------------------------------------------------------------------------------

    template <typename Resource, typename Identifier>
    void ResourceHolder<Resource, Identifier>::clear() noexcept {
        mResourceMap.clear();
    }

} // namespace core::resources::holders