#pragma once

#include <string>
#include <unordered_map>

#include "core/resources/ids/resourceIDs.h"
#include "core/utils/json/json_validator.h"

namespace core::resources::paths {

    class ResourcePaths {
      public:
        static void loadFromJSON(const std::string& filename);

        static const std::string& get(ids::TextureID id);
        static const std::string& get(ids::FontID id);
        static const std::string& get(ids::SoundID id);

      private:
        static std::unordered_map<ids::TextureID, std::string> mTextures;
        static std::unordered_map<ids::FontID, std::string> mFonts;
        static std::unordered_map<ids::SoundID, std::string> mSounds;

        // Валидация структуры JSON
        static void validateJSON(const nlohmann::json& data);
    };

} // namespace core::resources::paths