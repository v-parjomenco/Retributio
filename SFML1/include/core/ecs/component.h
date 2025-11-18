// ================================================================================================
// File: core/ecs/component.h
// Purpose: Base tag/interface for ECS components
// Used by: ComponentStorage<T>, World
// Related headers: component_storage.h, world.h
// ================================================================================================
#pragma once

namespace core::ecs {

    // COMMENT: Keep as a tag/base for future common behaviors if needed.
    // Minimal surface area helps performance and compile times.

    // Базовый класс-компонент.
    // Он пока пустой — этого достаточно, чтобы в проекте был "единый вход" для компонентов.
    // Здесь ничего добавлять не нужно, пока не появится необходимость в общих интерфейсах компонентов.
    struct Component {
        virtual ~Component() = default;
    };

} // namespace core::ecs