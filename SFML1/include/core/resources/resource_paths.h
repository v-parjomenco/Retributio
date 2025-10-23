#pragma once

#include <string>
#include <unordered_map>
#include "core/resources/resourceIDs.h"
#include "core/json_validator.h"

namespace core::resources {

    class ResourcePaths {
    public:
        static void loadFromJSON(const std::string& filename);

        static const std::string& get(TextureID id);
        static const std::string& get(FontID id);
        static const std::string& get(SoundID id);

    private:

        static std::unordered_map<TextureID, std::string> mTextures;
        static std::unordered_map<FontID, std::string> mFonts;
        static std::unordered_map<SoundID, std::string> mSounds;

        // Валидация структуры JSON
        static void validateJSON(const nlohmann::json& data);
    };

} // namespace core::resources