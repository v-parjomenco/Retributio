#pragma once

// Загружаем ресурс из файла, с произвольными дополнительными параметрами,
// которые могут форвардиться (передаваться дальше в том же виде, в котором получены)
// в Resource::loadFromFile(filename, args...).
template <typename Resource, typename Identifier>
template <typename... Args> // вариативный шаблон typename... (число и тип аргументов заранее неизвестны)
void ResourceHolder<Resource, Identifier>::load(Identifier id, const std::string& filename, Args&&... args) {

    // Проверка на дубликаты ID: если уже есть ресурс с таким ID — не загружаем снова.
    if (mResourceMap.find(id) != mResourceMap.end()) {
        return; // если поиск по id нашёл дубликат до конца контейнера mResourceMap, выходим, код ниже не выполняется
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
        throw std::runtime_error("[ResourceHolder::load] Не удалось загрузить файл: " + filename);
    }

    // Вставляем загруженный ресурс в карту.
    mResourceMap.emplace(id, std::move(resource)); // std::move(resource) передаёт владение ресурсом в контейнер mResouceMap.
}


// Получить ресурс (неконстантная версия)
// Возвращает ссылку на сам объект (не указатель).
template <typename Resource, typename Identifier>
Resource& ResourceHolder<Resource, Identifier>::get(const Identifier& id) {
    auto found = mResourceMap.find(id);
    assert(found != mResourceMap.end() && "[ResourceHolder::get]\nРесурс не найден");
    return *found->second;
}

// Константная версия метода get
template <typename Resource, typename Identifier>
const Resource& ResourceHolder<Resource, Identifier>::get(const Identifier& id) const {
    auto found = mResourceMap.find(id);
    assert(found != mResourceMap.end() && "[ResourceHolder::get]\nРесурс не найден");
    return *found->second;
}

// Проверить наличие ресурса по ID.
template <typename Resource, typename Identifier>
bool ResourceHolder<Resource, Identifier>::contains(const Identifier& id) const noexcept { // noexcept явно показывает, что метод не кидает исключений, и только делает поиск
    return mResourceMap.find(id) != mResourceMap.end();
}

// Метод для удаления ресурса
template <typename Resource, typename Identifier>
void ResourceHolder<Resource, Identifier>::unload(const Identifier& id) {
    if (!contains(id)) return; // проверяем перед удалением
    mResourceMap.erase(id);
}

// Метод для полной очистки ресурсов mResourceMap
template <typename Resource, typename Identifier>
void ResourceHolder<Resource, Identifier>::clear() noexcept {
    mResourceMap.clear();
}