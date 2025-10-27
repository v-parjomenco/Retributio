#pragma once

#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include "core/i_resource.h"

// Универсальный хранитель ресурсов.
// Хранит ресурсы как std::unique_ptr<Resource> в unordered_map<Identifier, unique_ptr<Resource>>.
// Позволяет вызывать openFromFile / другие методы с произвольными параметрами.

template <typename Resource, typename Identifier>
class ResourceHolder {
public:
    ResourceHolder() = default;
    ~ResourceHolder() = default;

    // Загружаем ресурс из файла, с произвольными дополнительными параметрами,
    // которые могут форвардиться (передаваться дальше в том же виде, в котором получены)
    // в Resource::openFromFile(filename, args...).
    template <typename... Args> // вариативный шаблон typename... (число и тип аргументов заранее неизвестны)
    void load(Identifier id, const std::string& filename, Args&&... args);

    // Вставить уже созданный ресурс (unique_ptr) — позволяет избежать повторного чтения с диска.
    void insert(Identifier id, std::unique_ptr<Resource> resource);

    // Получить ресурс
    Resource& get(const Identifier& id);
    const Resource& get(const Identifier& id) const;

    // Проверить наличие ресурса
    bool contains(const Identifier& id) const noexcept;

    // Удалить конкретный ресурс
    void unload(const Identifier& id);

    // Очистить все ресурсы
    void clear() noexcept;

private:
    std::unordered_map<Identifier, std::unique_ptr<Resource>> mResourceMap;
};

// Включаем реализацию шаблонов
#include "resource_holder.inl"