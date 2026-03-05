# Resource Registry v1 — Final Specification

**Version:** 1.0.1  
**Status:** FROZEN (contract for implementation)  
**Date:** 2025-01-16

---

## Table of Contents

1. [Overview](#1-overview)
2. [Changes to Requirements Guide](#2-changes-to-requirements-guide)
3. [Architectural Decisions](#3-architectural-decisions)
4. [Data Structures](#4-data-structures)
5. [File Format](#5-file-format)
6. [API Contracts](#6-api-contracts)
7. [Error Handling](#7-error-handling)
8. [Save Format](#8-save-format)
9. [Hot Reload](#9-hot-reload)
10. [File Changes](#10-file-changes)
11. [PR Plan](#11-pr-plan)
12. [Validation Checklist](#12-validation-checklist)
13. [Appendix A: Code Template Policies](#appendix-a-code-template-policies)
14. [Appendix B: Migration Examples](#appendix-b-migration-examples)
15. [Appendix C: Timeline Summary](#appendix-c-timeline-summary)

---

## 1. Overview

### 1.1 Problem Statement

Current enum-based resource IDs (`TextureID`/`FontID`/`SoundID`) do not scale for:
- 100k+ unique resources
- User mods (mods change data only, not code)
- Deterministic saves/replays across versions

### 1.2 Solution

Replace compile-time enum IDs with runtime registry:
- **Canonical Key String** → human-readable, mod-friendly
- **StableKey64** → persistence, cross-version stability
- **RuntimeKey32** → zero-overhead hot path access

### 1.3 Design Principles

| Principle | Implementation |
|-----------|----------------|
| Zero-overhead hot path | RuntimeKey32 = array index, no strings/maps |
| Validate-on-write | All validation at load time |
| Trust-on-read | No validation in runtime access |
| Determinism | Same inputs → same indices |
| Game-agnostic core | No game-specific code in `core/resources` |

---

## 2. Changes to Requirements Guide

The following sections of `retributio_atrapacielos_Requirements_Guide.pdf` must be updated:

### 2.1 Section 6.1 — Resource Layer Rules

**Old:**
> Enum IDs (TextureID/FontID/SoundID) are canonical for referencing resources.

**New:**
> Canonical Key Strings (namespaced) are canonical for referencing resources.
> RuntimeKey32 (index-based) is used in hot paths.
> StableKey64 (xxHash3_64) is used for persistence.

### 2.2 Section 6.4 — Deterministic Resource Loading

**Old:**
> Resource loading must be deterministic (sorted by enum ID).

**New:**
> Resource loading must be deterministic (sorted by StableKey64).

### 2.3 Section 14.2 — Enum ID Stability

**Old:**
> Enum IDs are never removed or reordered for mod compatibility.

**New:**
> Canonical Key Strings are never removed or renamed for mod compatibility.
> StableKey64 provides cross-version stability.

### 2.4 Appendix A.1 — Deterministic Resource Preloading

**Old:**
> Resources are loaded in sorted order by enum ID.

**New:**
> Resources are loaded in sorted order by StableKey64.

### 2.5 Section 5.7 — Sprite / Rendering Design

**Old:**
> SpriteComponent is data-only: TextureID, sf::IntRect textureRect...

**New:**
> SpriteComponent is data-only: TextureKey (RuntimeKey32), sf::IntRect textureRect...

---

## 3. Architectural Decisions

### 3.1 Three Resource Identities (frozen)

| Identity | Type | Location | Purpose |
|----------|------|----------|---------|
| **RuntimeKey32** | `uint32_t` (index + generation) | ECS components, RenderSystem, hot loops | Zero-overhead access |
| **StableKey64** | `uint64_t` (xxHash3_64) | Saves, replays, mod manifests | Cross-version stability |
| **DebugName** | `std::string_view` | Logs, debug overlay, errors | Human-readable diagnostics |

**Critical Rule:** In hot path (RenderSystem, simulation) — only RuntimeKey32. No strings, hashes, or map lookups.

### 3.2 RuntimeKey32 Encoding (v1.0.1 — corrected invalid sentinel)

The previous version used `InvalidRaw = 0xFFFFFFFF` which implicitly reserved one valid-looking combination. To remove ambiguity completely, we use **zero sentinel with biased index**.

**RuntimeKey32 Contract v1.0.1:**

| Property | Value |
|----------|-------|
| Invalid sentinel | `raw == 0` (only invalid value) |
| Index storage | bits 0..23 store `(index + 1)` |
| Valid index range | 0 .. 16,777,214 (0x00FFFFFE) |
| Generation storage | bits 24..31 (0..255) |
| Bit manipulation | mask/shift only, NO C++ bitfields |

**Encoding:**
```cpp
// To create key with index i and generation g:
raw = ((g << 24) | ((i + 1) & 0x00FFFFFF))

// To extract:
index = (raw & 0x00FFFFFF) - 1
generation = raw >> 24

// Invalid check:
isInvalid = (raw == 0)
```

**Generation in v1:**
- Stored always (no separate allocation)
- NOT checked in Release builds
- NOT incremented on reload (would invalidate ECS keys)
- Reserved for future hot-reload invalidation detection

### 3.3 StableKey64 — Hash Algorithm

| Property | Value |
|----------|-------|
| **Algorithm** | xxHash3_64 |
| **Implementation** | Single-header `xxhash.h` in `third_party/` |
| **Version** | Pinned (do not update without reason) |
| **Seed** | Fixed constant `0` (never change) |
| **Collision Policy** | Detect on load → PANIC |

**Why xxHash3_64:**
- Excellent distribution (fewer collisions at 100k+ keys)
- Fast on modern CPUs (SIMD)
- Industry-standard, battle-tested
- Stable contract for years

**Note:** Hash speed is secondary — hashing is load-time only, not hot path.

### 3.4 StringPool — Chunked Arena (paged monotonic)

**Decision:** Chunked Pool (paged arena)

**Why Chunked Pool:**
- Single-pass loading
- Unknown total size with mods/DLC
- Stable `string_view` lifetimes (never invalidated)
- Append-only for hot-reload support

```cpp
class StringPool {
    static constexpr size_t DefaultChunkSize = 4 * 1024 * 1024; // 4MB
    
    std::vector<std::unique_ptr<char[]>> mChunks;
    char* mCurrentPtr = nullptr;
    size_t mRemaining = 0;
    
    // Load-time only deduplication, freed after finalize
    std::unordered_set<std::string_view> mLookup;
    
public:
    [[nodiscard]] std::string_view intern(std::string_view sv);
    void clearLookup(); // Call after finalize to free memory
};
```

**Implementation Rules:**
- Chunk capacity = `max(DefaultChunkSize, neededBytes)`
- `mRemaining` tracks actual chunk capacity
- Strings stored WITH null-terminator (`size + 1`) for C interop
- Returned `string_view` excludes null-terminator
- `clearLookup()` called after finalize to free dedup memory

**Two-pass Reserve:** Used only in compiled registry build tool (all data known upfront).

### 3.5 Override Policy — Hybrid Layers (frozen)

**Layer Priorities:**

| Layer | Priority | Description |
|-------|----------|-------------|
| Core | 0 | Base game |
| DLC | 1000 | Official DLC |
| Patches | 2000 | Community patches |
| Mods | 3000 | User mods |

**Within Layer:** Last Wins (by explicit `loadOrder`)

**Determinism for Saves:**
```json
{
  "registry_version": 1,
  "mod_list": ["core", "dlc_space", "mod_ships", "mod_ui"],
  "mod_list_hash": "0x1A2B3C4D..."
}
```

**On Save Load:**
1. Compare `mod_list` with current
2. If mismatch: warn OR rebuild registry in save's order (for replays)

### 3.6 Canonical Key String Format (frozen)

**Format:** `namespace.category.name`

**Validation Rules:**
- Separator: `.` (dot)
- Minimum segments: 3 (namespace.category.name)
- Each segment: `^[a-z][a-z0-9_-]*$`
- Full string: only `[a-z0-9._-]`, lowercase only

**Regex:**
```
^[a-z][a-z0-9_-]*(\.[a-z][a-z0-9_-]*){2,}$
```

**Valid Examples:**
```
core.texture.missing
core.font.default
atrapacielos.sprite.player
atrapacielos.background.desert
mod.johndoe.elves.texture.archer
mod.jane-doe.orcs.sound.battle_cry
```

**Invalid Examples:**
```
Core.Texture.Missing     // uppercase
core.texture             // only 2 segments
core..texture.foo        // empty segment
core.texture.foo bar     // space
core.texture.FOO         // uppercase
```

### 3.7 Registry Ownership

**Decision:** Instance-owned (Game/Composition Root owns ResourceRegistry)

**NOT Singleton.**

**Reasons:**
- Tests create own registry without global state
- Multi-context (editor + game) without changes
- RuntimeKey remains 4 bytes (no registry ID needed)
- Aligns with "no globals" in Requirements Guide

### 3.8 Deterministic Registry Build (v1.0.1 — corrected duplicate semantics)

**Definitions:**

| Scenario | Action |
|----------|--------|
| Duplicate canonical key **within single source file** | PANIC (authoring error) |
| Duplicate canonical key **across sources** | Resolved by override policy (not an error) |

**Algorithm:**

1. **Load** all sources into definitions map, tracking for each:
   - `layerPriority`
   - `loadOrder`
   - `sourceName`

2. **Merge** into winning set using override comparator:
   - Higher `layerPriority` wins
   - If equal, higher `loadOrder` wins
   - If still tied: PANIC (force explicit order)

3. **Compute** StableKey64 for each winning canonical key

4. **Validate:**
   - Canonical key format valid
   - No StableKey64 collisions (different keys → same hash = PANIC)

5. **Sort** winning set by StableKey64 (total order)

6. **Assign** RuntimeKey.index = position in sorted array (0..N-1)

7. **Build** typed vectors (textures, fonts, sounds)

8. **Build** name-index arrays for debug lookup

9. **Resolve** fallback keys (`core.texture.missing`, `core.font.default`)
   - If missing → PANIC (Type A)

**Guarantee:** Same inputs + same mod order = same runtime indices.

---

## 4. Data Structures

### 4.1 Resource Entry

```cpp
template <typename Key, typename Config>
struct ResourceEntry {
    Key key;                    // RuntimeKey32
    uint64_t stableKey;         // StableKey64 (xxHash3_64)
    std::string_view name;      // interned "atrapacielos.sprite.player"
    std::string_view path;      // interned "assets/..."
    Config config;              // Type-specific config
};

using TextureEntry = ResourceEntry<TextureKey, TextureResourceConfig>;
using FontEntry = ResourceEntry<FontKey, FontResourceConfig>;
using SoundEntry = ResourceEntry<SoundKey, SoundResourceConfig>;
```

### 4.2 Resource Source

```cpp
struct ResourceSource {
    std::string path;           // Path to JSON file
    int layerPriority;          // 0=Core, 1000=DLC, etc.
    int loadOrder;              // Order within layer
    std::string sourceName;     // "core", "dlc_space", "mod_ships"
};
```

### 4.3 Name Index (for debug lookup)

```cpp
struct NameIndex {
    std::string_view name;      // interned canonical key
    uint32_t index;             // index into entries vector
};

// Sorted by name for binary search
std::vector<NameIndex> mTextureNameIndex;
std::vector<NameIndex> mFontNameIndex;
std::vector<NameIndex> mSoundNameIndex;
```

### 4.4 ResourceRegistry (v1.0.1 — corrected API)

```cpp
class ResourceRegistry {
public:
    // === Load-time API ===
    // finalize() is internal to loadFromSources, not public
    void loadFromSources(std::span<const ResourceSource> sources);
    
    // === Runtime API (O(1)) ===
    [[nodiscard]] const TextureEntry& getTexture(TextureKey key) const noexcept;
    [[nodiscard]] const FontEntry& getFont(FontKey key) const noexcept;
    [[nodiscard]] const SoundEntry* tryGetSound(SoundKey key) const noexcept;  // nullable!
    
    // === Debug/Tooling API (O(log N) binary search) ===
    [[nodiscard]] TextureKey findTextureByName(std::string_view name) const;
    [[nodiscard]] TextureKey findTextureByStableKey(uint64_t stableKey) const;
    [[nodiscard]] FontKey findFontByName(std::string_view name) const;
    [[nodiscard]] SoundKey findSoundByName(std::string_view name) const;
    
    // === Fallback keys ===
    [[nodiscard]] TextureKey missingTextureKey() const noexcept;
    [[nodiscard]] FontKey missingFontKey() const noexcept;
    
    // === Metrics ===
    [[nodiscard]] size_t textureCount() const noexcept;
    [[nodiscard]] size_t fontCount() const noexcept;
    [[nodiscard]] size_t soundCount() const noexcept;

private:
    StringPool mStrings;
    
    // Entry vectors sorted by stableKey
    std::vector<TextureEntry> mTextures;
    std::vector<FontEntry> mFonts;
    std::vector<SoundEntry> mSounds;
    
    // Name index arrays sorted by name (for O(log N) name lookup)
    std::vector<NameIndex> mTextureNameIndex;
    std::vector<NameIndex> mFontNameIndex;
    std::vector<NameIndex> mSoundNameIndex;
    
    // Fallback keys (resolved during finalize)
    TextureKey mMissingTexture;
    FontKey mMissingFont;
};
```

**Critical Notes:**
- `tryGetSound()` returns pointer (nullable) for soft-fail
- `finalize()` is internal, not exposed publicly (footgun prevention)
- Name lookup uses dedicated sorted `NameIndex` arrays (entries sorted by stableKey, not name)
- `unordered_map` allowed ONLY during load, then destroyed

### 4.5 ResourceManager (v1.0.1 — corrected sound API)

```cpp
class ResourceManager {
public:
    void initialize(std::span<const ResourceSource> sources);
    
    // === Primary API (RuntimeKey32) ===
    [[nodiscard]] const sf::Texture& getTexture(TextureKey key);   // returns fallback on invalid
    [[nodiscard]] const sf::Font& getFont(FontKey key);            // returns fallback on invalid
    [[nodiscard]] const sf::SoundBuffer* tryGetSound(SoundKey key); // returns nullptr on invalid
    
    // === Preload ===
    void preloadTexture(TextureKey key);
    void preloadAllTextures();
    
    // === Fallback ===
    [[nodiscard]] TextureKey missingTextureKey() const noexcept;
    [[nodiscard]] FontKey missingFontKey() const noexcept;
    
    // === Registry access (for config loaders) ===
    [[nodiscard]] const ResourceRegistry& registry() const noexcept;
    [[nodiscard]] TextureKey findTexture(std::string_view name) const;

private:
    ResourceRegistry mRegistry;
    
    // Vector-based cache (O(1) by index, nullptr = not loaded yet)
    std::vector<std::unique_ptr<TextureResource>> mTextureCache;
    std::vector<std::unique_ptr<FontResource>> mFontCache;
    std::vector<std::unique_ptr<SoundBufferResource>> mSoundCache;
};
```

**Critical:** No `unordered_map` in cache. Index directly into vector.

---

## 5. File Format

### 5.1 resources.json (New Format)

```json
{
  "version": 1,
  "textures": {
    "core.texture.missing": {
      "path": "assets/textures/placeholders/missing_texture.png",
      "smooth": false,
      "repeated": false,
      "mipmap": false
    },
    "atrapacielos.sprite.player": {
      "path": "assets/textures/su57.png",
      "smooth": true,
      "repeated": false,
      "mipmap": false
    },
    "atrapacielos.background.desert": {
      "path": "assets/textures/backgrounds/desert_tile.png",
      "smooth": true,
      "repeated": true,
      "mipmap": false
    }
  },
  "fonts": {
    "core.font.default": {
      "path": "assets/fonts/Wolgadeutsche.otf"
    }
  },
  "sounds": {}
}
```

### 5.2 Migration from Old Format

**Old:**
```json
{
  "textures": {
    "Player": { "path": "...", ... }
  }
}
```

**New:**
```json
{
  "version": 1,
  "textures": {
    "atrapacielos.sprite.player": { "path": "...", ... }
  }
}
```

---

## 6. API Contracts

### 6.1 Load-time vs Runtime

| Operation | Allowed At | Complexity |
|-----------|------------|------------|
| `loadFromSources()` | Load-time only | O(N log N) |
| `getTexture(TextureKey)` | Runtime | O(1) |
| `findTextureByName(string_view)` | Debug/Tooling | O(log N) |

**Note:** `finalize()` is internal to `loadFromSources()`, not a public API.

### 6.2 Hot Path Rules — Per-Frame Texture Cache (v1.0.1 — critical correction)

From Requirements Guide Section 6.3:
> ResourceManager::getTexture() must not be called in a tight loop or on every texture switch.

**WRONG approach (uses map in render loop):**
```cpp
// DO NOT DO THIS
std::unordered_map<TextureKey, const sf::Texture*> frameCache;
```

**CORRECT approach — Epoch Arrays (no maps):**

```cpp
class RenderSystem {
    // Sized to registry.textureCount() at init
    std::vector<const sf::Texture*> mFrameTexturePtr;
    std::vector<uint32_t> mFrameTextureStamp;
    uint32_t mFrameId = 0;
    
    const sf::Texture* getTextureCached(TextureKey key) {
        const uint32_t idx = key.index();
        
        if (mFrameTextureStamp[idx] != mFrameId) {
            // One ResourceManager call per unique texture per frame
            mFrameTexturePtr[idx] = &mResources->getTexture(key);
            mFrameTextureStamp[idx] = mFrameId;
        }
        
        return mFrameTexturePtr[idx];
    }
    
    void beginFrame() {
        ++mFrameId;
        // No clearing needed — stamp comparison handles staleness
    }
};
```

**Benefits:**
- O(1) per sprite (array index + integer compare)
- No map operations in render loop
- One `ResourceManager::getTexture()` call per unique texture per frame

### 6.3 Config Loading Pattern (v1.0.1 — use project JSON accessors)

Config loaders are load-time boundary code. Use existing project JSON validators.

```cpp
// In config loader (load-time, not hot path)
void loadSpriteConfig(const nlohmann::json& json, 
                      const ResourceManager& resources,
                      SpriteProperties& out) {
    // Use project's JSON accessors (parseAndValidateCritical, etc.)
    // Do NOT use json["texture"].get<std::string_view>() directly
    
    std::string textureKeyStr;
    if (!tryParseString(json, "texture", textureKeyStr)) {
        LOG_PANIC(log::cat::Config, "Missing 'texture' in sprite config");
    }
    
    TextureKey key = resources.findTexture(textureKeyStr);
    if (!key.valid()) {
        LOG_PANIC(log::cat::Config, 
            "Unknown texture '{}' in sprite config", textureKeyStr);
    }
    
    out.texture = key;  // Store RuntimeKey32 — runtime trusts it
}
```

---

## 7. Error Handling

### 7.1 Error Policies (from Requirements Guide)

| Resource Type | Policy | On Error |
|---------------|--------|----------|
| Textures | Type A (critical) | PANIC |
| Fonts | Type A (critical) | PANIC |
| Sounds | Type C (soft) | WARN + skip |

### 7.2 Validation Errors (v1.0.1 — corrected duplicate semantics)

| Error | Action |
|-------|--------|
| Invalid canonical key format | PANIC |
| Duplicate canonical key **within single source file** | PANIC |
| Duplicate canonical key **across sources** | Resolved by override (NOT an error) |
| StableKey64 collision (different keys → same hash) | PANIC |
| Missing texture file | PANIC |
| Missing font file | PANIC |
| Missing sound file | WARN + skip |
| Missing `core.texture.missing` during finalize | PANIC |
| Missing `core.font.default` during finalize | PANIC |

### 7.3 Runtime Errors

| Error | Action |
|-------|--------|
| Invalid TextureKey | Return fallback (missing texture) |
| Invalid FontKey | Return fallback (missing font) |
| Invalid SoundKey | `tryGetSound()` returns `nullptr`; caller skips |

---

## 8. Save Format

### 8.1 Design for Scale

For 4X with 2000+ players, millions of entity references:

**Body (millions of references):**
```
LocalId: varint (1-4 bytes, typically 1-2)
```

**Header (once per save):**
```
LocalId → StableKey64 table (dense vector, not map)
StableKey64 → canonical key string (optional, for debug)
```

### 8.2 Implementation

```cpp
// In save header
std::vector<uint64_t> stableKeyByLocalId;  // dense, indexed by LocalId

// At load time (built once, then discarded)
std::unordered_map<uint64_t, RuntimeKey32> runtimeKeyByStableKey;
```

### 8.3 Benefits

- Size: ~2 bytes per reference (vs 8 for StableKey64, vs 30+ for strings)
- Speed: varint decode is fast
- Stability: StableKey64 survives version changes
- Self-contained: save carries its own key snapshot

### 8.4 Load Algorithm

1. Read header: build `LocalId → StableKey64` vector
2. Build `StableKey64 → RuntimeKey32` map from current registry (load-time only)
3. Read body: convert `LocalId → StableKey64 → RuntimeKey32`
4. Discard temporary map

---

## 9. Hot Reload

### 9.1 Supported Modes

| Mode | Description | Index Change | Generation Change |
|------|-------------|--------------|-------------------|
| **Reload in-place** | Replace texture bytes | NO | NO |
| **Append-only** | Add new resource | NO (append) | NO (new = 0) |
| **Full rebuild** | NOT SUPPORTED in v1 | — | — |

### 9.2 Reload In-Place

**Use case:** Artist modifies texture file, wants to see change without restart.

**Behavior:**
- Same canonical key
- Same RuntimeKey32 (index unchanged)
- Same generation (NOT incremented)
- New bytes loaded into existing cache slot

**Why NOT increment generation:**
- All existing `TextureKey` in ECS components would become stale
- Would require invalidation pass through all entities
- Defeats purpose of "quick iteration"

### 9.3 Append-Only Add (Dev Mode)

**Use case:** Add new texture without restart.

**Behavior:**
- New canonical key
- New RuntimeKey32.index = N (appended)
- Generation = 0
- Existing indices unchanged

### 9.4 Generation Checks

| Property | Value |
|----------|-------|
| Storage | Always (in RuntimeKey32.raw) |
| Checked | Only in dev/hot-reload builds |
| Location | Cache boundary, NOT per-sprite |
| Release | Zero overhead (checks compiled out) |

---

## 10. File Changes

### 10.1 New Files

```
engine/include/core/resources/
├── keys/
│   ├── resource_key.h          // ResourceKey<Tag> template
│   └── stable_key.h            // StableKey64 + xxHash3 wrapper
├── registry/
│   ├── string_pool.h
│   ├── string_pool.cpp
│   ├── resource_entry.h
│   ├── resource_registry.h
│   └── resource_registry.cpp

third_party/
└── xxhash/
    └── xxhash.h                // Pinned version
```

### 10.2 Modified Files

```
engine/include/core/resources/
├── resource_manager.h          // RuntimeKey32 API
├── resource_manager.cpp
└── holders/resource_holder.h   // Vector-based cache

engine/include/core/ecs/
├── components/sprite_component.h    // TextureKey
└── systems/
    ├── render_system.h              // TextureKey + epoch arrays
    └── render_system.cpp

engine/include/core/config/
└── properties/sprite_properties.h   // TextureKey

games/atrapacielos/include/
├── config/loader/config_loader.cpp  // string → TextureKey
├── game.cpp                         // Registry init
├── presentation/
│   ├── background_renderer.h
│   └── background_renderer.cpp
└── dev/
    ├── stress_scene.h               // Remove dense enum hack
    └── stress_scene.cpp

games/atrapacielos/assets/config/
└── resources.json                   // New namespaced format

docs/
└── retributio_atrapacielos_Requirements_Guide.pdf  // Updated sections
```

### 10.3 Deleted Files and Folders

```
engine/include/core/resources/ids/resource_ids.h
engine/include/core/resources/ids/resource_id_utils.h
engine/include/core/resources/paths/resource_paths.h
engine/include/core/resources/paths/resource_paths.cpp
engine/include/core/resources/ids/        // legacy enum IDs + helpers
engine/include/core/resources/paths/      // legacy ResourcePaths registry
```

---

## 11. PR Plan

### PR0 — Spec Freeze + Documentation

**Goal:** Lock contracts, update Requirements Guide.

**Changes:**
- Update `retributio_atrapacielos_Requirements_Guide.pdf`:
  - Section 6.1, 6.4, 14.2, A.1
  - Section 5.7 (SpriteComponent)
- Create `docs/resource_registry_spec.md` (this document)

**Acceptance Criteria:**
- [ ] Documentation does not contradict new architecture
- [ ] All "enum canonical" references removed

**Time:** 0.5 day

---

### PR1 — Keys + xxHash + StringPool

**Goal:** Core infrastructure without changing existing code.

**New Files:**
```
engine/include/core/resources/keys/resource_key.h
engine/include/core/resources/keys/stable_key.h
engine/include/core/resources/registry/string_pool.h
engine/include/core/resources/registry/string_pool.cpp
third_party/xxhash/xxhash.h
```

**Unit Tests:**
- [ ] ResourceKey: `raw == 0` is invalid
- [ ] ResourceKey: `index()` returns `(raw & mask) - 1`
- [ ] ResourceKey: `generation()` returns `raw >> 24`
- [ ] ResourceKey: comparison operators work correctly
- [ ] StableKey64: hash consistency (same input = same hash)
- [ ] StableKey64: different inputs = different hashes
- [ ] StringPool: intern returns same pointer for same string
- [ ] StringPool: string_view valid after additional interns
- [ ] StringPool: clearLookup() doesn't invalidate views

**Acceptance Criteria:**
- [ ] All tests pass
- [ ] No changes to existing code
- [ ] NO `std::hash` partial specialization in `namespace std`

**Time:** 1-2 days

---

### PR2 — ResourceRegistry v1

**Goal:** New registry with JSON loader, deterministic finalize.

**New Files:**
```
engine/include/core/resources/registry/resource_entry.h
engine/include/core/resources/registry/resource_registry.h
engine/src/core/resources/registry/resource_registry.cpp
```

**Functionality:**
- `loadFromSources(span<ResourceSource>)` — accepts list (even if one)
- Merge with override policy (layers + load order)
- Internal `finalize()` — sort by StableKey64, assign indices, build name indices
- Type A/B/C error policies per Requirements Guide
- Collision detection + PANIC
- Name index arrays for O(log N) name lookup

**Unit Tests:**
- [ ] Load valid JSON
- [ ] Detect duplicate canonical keys **within same source** → PANIC
- [ ] Duplicate canonical keys **across sources** → override (no error)
- [ ] Detect invalid key format → PANIC
- [ ] Detect StableKey64 collision → PANIC
- [ ] Determinism: same JSON = same indices (run 100 times)
- [ ] Override: mod overwrites core
- [ ] `findByName` returns correct key (uses name index)
- [ ] `findByStableKey` returns correct key

**Acceptance Criteria:**
- [ ] All tests pass
- [ ] No `unordered_map` in runtime API
- [ ] `finalize()` is internal, not public

**Time:** 2 days

---

### PR3 — ResourceManager Cache by Index

**Goal:** Cache uses vector slots, not map.

**Changes:**
```
engine/include/core/resources/resource_manager.h
engine/src/core/resources/resource_manager.cpp
engine/include/core/resources/holders/resource_holder.h (if needed)
```

**Functionality:**
- `ResourceManager` owns `ResourceRegistry`
- Cache: `vector<unique_ptr<Resource>>` indexed by `key.index()`
- `getTexture(TextureKey)` → direct vector access, fallback on invalid
- `getFont(FontKey)` → direct vector access, fallback on invalid
- `tryGetSound(SoundKey)` → returns `nullptr` on invalid
- `findTexture(string_view)` → delegates to registry

**Unit Tests:**
- [ ] Get texture by key
- [ ] Get font by key
- [ ] Invalid TextureKey returns fallback
- [ ] Invalid SoundKey returns nullptr
- [ ] No map lookup in get* path

**Acceptance Criteria:**
- [ ] All tests pass
- [ ] Cache is vector-based (no maps)

**Time:** 1 day

---

### PR4 — Cutover Hot Path

**Goal:** All components and systems use new keys.

**Changes (in order):**

1. **SpriteComponent:**
   ```cpp
   // OLD:
   core::resources::ids::TextureID textureId{TextureID::Unknown};
   
   // NEW:
   core::resources::TextureKey texture{};
   ```

2. **RenderSystem:**
   - `RenderKey.textureId` → `RenderKey.texture`
   - Implement epoch arrays for per-frame cache (NO maps)
   - Sort by `key.sortKey()` (= index)

3. **sprite_properties.h:**
   - `TextureID` → `TextureKey`

4. **Config loaders:**
   - Parse string → `TextureKey` via `registry.findTextureByName()`
   - Use project JSON accessors
   - Validate on write (PANIC if not found)

5. **background_renderer:**
   - `TextureID` → `TextureKey`

6. **stress_scene:**
   - Remove dense enum hack
   - Use explicit key list or iterate registry

**Changes to resources.json:**
- Rename keys to namespaced format
- Add `"version": 1`

**Unit Tests:**
- [ ] Render with new keys
- [ ] Config loading produces valid keys
- [ ] Stress scene works without enum

**Acceptance Criteria:**
- [ ] RenderSystem uses epoch arrays (NO `unordered_map` in render loop)
- [ ] No strings/maps in hot loop
- [ ] All existing functionality preserved

**Time:** 2-3 days

---

### PR5 — Purge Legacy

**Goal:** Legacy = 0.

**Delete:**
```
engine/include/core/resources/ids/resource_ids.h
engine/include/core/resources/ids/resource_id_utils.h
engine/include/core/resources/paths/resource_paths.h
engine/src/core/resources/paths/resource_paths.cpp
engine/include/core/resources/ids/
engine/include/core/resources/paths/
```

**Remove from code:**
- All `ids::TextureID`, `ids::FontID`, `ids::SoundID`
- All `toString()`/`fromString()` for enums
- All includes of deleted headers

**Acceptance Criteria:**
- [ ] `grep -r "TextureID\|FontID\|SoundID" engine/` = 0 results
- [ ] `grep -r "resource_ids.h\|resource_paths.h" .` = 0 results
- [ ] `grep -r "legacy resource ids/paths" .` = 0 results
- [ ] Compiles without warnings
- [ ] All tests pass

**Time:** 0.5 day

---

### PR6 — Compiled Registry Tool (Roadmap)

**Goal:** JSON → Binary for production.

**New Files:**
```
tools/resource_compiler/main.cpp
tools/resource_compiler/CMakeLists.txt
```

**Functionality:**
- Read resources.json
- Validate all keys and paths
- Output resources.bin:
  - String blob (contiguous)
  - Entry arrays
  - StableKey64 for each entry

**Runtime:**
- `ResourceRegistry::loadFromBinary(span<byte>)`
- mmap + zero parsing

**Time:** 2-3 days

---

### PR7 — Hot Reload Minimal (Roadmap)

**Goal:** File watcher + reload in-place.

**New Files:**
```
engine/include/core/resources/watcher/file_watcher.h
engine/src/core/resources/watcher/file_watcher_win32.cpp
engine/src/core/resources/watcher/file_watcher_linux.cpp
```

**Functionality:**
- Watch `assets/` directory
- On file change: reload texture/font in-place
- Append-only add (dev mode)

**Constraints:**
- No deletion
- No key rename
- Full rebuild = restart

**Time:** 2-3 days

---

### PR8 — Texture color space flag (sRGB)

Причина: в RRv1 примере для текстур есть smooth/repeated/mipmap, но нет srgb; а в Texture Standard sRGB — non-negotiable.
Суть:

добавить bool srgb в TextureResourceConfig

расширить resources.json ("srgb": true/false, default = true для color textures в Atrapacielos)

загрузчик: sf::Texture::loadFromFile(path, {srgb}) (или эквивалентный вызов SFML 3)

Почему это не конфликтует с RRv1: это просто расширение config'а; ключи/StableKey/RuntimeKey остаются прежними. RRv1 принцип "validate-on-write" сохраняется. 

resource_registry_v1_spec v2

---

### PR9 — Arrays sharding + capability-driven fallback (engine backend)

Причина: убрать магические пороги вида "<2048 → fallback" и перейти на "need vs supported".
Суть:

backend выбирается по capabilities

arrays шардируются по maxLayers

multi-atlas остаётся гарантированным fallback (и forced-test)

(Это согласуется с вашим RoadMap, где multi-atlas already primary, arrays optional; мы лишь делаем arrays "primary where applicable" без потери fallback.) 

retributio_atrapacielos_RoadMap_v2.3

---

## 12. Validation Checklist

### 12.1 Post-PR5 Validation

- [ ] `grep -r "TextureID" engine/` = 0 results
- [ ] `grep -r "FontID" engine/` = 0 results
- [ ] `grep -r "SoundID" engine/` = 0 results
- [ ] `grep -r "resource_ids.h" .` = 0 results
- [ ] `grep -r "resource_paths.h" .` = 0 results
- [ ] `grep -r "fromString.*Texture" .` = 0 results
- [ ] `grep -r "toString.*Texture" .` = 0 results
- [ ] `grep -r "legacy resource id headers" .` = 0 results
- [ ] `grep -r "legacy resource paths headers" .` = 0 results
- [ ] `grep -r "legacy enum string converters" .` = 0 results

### 12.2 Performance Validation

- [ ] RenderSystem: uses epoch arrays, NOT `unordered_map`
- [ ] RenderSystem: no string operations in render loop
- [ ] RenderSystem: no map lookups in render loop
- [ ] ResourceManager::getTexture: single vector index access
- [ ] ResourceRegistry::findTextureByName: O(log N) via name index
- [ ] No `unordered_map` in any runtime path

### 12.3 Determinism Validation

- [ ] Unit test: same JSON → same indices (run 100 times)
- [ ] Unit test: different JSON key order → same indices (after sort)
- [ ] Unit test: mod override produces expected winner

### 12.4 Documentation Validation

- [ ] Requirements Guide updated
- [ ] No "enum" references in resource documentation
- [ ] API comments accurate

---

## Appendix A: Code Template Policies

### A.1 ResourceKey Template (v1.0.1 — zero sentinel)

```cpp
// engine/include/core/resources/keys/resource_key.h
#pragma once

#include <cstdint>
#include <compare>

namespace core::resources {

template <typename Tag>
struct ResourceKey {
    static constexpr uint32_t IndexMask = 0x00FFFFFF;
    static constexpr uint32_t GenerationShift = 24;
    
    uint32_t raw = 0;  // 0 = invalid
    
    constexpr ResourceKey() noexcept = default;
    
    // Create valid key: stores (index + 1) in lower 24 bits
    explicit constexpr ResourceKey(uint32_t index, uint8_t generation = 0) noexcept
        : raw((static_cast<uint32_t>(generation) << GenerationShift) | 
              ((index + 1) & IndexMask)) 
    {}
    
    [[nodiscard]] constexpr uint32_t index() const noexcept { 
        return (raw & IndexMask) - 1;  // Subtract bias
    }
    
    [[nodiscard]] constexpr uint8_t generation() const noexcept { 
        return static_cast<uint8_t>(raw >> GenerationShift); 
    }
    
    [[nodiscard]] constexpr bool valid() const noexcept { 
        return raw != 0; 
    }
    
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return valid();
    }
    
    [[nodiscard]] constexpr uint32_t sortKey() const noexcept {
        return index();
    }
    
    auto operator<=>(const ResourceKey&) const = default;
};

struct TextureTag {};
struct FontTag {};
struct SoundTag {};

using TextureKey = ResourceKey<TextureTag>;
using FontKey = ResourceKey<FontTag>;
using SoundKey = ResourceKey<SoundTag>;

// Explicit hashers (NOT std::hash partial specialization!)
struct TextureKeyHash {
    size_t operator()(TextureKey key) const noexcept {
        return std::hash<uint32_t>{}(key.raw);
    }
};

struct FontKeyHash {
    size_t operator()(FontKey key) const noexcept {
        return std::hash<uint32_t>{}(key.raw);
    }
};

struct SoundKeyHash {
    size_t operator()(SoundKey key) const noexcept {
        return std::hash<uint32_t>{}(key.raw);
    }
};

} // namespace core::resources
```

**Critical:** Do NOT add `template<typename Tag> struct hash<ResourceKey<Tag>>` in `namespace std`. Partial specializations in `std` are not permitted. Use explicit hashers above.

### A.2 StableKey Computation

```cpp
// engine/include/core/resources/keys/stable_key.h
#pragma once

#include <cstdint>
#include <string_view>

#define XXH_INLINE_ALL
#include "third_party/xxhash/xxhash.h"

namespace core::resources {

// Fixed seed = 0, never change
inline constexpr uint64_t StableKeySeed = 0;

[[nodiscard]] inline uint64_t computeStableKey(std::string_view canonicalKey) noexcept {
    return XXH3_64bits_withSeed(canonicalKey.data(), canonicalKey.size(), StableKeySeed);
}

} // namespace core::resources
```

### A.3 Key Validation

```cpp
// In engine/src/core/resources/registry/resource_registry.cpp

namespace {

[[nodiscard]] bool isValidKeySegment(std::string_view segment) noexcept {
    if (segment.empty()) return false;
    
    // First char: [a-z]
    if (segment[0] < 'a' || segment[0] > 'z') return false;
    
    // Rest: [a-z0-9_-]
    for (size_t i = 1; i < segment.size(); ++i) {
        char c = segment[i];
        bool valid = (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') ||
                     c == '_' || c == '-';
        if (!valid) return false;
    }
    
    return true;
}

[[nodiscard]] bool isValidCanonicalKey(std::string_view key) noexcept {
    if (key.empty()) return false;
    
    size_t segmentCount = 0;
    size_t start = 0;
    
    for (size_t i = 0; i <= key.size(); ++i) {
        if (i == key.size() || key[i] == '.') {
            if (i == start) return false;  // Empty segment
            
            std::string_view segment = key.substr(start, i - start);
            if (!isValidKeySegment(segment)) return false;
            
            ++segmentCount;
            start = i + 1;
        }
    }
    
    // Minimum 3 segments: namespace.category.name
    return segmentCount >= 3;
}

} // namespace
```

---

## Appendix B: Migration Examples

### B.1 SpriteComponent

```cpp
// OLD (sprite_component.h)
struct SpriteComponent {
    core::resources::ids::TextureID textureId{core::resources::ids::TextureID::Unknown};
    // ...
};

// NEW (sprite_component.h)
struct SpriteComponent {
    core::resources::TextureKey texture{};  // raw == 0 = invalid
    // ...
};
```

### B.2 RenderSystem Per-Frame Cache (epoch arrays)

```cpp
// OLD (unordered_map - WRONG)
std::unordered_map<TextureID, const sf::Texture*> mTextureCache;

// NEW (epoch arrays - CORRECT)
class RenderSystem {
    std::vector<const sf::Texture*> mFrameTexturePtr;
    std::vector<uint32_t> mFrameTextureStamp;
    uint32_t mFrameId = 0;
    
    void initCache(size_t textureCount) {
        mFrameTexturePtr.resize(textureCount, nullptr);
        mFrameTextureStamp.resize(textureCount, 0);
    }
    
    const sf::Texture* getTextureCached(TextureKey key) {
        const uint32_t idx = key.index();
        
        if (mFrameTextureStamp[idx] != mFrameId) {
            mFrameTexturePtr[idx] = &mResources->getTexture(key);
            mFrameTextureStamp[idx] = mFrameId;
        }
        
        return mFrameTexturePtr[idx];
    }
    
    void beginFrame() {
        ++mFrameId;
    }
};
```

### B.3 RenderSystem Sorting

```cpp
// OLD (render_system.cpp)
std::sort(mKeys.begin(), mKeys.end(), [](const RenderKey& a, const RenderKey& b) {
    // ...
    const auto aTex = core::resources::ids::toUnderlying(a.textureId);
    const auto bTex = core::resources::ids::toUnderlying(b.textureId);
    // ...
});

// NEW (render_system.cpp)
std::sort(mKeys.begin(), mKeys.end(), [](const RenderKey& a, const RenderKey& b) {
    // ...
    const auto aTex = a.texture.sortKey();
    const auto bTex = b.texture.sortKey();
    // ...
});
```

### B.4 Config Loading

```cpp
// OLD (config_loader.cpp)
TextureID id = ids::fromString<TextureID>(jsonValue);
if (id == TextureID::Unknown) {
    LOG_PANIC(...);
}
out.textureId = id;
// Legacy enum-based parsing from string (removed in PR5).

// NEW (config_loader.cpp)
std::string textureKeyStr;
if (!tryParseString(json, "texture", textureKeyStr)) {
    LOG_PANIC(log::cat::Config, "Missing 'texture' in config");
}

TextureKey key = resourceManager.findTexture(textureKeyStr);
if (!key.valid()) {
    LOG_PANIC(log::cat::Config, "Unknown texture '{}'", textureKeyStr);
}
out.texture = key;
```

### B.5 stress_scene.cpp

```cpp
// OLD (dense enum hack)
for (std::size_t i = 0; i < requestedCount; ++i) {
    const U candidate = static_cast<U>(base + static_cast<U>(i));
    const auto id = static_cast<TextureID>(candidate);
    if (tryValidateTexture(resources, id)) {
        out.push_back(id);
    }
}

// NEW (iterate registry)
const auto& registry = resources.registry();
const size_t count = std::min(registry.textureCount(), requestedCount);
for (size_t i = 0; i < count; ++i) {
    stressTextures.push_back(TextureKey(static_cast<uint32_t>(i), 0));
}
```

---

## Appendix C: Timeline Summary

| PR | Duration | Dependencies | Deferrable |
|----|----------|--------------|------------|
| PR0: Spec | 0.5 day | — | No |
| PR1: Keys | 1-2 days | PR0 | No |
| PR2: Registry | 2 days | PR1 | No |
| PR3: Manager | 1 day | PR2 | No |
| PR4: Cutover | 2-3 days | PR3 | No |
| PR5: Purge | 0.5 day | PR4 | No |
| **Total Core** | **7-9 days** | | |
| PR6: Compiler | 2-3 days | PR5 | Yes |
| PR7: Hot-reload | 2-3 days | PR5 | Yes |

---

**End of Specification**
