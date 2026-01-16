# Resource Layer Overview (Resource Registry v1)

This document describes the **resource layer** of the engine: how textures, fonts and sounds are
**identified**, **defined**, **loaded**, **cached**, and **accessed** in a consistent, data-driven way.

The design scales from small games (SkyGuard) up to large 4X projects (100k+ unique resources, data-only mods),
while keeping **zero-overhead in hot paths** and **deterministic behavior**.

---

## 1. High-Level Pipeline

```text
             Authoring / Mods / DLC                        Runtime (hot path)
        (resources.json + ordered sources)               (ECS + RenderSystem)
                       │
                       ▼
              Resource Registry Build (validate-on-write)
    - validate Canonical Key String format
    - merge by override policy (layer + loadOrder)
    - compute StableKey64 (xxHash3_64, seed=0)
    - detect StableKey64 collisions → PANIC
    - sort by StableKey64 total order
    - assign RuntimeKey32 indices deterministically
                       │
                       ▼
                ResourceRegistry (cold-ish storage)
      - typed vectors: textures / fonts / sounds
      - debug indices: name → RuntimeKey32 (O(log N))
                       │
                       ▼
                    ResourceManager (façade)
        (public API: getTexture / getFont / tryGetSound)
                       │
                       ▼
        Vector-backed runtime caches (index-addressed, no maps)
      - nullptr means “not loaded yet”
      - O(1) by RuntimeKey32.index()
                       │
                       ▼
        TextureResource / FontResource / SoundBufferResource
             (thin wrappers around SFML resources)
```

**Key ideas (v1):**

- Resource identity is **not** compile-time enums; it is a runtime contract:
  **Canonical Key String** + **StableKey64** + **RuntimeKey32**.
- **Validate on write, trust on read:** all validation happens at registry build / config load boundaries.
- **Hot path uses only RuntimeKey32** (no strings, no hashing, no maps).
- Registry build and runtime indices are **deterministic** given the same ordered sources.

---

## 2. Core Concepts

### 2.1 Resource identities (frozen contract)

Resource Registry v1 defines three identities:

- **Canonical Key String** — canonical, human-readable, mod-friendly identifier.
  - Format: `namespace.category.name`.
  - Minimum 3 segments separated by `.`.
  - Each segment: `[a-z][a-z0-9_-]*`.
  - Entire key is lowercase; allowed chars: `[a-z0-9._-]`.
  - Regex: `^[a-z][a-z0-9_-]*(\.[a-z][a-z0-9_-]*){2,}$`.
  - Invalid format → **PANIC** at registry build.

- **StableKey64** — persistence/cross-version stability.
  - Derived deterministically from the canonical key string.
  - Hash: **xxHash3_64**, seed **0** (frozen; never change for v1).
  - Collision (two different canonical keys with the same StableKey64) → **PANIC**.

- **RuntimeKey32** — hot path handle (ECS components, render loops).
  - Encodes `(index + 1)` in low 24 bits and generation in high 8 bits.
  - `raw == 0` is the only invalid sentinel.
  - In v1, generation is stored but **not checked in Release**.

**Rule:** runtime code stores and uses **only RuntimeKey32**.
Strings and StableKey64 are load-time / tooling / persistence only.

### 2.2 Resource configs (`*ResourceConfig`)

`*ResourceConfig` describes *how to load/configure* low-level resources (path + low-level flags),
not how they are used in gameplay.

```cpp
struct TextureResourceConfig {
    std::string path;
    bool smooth         = true;
    bool repeated       = false;
    bool generateMipmap = false;
};

struct FontResourceConfig {
    std::string path;
};

struct SoundResourceConfig {
    std::string path;
    // v1: no streaming flags; extension point.
};
```

Separation of concerns:

- `*ResourceConfig` — low-level GPU / `sf::SoundBuffer` configuration (path, flags).
- Higher-level usage (character size, UI layout, sound volume, looping) belongs to higher-level systems
  (blueprints, UI, audio), not the resource layer.

### 2.3 JSON registry (`resources.json`) and ordered sources

The runtime registry is built from **one or more ordered sources** (Core/DLC/Patches/Mods),
each with:

- `layerPriority` (frozen layer policy)
- `loadOrder` (explicit ordering within the same layer)
- `sourceName` (diagnostics)
- `path` (source file)

**Duplicate keys:**

- Duplicate canonical key **within one source file** → **PANIC** (authoring error).
- Duplicate canonical key **across sources** → expected; resolved deterministically by override policy.

#### 2.3.1 Canonical JSON layout (namespaced)

Example:

```json
{
  "textures": {
    "skyguard.sprite.player": {
      "path": "assets/game/skyguard/images/su-57.png",
      "smooth": true,
      "repeated": false,
      "mipmap": false
    },
    "core.texture.missing": {
      "path": "assets/core/images/missing.png",
      "smooth": false,
      "repeated": false,
      "mipmap": false
    }
  },
  "fonts": {
    "core.font.default": {
      "path": "assets/core/fonts/Wolgadeutsche.otf"
    }
  },
  "sounds": {
    "skyguard.ui.click": {
      "path": "assets/core/sounds/ui_click.wav"
    }
  }
}
```

Notes:

- Keys in `textures/fonts/sounds` are **canonical key strings**.
- Values are config objects (even for fonts/sounds) to keep the format uniform and extensible.

#### 2.3.2 Validation and error handling

At registry build:

- validate canonical key format
- validate per-type JSON shape and required fields
- merge sources deterministically (override policy)
- compute StableKey64 and detect collisions
- assign RuntimeKey32 indices deterministically

Failure policy:

- **Textures/Fonts:** Type A → **PANIC** on any registry build failure.
- **Sounds:** soft-fail → **WARN + skip** invalid/missing entries (never PANIC due to sound assets).

---

## 3. ResourceRegistry: runtime index + debug indices

`ResourceRegistry` is the built, immutable runtime representation:

- typed vectors per kind (textures/fonts/sounds), ordered by StableKey64 total order
- O(1) access by `RuntimeKey32.index()`
- debug indices (tools/overlay only):
  - canonical name → RuntimeKey32 (sorted by name; O(log N))
  - stableKey → RuntimeKey32 (binary search over stable-sorted entries; or an optional index)

Fallback keys (Type A):

- `core.texture.missing`
- `core.font.default`

They must exist and be resolvable during finalize/build; otherwise **PANIC**.

---

## 4. ResourceManager: public API (hot path safe)

`ResourceManager` is the high-level façade used by gameplay, ECS systems, UI, etc.

### 4.1 RuntimeKey32-based access (canonical runtime path)

```cpp
const sf::Texture& getTexture(TextureKey key); // returns fallback on invalid
const sf::Font&    getFont(FontKey key);       // returns fallback on invalid
const sf::SoundBuffer* tryGetSound(SoundKey key); // returns nullptr on invalid/missing
```

Rules:

- Texture/Font access never throws in runtime hot paths; invalid keys map to fallbacks.
- Sound access is pointer-returning because “skip on missing” is part of the policy.

### 4.2 Runtime caches (no maps)

ResourceManager caches loaded SFML resources using **vector-backed caches indexed by runtime index**:

- `std::vector<std::unique_ptr<sf::Texture>>` (or wrapper) sized to `registry.textureCount()`
- `nullptr` means “not loaded yet”

This guarantees:

- O(1) lookups by index
- stable cache locality
- zero associative container overhead

### 4.3 Debug/tooling lookup (cold path)

For tools and debugging, name/stable-key lookups may exist:

- resolve canonical key string → `TextureKey/FontKey/SoundKey` via debug index (O(log N))
- resolve StableKey64 similarly

These lookups are **not** permitted in render hot loops.

---

## 5. Render hot path integration (per-frame cache)

Hard rule:

- Render hot loops must not do string lookups, hashing, or map lookups.

Correct per-frame cache contract:

- RenderSystem keeps two arrays sized to `registry.textureCount()`:
  - `mFrameTexturePtr[idx]` (`const sf::Texture*`)
  - `mFrameTextureStamp[idx]` (`uint32_t`)
- RenderSystem increments `mFrameId` each frame.
- For a texture key:
  - `idx = key.index()`
  - if `mFrameTextureStamp[idx] != mFrameId`:
    - `mFrameTexturePtr[idx] = &resources.getTexture(key)`
    - `mFrameTextureStamp[idx] = mFrameId`
  - use `mFrameTexturePtr[idx]` for draw

This yields **one ResourceManager call per unique texture per frame**, with **zero overhead per sprite**.

---

## 6. Error-handling strategy by layer

**Registry build / config load boundary:**

- Textures/Fonts: Type A → PANIC on any invalid registry/build condition.
- Sounds: WARN + skip invalid/missing entries.

**Runtime access:**

- invalid TextureKey → fallback texture
- invalid FontKey → fallback font
- invalid/missing SoundKey → `tryGetSound()` returns nullptr; caller skips

---

## 7. Usage examples & patterns

### 7.1 Load-time resolution (validate-on-write)

Blueprint/config loaders parse canonical key strings and resolve them **once** at load time:

```cpp
// Load-time boundary code (may log/PANIC based on policy).
// After this point, the runtime stores only TextureKey.
TextureKey playerTex = registry.resolveTextureKey("skyguard.sprite.player");
entity.add<SpriteComponent>({ .textureKey = playerTex, /* ... */ });
```

### 7.2 Runtime usage (trust-on-read)

```cpp
// Hot path: only TextureKey, no strings.
const sf::Texture& tex = resourceManager.getTexture(sprite.textureKey);
```

### 7.3 Tooling / debug usage (cold path)

```cpp
// Tools/overlay: canonical name lookup is allowed (O(log N)), not in hot loops.
auto key = registry.tryResolveTextureKeyByName("skyguard.sprite.player");
```

---

## 8. Future extensions (roadmap)

- **Compiled registry:** offline-built registry binary to reduce load-time cost and strengthen determinism guarantees.
- **Hot reload:** v1 supports reload-in-place and append-only; full rebuild is not supported in v1.
- **Streaming:** layered on top of ResourceManager with budgets and streaming policies.
- **Richer configs:** extend `*ResourceConfig` without changing the identity contract.
