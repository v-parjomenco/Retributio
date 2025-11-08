#pragma once

#include <string>

namespace core::resources {

    // Базовый интерфейс для всех ресурсов.
    // Позволяет ResourceHolder загружать разные типы ресурсов через единый метод loadFromFile
    class IResource {
    public:
        virtual ~IResource() = default;

        // Унифицированный метод загрузки из файла
        virtual bool loadFromFile(const std::string& filename) = 0;
    };

} // namespace core::resources