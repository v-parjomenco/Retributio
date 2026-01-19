# SkyGuard Texture Standard v1.1 (corrected, no DDS, RRv1-compatible)

**Project:** SFML1 (codename) | **Library:** SFML 3.0.2  
**Engine Target:** Titan 4X (flagship, 100k+ registered textures; bounded active set via streaming/cache)  
**Current Product:** SkyGuard (MVP, casual shooter for ages 6-13)

**Philosophy:** One engine, two profiles. SkyGuard ships fast; engine decisions always stay compatible with Titan scale.

---

## 1. PLATFORM & OPENGL BASELINE (Tiers)

### Tier0 — SkyGuard (2026, broad reach)
- **OpenGL:** 3.3 minimum
- **VRAM:** 1GB minimum, 2GB comfortable
- **FPS floor (release):** 60 FPS
- **Stress tests:** dips allowed only as signal for LOD/streaming (not a target)

### Tier1 — Titan 4X (2030+, forward-looking)
- **OpenGL:** 4.5 minimum
- **VRAM:** 2GB minimum, 4GB comfortable
- **FPS floor (release):** 60 FPS

**Rule:** Engine picks render backend by capabilities (tiered), never hard-coded thresholds.

---

## 2. TEXTURE FORMATS & COMPRESSION STRATEGY

### SkyGuard (Months 1–6, MVP)
- **Format:** PNG → RGBA8 in VRAM
- **Mipmaps:** generated once on load (not hot path)
- **sRGB:** enabled for color textures
- **Compression:** none (small asset count)

### Titan 4X Foundation (post-SkyGuard)
**BC compression mandatory** (desktop Windows/Linux).

- **GPU formats:**
  - **BC3 (DXT5):** default for color+alpha
  - **BC7:** only for hero/UI where visibly needed (costlier to encode)
- **Mipmaps:** offline mipchain (higher quality, predictable)
- **Container:** custom (**NOT DDS**; NOT relying on SFML loader)
- **Runtime upload:** `glCompressedTexImage2D`
- **Fallback:** if a BC format unsupported → decode to RGBA8 + log 1 warning (quality drop, continue)

**Note:** “DDS support in SFML” is intentionally NOT assumed. DDS is out of scope/removed.

---

## 3. NAMING CONVENTIONS (RRv1-compatible)

### File names
- `lowercase_snake_case.png`
- No `_texture` suffix
- Semantic suffix only:
  - `*_tile.png` (tiled)
  - `*_lod0.png`, `*_lod1.png` (explicit LOD levels)
  - era/culture variants: `rome_ancient.png` etc.

### Canonical Key Strings (engine “source of truth”)
**MUST be dot-separated, 3+ segments (RRv1):**  
`namespace.category.name[.variant...]`

Examples:
- `core.texture.missing`
- `core.texture.white`
- `skyguard.background.desert`
- `skyguard.aircraft.su57`
- `titan.unit.infantry.spearman`
- `titan.city.roman.ancient`

**Rule:** keys are stable forever (mods/saves). No slashes.

### Runtime mapping
- Runtime uses `{arrayId, layer}` or `{atlasId, rect}` from generated metadata.
- Hot path uses only RuntimeKey32 handles (no strings/maps).

---

## 4. FOLDER STRUCTURE

```
assets/
├─ core/
│  ├─ fonts/
│  └─ textures/
│     └─ placeholders/
│        ├─ missing_texture.png   (64×64)
│        ├─ white.png             (1×1)
│        └─ black.png             (1×1)
├─ game/
│  ├─ skyguard/
│  │  └─ textures/
│  │     ├─ backgrounds/
│  │     ├─ aircraft/
│  │     ├─ particles/
│  │     └─ ui/
│  └─ titan4x/
│     └─ textures/
│        ├─ terrain/
│        ├─ cities/
│        ├─ units/
│        ├─ buildings/
│        └─ ui/
```

Source art (optional): `art_source/` outside `assets/` (Git LFS optional).

---

## 5. TEXTURE SIZES & RESOLUTION STANDARDS

### Texel density targets (by profile)
- **SkyGuard:** 0.8–1.2 typical, 1.5–2.0 only for hero/menu close-ups
- **4X:** 0.5–1.0 typical, 1.0–1.5 where readability demands

### SkyGuard sizes

| Asset Type | Resolution |
|---|---|
| Backgrounds (tiled) | 1024×1024 (upgrade to 2048 if repetition visible) |
| Player aircraft | 1024×1024 |
| Enemy aircraft | 512×512 (or 1024 if same asset can be player choice) |
| Particles/bullets | 64×64 (atlas) |
| UI icons | 128×128 |

**No role-degrading:** if `su57` can be player or enemy, it stays one quality level (single asset).

### 4X sizes (baseline readability)

| Asset Type | Resolution |
|---|---|
| Terrain layers (per-layer tiles) | 256×256 |
| Cities (base) | 256×256 |
| Units (sprites) | 128×128 |
| Improvements/overlays | 64×64 |
| Strategic view icons | 32×32 |

---

## 6. QUALITY REQUIREMENTS (NON-NEGOTIABLE)

### 6.1 sRGB correctness
- Color textures: load as sRGB
- Data textures: linear
- Framebuffer: request sRGB-capable context when available

### 6.2 Mipmaps
- SkyGuard: generate once on load
- 4X: offline mipchain
- Failure: warn once, continue

### 6.3 Alpha blending
- Straight alpha (SFML default)
- Asset rule: edge-bleeding, no halos
- Premultiplied alpha: deferred; requires pipeline + blend mode change

### 6.4 Filtering & repeat
- Smooth: true for realistic art
- Repeat: true for tiled backgrounds
- NPOT repeat artifacts on very old GPUs accepted risk (Tier0 baseline is still sane)

---

## 7. 4X MUST: MULTI-LAYER TERRAIN (shader blending)

Avoid combinatorial explosion by layering:
- Relief array
- Vegetation array
- Hydrology array

Blend in shader (alpha-driven).

Budget is explicit: 3–4 samples/pixel typical; adjust by quality preset if needed.

---

## 8. 4X PRIMARY STORAGE: TEXTURE ARRAYS (SHARDED) + FALLBACK

### Primary path (Tier1)
- Texture arrays for uniform-size categories (64/128/256...)
- Sharding by `GL_MAX_ARRAY_TEXTURE_LAYERS` per GPU:
  - `units_128_shard_0..N`
  - `cities_256_shard_0..N`
- No fixed “2048 threshold”; decide by “need vs supported”.

### Fallback path (Tier0 and safety)
- Multi-atlas renderer is always available and testable via forced flag.
- Visual parity required.

---

## 9. LOD SYSTEM (zoom-driven)

Two options:
- **Option B (default MVP):** rely on mipmaps + minification (no duplicate assets)
- **Option A (if VRAM constrained):** discrete LOD assets (`*_lod0`, `*_lod1`), loaded by zoom

Strategic view (<0.25 zoom):
- cities → icons/counters
- units → counters
- terrain → simplified rendering (optional procedural)

---

## 10. CITY VIEW (keep)

Static strategic UI mode (not full simulator):
- grid, placement, info panels
- sprites 64/128; base background 512; expand later

---

## 11. ASSET PIPELINE (4X requirement)

- SkyGuard: manual PNG ok.
- 4X: automation mandatory:
  - validate keys/naming/sizes
  - resize to size-classes
  - offline mipchain
  - BC compress (BC3 default, BC7 selective)
  - pack into array shards / UI atlases
  - emit metadata mapping canonical keys → runtime bindings

Incremental rebuild (hash-based) is required.

---

## 12. CONTENT TARGETS (architecture vs content)
- Engine scales to 100k+ registered textures.
- 4X ships with 500–1k initially; expands later.

---

## 13. PLACEHOLDERS (core)

- `missing_texture.png` 64×64
- `white.png` 1×1
- `black.png` 1×1

Canonical keys:
- `core.texture.missing`
- `core.texture.white`
- `core.texture.black`

---

## 14. FORBIDDEN PRACTICES
- Hardcoded asset counts
- Runtime image processing in hot paths
- Duplicate identical assets for roles
- Resolution suffix in filenames

---

## 15. PERFORMANCE RULES
- No image processing in render loop
- Upload on load phase (async in 4X)
- Batch by texture binding (array/atlas grouping)
- Profile-first
- Log/track texture memory on scene changes

---

## 16. VISUAL STYLE GUIDELINES
- **SkyGuard:** vibrant, high contrast, clean silhouettes, realistic aircraft reference look.
- **4X:** muted palette, historical atmosphere, readability-first (Port Royale 2 vibe).

AI prompts MUST specify profile (SkyGuard vs 4X) explicitly.

---

## 17. METADATA STRUCTURE (generated)
Use canonical keys (dot-format), map to array shard/layer or atlas rect.

---

## 18. ROADMAP INTEGRATION
- SkyGuard: PNG workflow + sRGB + mipmaps + naming enforcement.
- 4X: pipeline + BC compression + arrays primary + multi-atlas fallback + LOD.

---

## 19. TESTING & VALIDATION
- Forced fallback tests
- Screenshot diff tests (array vs atlas)
- VRAM logging
- Asset validation in CI

---

## 20. FINAL CHECKLIST
- Tier0/Tier1 baselines confirmed
- RRv1 canonical keys are dot-format
- sRGB + mipmaps verified
- BC pipeline scheduled post-SkyGuard
- Array sharding logic designed
- Multi-atlas fallback always present
- Automation planned
