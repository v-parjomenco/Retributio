// ================================================================================================
// File: game/skyguard/dev/stress_scene.h
// Purpose: DEV/PROFILE-only stress scene helpers for validating ECS + RenderSystem batching.
// Notes:
//  - Compiled only in Debug or Profile builds (guarded at call-site).
//  - Reads environment variables to spawn a large amount of Sprite entities.
//  - Validates texture IDs via ResourceManager to avoid runtime crashes from invalid IDs.
// ================================================================================================
#pragma once

#include "core/resources/ids/resource_ids.h"

namespace core::ecs {
    class World;
}

namespace core::resources {
    class ResourceManager;
}

namespace game::skyguard::dev {

    /**
     * @brief Создает стресс-спрайты, если установлен флаг SKYGUARD_STRESS_SPRITES.
     *
     * Переменные окружения (DEV/PROFILE):
     *  - SKYGUARD_STRESS_SPRITES       : общее количество спрайтов (0 = отключено)
     *  - SKYGUARD_STRESS_Z_LAYERS      : количество слоев zOrder (по умолчанию: 5)
     *  - SKYGUARD_STRESS_TEXTURE_IDS   : разделенные запятыми числовые значения TextureID (рекомендуется)
     *  - SKYGUARD_STRESS_TEXTURE_COUNT : если TEXTURE_IDS отсутствует, пробует последовательные ID, начиная
     *                                   с fallbackTextureId (по умолчанию: 1). Максимальные усилия.
     *
     * Генератор намеренно использует «грязный» порядок создания, чтобы подчеркнуть сортировку + поведение кэша.
     */
    void trySpawnStressSpritesFromEnv(core::ecs::World& world,
                                     core::resources::ResourceManager& resources,
                                     core::resources::ids::TextureID fallbackTextureId);

} // namespace game::skyguard::dev