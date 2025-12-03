# Resource Layer Overview

This document describes the **resource layer** of the engine: how textures, fonts and sounds
are defined, loaded, cached and accessed in a consistent, data-driven way.

The pipeline is designed to scale from small games (SkyGuard) up to large 4X projects
with many assets and possible streaming.

---

## 1. High-Level Pipeline

```text
          Compile-time IDs                         Runtime data
    (TextureID / FontID / SoundID)              (resources.json)
                 │                                      │
                 └───────────────┬──────────────────────┘
                                 ▼
                    ResourcePaths + *ResourceConfig
                    (ID → { path, flags… })
                                 │
                                 ▼
                         ResourceManager
         (public API: getTexture / getFont / getSound / preload / unload)
                                 │
                                 ▼
        ResourceHolder<Resource, Identifier> (per resource kind)
                   (in-memory cache of loaded resources)
                                 │
                                 ▼
      TextureResource / FontResource / SoundBufferResource
                (thin wrappers around SFML: sf::Texture, sf::Font,
                 sf::SoundBuffer, etc.)
				 
				 Key ideas:

- Enum IDs are the canonical way to address engine resources.
- resources.json describes where the data lives and low-level flags.
- ResourcePaths converts enum IDs into *ResourceConfig.
- ResourceManager is the façade used by gameplay / ECS / UI.
- ResourceHolder is the internal cache keyed by IDs or strings.
- SFML types live at the very bottom; higher-level systems never talk to them directly.


## 2. Core Concepts

2.1. Resource identifiers (IDs)

Header: include/core/resources/ids/resource_ids.h
Helpers: include/core/resources/ids/resource_id_utils.h + src/core/resources/ids/resource_ids.cpp

We currently use three enum classes:

enum class TextureID : std::uint16_t {
    Unknown = 0,
    Player,
    // ...
};

enum class FontID   : std::uint16_t { Unknown = 0, Default };
enum class SoundID  : std::uint16_t { Unknown = 0 };

Each enum has:

- a toString(...) function for logging and error messages;
- a std::hash<> specialization to be used as keys in std::unordered_map;
- string→enum helpers (textureFromString, fontFromString, soundFromString) used when parsing JSON.

These IDs describe the resource set of the current build (e.g. SkyGuard).
For multi-game setups they can be split per game while keeping the same pattern.

2.2. Resource configs (*ResourceConfig)

Headers:

- core/resources/config/texture_resource_config.h
- core/resources/config/font_resource_config.h
- core/resources/config/sound_resource_config.h

Each config is a plain data struct describing how to load/configure the low-level resource, not how it is used in the game:

struct TextureResourceConfig {
    std::string path;
    bool smooth         = true;
    bool repeated       = false;
    bool generateMipmap = false;
};

struct FontResourceConfig {
    std::string path;
    // Future low-level font flags may appear here.
};

struct SoundResourceConfig {
    std::string path;
    // Future streaming / compression / preload flags may appear here.
};

Important separation:

- *ResourceConfig — low-level GPU / SoundBuffer configuration (path, flags).
- Higher-level things like character size, color, UI layout, sound volume, looping belong to
other models (e.g. text blueprints, audio systems), not here.

2.3. JSON registry (resources.json) and ResourcePaths

Header: include/core/resources/paths/resource_paths.h
Implementation: src/core/resources/paths/resource_paths.cpp

ResourcePaths is a centralized read-only registry:

- loaded once from resources.json at startup;
- maps enum IDs to TextureResourceConfig, FontResourceConfig, SoundResourceConfig;
- exposes simple static accessors:

// Per-ID access (throws std::runtime_error if ID is not found)
const TextureConfig& getTextureConfig(ids::TextureID id);
const FontConfig&    getFontConfig   (ids::FontID id);
const SoundConfig&   getSoundConfig  (ids::SoundID id);

// Presence checks (no exceptions)
bool contains(ids::TextureID id) noexcept;
bool contains(ids::FontID id) noexcept;
bool contains(ids::SoundID id) noexcept;

// Bulk access (for preload, tools)
const std::unordered_map<ids::TextureID, TextureConfig>& getAllTextureConfigs() noexcept;
const std::unordered_map<ids::FontID,    FontConfig>&    getAllFontConfigs()    noexcept;
const std::unordered_map<ids::SoundID,   SoundConfig>&   getAllSoundConfigs()   noexcept;

2.3.1. JSON layout

Canonical JSON structure:

{
  "textures": {
    "Player": {
      "path": "assets/game/skyguard/images/0000su-57.png",
      "smooth": true,
      "repeated": false,
      "mipmap": false
    }
  },
  "fonts": {
    "Default": "assets/core/fonts/Wolgadeutsche.otf"
  },
  "sounds": {
    // "Click": "assets/core/sounds/ui_click.wav"
  }
}

- textures — object; each key is a string ID (e.g. "Player"), value is an object → TextureResourceConfig.
- fonts — object; each key is a string ID, value is a string path → FontResourceConfig.
- sounds — optional object; layout analogous to fonts.

String keys are parsed via textureFromString(...), fontFromString(...), soundFromString(...)
to map them to the corresponding enum IDs.

2.3.2. Validation and error handling

ResourcePaths::loadFromJSON(...):

- Reads file via FileLoader::loadTextFile.
- Parses JSON with nlohmann::json.
- Validates overall structure using JsonValidator.
- On critical errors (missing file, invalid JSON, wrong shape):
	- uses message::showError(...);
	- terminates the process via std::exit(EXIT_FAILURE).

At this level the game cannot run without a valid resources registry, so failing fast is correct.


## 3. ResourceHolder: in-memory cache

Header: include/core/resources/holders/resource_holder.h
Implementation: include/core/resources/holders/resource_holder.inl

Generic template:

template <typename Resource, typename Identifier>
class ResourceHolder {
    // std::unordered_map<Identifier, std::unique_ptr<Resource>> mResourceMap;
};

Main responsibilities:

- Store resources as std::unique_ptr<Resource> keyed by Identifier (enum or std::string).
- Provide a simple API:

template <typename... Args>
bool load(const Identifier& id, const std::string& filename, Args&&... args);
// Throws std::runtime_error on I/O / parse errors.

void insert(const Identifier& id, std::unique_ptr<Resource> resource);
// Takes ownership of an already-loaded resource.

Resource&       get(const Identifier& id);
const Resource& get(const Identifier& id) const;
// Throws std::runtime_error if not found.

bool contains(const Identifier& id) const noexcept;
void unload(const Identifier& id);
void clear() noexcept;
std::size_t size() const noexcept;

Error policy:

- load(...) and get(...) throw std::runtime_error on problems.
- Fallbacks / user-visible recovery are handled one level above (in ResourceManager).


## 4. ResourceManager: public API

Header: include/core/resources/resource_manager.h
Implementation: src/core/resources/resource_manager.cpp

ResourceManager is the high-level façade used by gameplay, ECS systems, UI, etc.

Internally it has six holders:

ResourceHolder<TextureResource, TextureID>     mTextures;
ResourceHolder<FontResource,    FontID>        mFonts;
ResourceHolder<SoundBufferResource, SoundID>   mSounds;

ResourceHolder<TextureResource, std::string>   mDynamicTextures;
ResourceHolder<FontResource,    std::string>   mDynamicFonts;
ResourceHolder<SoundBufferResource, std::string> mDynamicSounds;

Plus fallback IDs (for “purple square”, default font, etc.):

bool         mHasMissingTextureFallback = false;
TextureID    mMissingTextureID{};

bool         mHasMissingFontFallback = false;
FontID       mMissingFontID{};

bool         mHasMissingSoundFallback = false;
SoundID      mMissingSoundID{};

4.1. Enum-based access (canonical path)
const types::TextureResource& getTexture(TextureID id);
const types::FontResource&    getFont   (FontID id);
const types::SoundBufferResource& getSound(SoundID id);

Flow for textures (fonts/sounds analogous):

Query ResourcePaths::getTextureConfig(id) → TextureResourceConfig:

- path
- smooth
- repeated
- generateMipmap

Use helper ensureTextureLoadedWithConfig(...):

- if resource is not in the holder:
- call holder.load(id, config.path);

apply flags: setSmooth(config.smooth), setRepeated(config.repeated),
generateMipmap() if requested (with debug logging on failure).

Return a const TextureResource&.

Error behavior:

- If getTextureConfig(id) or the load fails:
	- if a fallback texture is configured and id != mMissingTextureID:
		- log a debug message with details and return the fallback ID via getTexture(mMissingTextureID);
	- otherwise, rethrow the original exception to the caller.

This is the main path used by the engine and gameplay code.

4.2. Dynamic string IDs
const types::TextureResource&       getTexture(const std::string& id);
const types::FontResource&          getFont(const std::string& id);
const types::SoundBufferResource&   getSound(const std::string& id);

Semantics:

- Intended for tools, modding, prototypes, tests.
- The string is treated as a logical ID, but the current implementation maps it 1:1 to a file path.

Current implementation:

- Builds a temporary *ResourceConfig:

	TextureResourceConfig cfg{};
	cfg.path          = id;
	cfg.smooth        = true;
	cfg.repeated      = false;
	cfg.generateMipmap = false;

	( analogous configs for fonts / sounds with just path )

- Calls ensure*LoadedWithConfig(mDynamic*, id, cfg).

Error / fallback policy:

- Same pattern as enum methods:

	- try to load;
    - on failure, if a fallback (texture/font/sound) is configured → log and return fallback;
	- if not, rethrow.

Conceptually:

- Enum + resources.json is the canonical way for engine/game code.
- Dynamic string IDs are a consciously more “loose” path for tools and modding,
with hard-coded default flags for now.
- In the future, a dedicated registry (dynamic_resources.json) can be introduced to
describe these dynamic resources with full configs.

4.3. Escape hatch: getTextureByPath
const types::TextureResource& getTextureByPath(const std::string& path);

Semantics:

- Direct access by physical filesystem path, bypassing any ID schemes.
- Key in the cache is the exact path string.
- Used for low-level testing, debugging and ad-hoc utilities.

Current implementation:

- Builds a TextureResourceConfig:
	TextureResourceConfig cfg{};
	cfg.path          = path;
	cfg.smooth        = true;
	cfg.repeated      = false;
	cfg.generateMipmap = false;

- Calls the same helper ensureTextureLoadedWithConfig(mDynamicTextures, path, cfg).

- Error policy is identical:
 - if fallback texture is configured → log and return fallback;
 - otherwise, rethrow.

4.4. Fallback resources
void setMissingTextureFallback(TextureID id);
void setMissingFontFallback   (FontID id);
void setMissingSoundFallback  (SoundID id);

Responsibilities:

- Store which ID should be used as the fallback.
- Immediately warm it up:

void ResourceManager::setMissingTextureFallback(TextureID id) {
    mMissingTextureID = id;
    mHasMissingTextureFallback = true;
    (void)getTexture(id); // will throw early if fallback is misconfigured
}

If a later load fails, the corresponding getTexture/getFont/getSound method will:

- log a detailed debug message including:
	- failing ID,
	- fallback ID,
	- exception message;
- attempt to return the fallback resource.

If the fallback itself is broken, the exception will propagate.

4.5. Preload, unload and metrics

Preload:

void preloadTexture(TextureID id);
void preloadFont   (FontID id);
void preloadSound  (SoundID id);

void preloadAllTextures();
void preloadAllFonts();
void preloadAllSounds();

- Per-ID methods simply call get* and discard the result.
- preloadAll* iterate over ResourcePaths::getAll*Configs() and call get* for each ID.
- Typical usage:
	- loading screens,
	- preparing a heavy scene,
	- profiling asset memory usage.

Unload and clear:

void unloadTexture(TextureID id) noexcept;
void unloadTexture(const std::string& id) noexcept;
// Same for Font and Sound.

void clearAll() noexcept;

- unload* removes specific entries from the holders (careful with dangling references).
- clearAll() wipes all caches; fallback IDs remain but their resources will be reloaded lazily.

Metrics:

struct ResourceMetrics {
    std::size_t textureCount        = 0;
    std::size_t dynamicTextureCount = 0;
    std::size_t fontCount           = 0;
    std::size_t dynamicFontCount    = 0;
    std::size_t soundCount          = 0;
    std::size_t dynamicSoundCount   = 0;
};

ResourceMetrics getMetrics() const noexcept;

- Simple snapshot of how many resources are currently cached in each holder.
- Useful for debug overlays, profiling tools and unit tests.


## 5. Error-handling strategy by layer

FileLoader / JSON utils / ResourcePaths:
- On critical configuration errors at startup (e.g. missing / broken resources.json):
	- log via message::showError(...);
	- terminate the process via std::exit(EXIT_FAILURE).
ResourceHolder:
- Throws std::runtime_error with detailed messages when:
	- file loading fails in load(...);
	- resource is missing in get(...).
ResourceManager:
- Wraps ResourceHolder calls in try/catch.
- On failure:
	- if a fallback is configured → log + return fallback;
	- else rethrow.
Gameplay / ECS systems / Game class:
- Consumer code (e.g. Game::initWorld) either:
	- lets exceptions bubble up to a top-level try/catch in main() or Game::run(),
	- or catches them and shows user-facing error messages.


## 6. Usage examples & patterns

6.1. Typical engine usage (enum IDs)
// Game / ECS system
const sf::Texture& playerTexture =
    resourceManager.getTexture(core::resources::ids::TextureID::Player).get();

// Debug overlay
const sf::Font& debugFont =
    resourceManager.getFont(core::resources::ids::FontID::Default).get();


Everything goes through:

EnumID → ResourcePaths → *ResourceConfig → ResourceManager → ResourceHolder → SFML

6.2. Dynamic / tools / modding usage
// Tool or editor prototype
const sf::Texture& tempTexture =
    resourceManager.getTexture("C:/tmp/experimental_sprite.png").get();

// Hard-coded experimental font for a debug tool
const sf::Font& tempFont =
    resourceManager.getFont("assets/dev/fonts/DebugMono.ttf").get();

Here:

- String is used as a flexible ID.
- Low-level flags for textures are currently fixed (smooth = true; repeated = false; mipmap = false),
which is acceptable for tools and prototypes.
- For any asset that becomes part of the final build, it’s recommended to:
	- add a proper TextureID/FontID/SoundID,
	- describe it in resources.json,
	- switch to the enum-based path.

6.3. Escape hatch using paths directly
// One-off debug helper
const sf::Texture& tex =
    resourceManager.getTextureByPath("assets/debug/test_pattern.png").get();

Use this when:

- you explicitly want to bypass all registries,
- you are in a debug / experimental context.


## 7. Future extensions for 4X-scale projects

The current design is intentionally minimal, but extendable. Natural evolution paths:

- Separate registry for dynamic resources
	- Introduce dynamic_resources.json:
		- map string IDs to *ResourceConfig with full control over flags;
		- keep the same helper functions (ensure*LoadedWithConfig).
- Streaming / hot-reload layer
	- Add a streaming manager on top of ResourceManager, responsible for:
		- unloading rarely used assets by region / chunk;
		- background loading;
		- reacting to editor / file-watcher signals for hot-reload.
- Memory budgets and advanced metrics
	- Add tracking of approximate memory usage per resource type.
	- Integrate with debug overlays and profiling tools.
- Integration with higher-level blueprints
	- Keep the clean separation:
		- *ResourceConfig — low-level resource.
		- PlayerBlueprint, TerrainTileBlueprint, UITextBlueprint — high-level usage.
	- Larger games can build complex blueprints on top of a stable and simple resource layer.

The goal is to keep the core resource pipeline small and robust, so that all future 4X-specific
features can be layered on top without having to rewrite the fundamentals.