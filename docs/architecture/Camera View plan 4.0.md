# SFML1 / SkyGuard — CAMERA & VIEW ARCHITECTURE v4.0
## Fixed Logical Size · Letterbox · World/UI Separation

**Document Version**: 4.0  
**Date**: 2026-01-09  
**Status**: Ready for Implementation  
**Target**: SkyGuard (arcade vertical shoot 'em up)

---

## 📐 ARCHITECTURE OVERVIEW

### **CORE PRINCIPLES** (железобетон):

```
✅ Fixed Logical World Size (из конфига, НЕ hardcode)
✅ WorldView.size НИКОГДА не меняется при resize
✅ Letterbox/Pillarbox viewport при resize
✅ Camera = Presentation Controller (НЕ ECS система)
✅ Два View: WorldView (gameplay) + UiView (HUD/debug)
✅ Transform.position = World Space ВСЕГДА
✅ Resize НЕ влияет на gameplay
✅ Core остаётся game-agnostic
✅ Максимум 2 игрока
```

### **COORDINATE SPACES** (чёткое разделение):

| Пространство | Для чего | Кто живёт | View |
|--------------|----------|-----------|------|
| **World** | Симуляция | Transform, Physics, AI, Player | worldView |
| **Screen** | Пиксели окна | Raw input, viewport calc | — |
| **UI** | Интерфейс | HUD, Debug Overlay, Menus | uiView |

```cpp
// НИКОГДА не смешивать!
// ❌ transform.position в screen space
// ❌ UI position в world space
// ❌ компенсация камеры вручную
```

### **VIEW ARCHITECTURE**:

```
WINDOW (pixels)
 │
 ├── WorldView
 │    size     = worldLogicalSize (fixed, from config, e.g. 1024x768)
 │    center   = player.position + cameraOffset
 │    viewport = letterboxed rect (black bars on sides/top-bottom)
 │
 └── UiView
      size     = uiLogicalSize (fixed, from config, e.g. 1024x768)
      center   = uiLogicalSize * 0.5f (static)
      viewport = FULL WINDOW {0, 0, 1, 1} (covers everything, including black bars!)
```

**⚠️ CRITICAL DISTINCTION:**
```
WorldView uses letterbox viewport → gameplay area with black bars
UiView ALWAYS covers full window → HUD renders OVER black bars

This is the standard approach in professional games:
- Black bars are part of visual design
- UI elements (health, score, buttons) appear over the entire screen
- Player never loses access to UI due to aspect ratio
```

**Ключевые принципы UI:**
```
✅ UiView uses fixed logical size with uniform scaling
✅ UiView viewport = {0, 0, 1, 1} ALWAYS (full window)
✅ No pixel-space UI in SkyGuard
✅ UI scales uniformly with window size using fixed logical UI size.
   UI scaling is independent of WorldView letterbox.
✅ UI renders OVER letterbox bars, not inside them
```

---

## 📊 DATA OWNERSHIP & FLOW (проблема #7)

### **Кто владеет какими данными:**

```
┌─────────────────────────────────────────────────────────────────┐
│                        DATA OWNERSHIP                           │
├─────────────────────────────────────────────────────────────────┤
│ ViewConfig (JSON)                                               │
│   owner: Game (loads once at startup)                           │
│   contains: worldLogicalSize, cameraOffset, cameraMinY          │
│                                                                 │
│ ViewManager                                                     │
│   owner: Game                                                   │
│   owns: worldView, uiView, mWorldLogicalSize, mCurrentWindowSize│
│   computes: viewport (on resize), camera center (on update)     │
│                                                                 │
│ Aspect Ratio                                                    │
│   computed by: computeLetterboxViewport()                       │
│   used by: ViewManager::onResize()                              │
│   NEVER stored as separate field (derived data)                 │
│                                                                 │
│ Window Size                                                     │
│   source: sf::Event::Resized                                    │
│   stored in: ViewManager::mCurrentWindowSize                    │
│   propagated to: onResize() → viewport recalc                   │
└─────────────────────────────────────────────────────────────────┘
```

### **Update Flow при Resize:**

```
sf::Event::Resized
       │
       ▼
Game::processEvents()
       │
       └──► ViewManager::onResize(newWindowSize)
                  │
                  ├──► computeLetterboxViewport(worldLogicalSize) → worldView.setViewport()
                  │
                  └──► uiView.viewport stays {0,0,1,1} (always full window)

// NOTE: UiView does NOT use letterbox — it covers full window
// This allows HUD to render OVER black bars
```

### **Update Flow при Camera Follow:**

```
Game::update()
       │
       ├──► ECS systems (movement, etc.)
       │
       └──► Game::updateCamera()
                  │
                  ▼
            Find LocalPlayerTagComponent
                  │
                  ▼
            ViewManager::updateCamera(playerPosition)
                  │
                  └──► worldView.setCenter(playerPos + offset)
```

**Ключевые инварианты:**
- `aspectRatio` — производное значение, не хранится
- `worldLogicalSize` — иммутабельный после init
- `currentWindowSize` — единственный источник истины для viewport

---

## 🎯 PHASE 0: CONFIGURATION & FOUNDATION

### **0.1 View Configuration** (data-driven, NOT hardcoded)

```cpp
// game/skyguard/config/view_config.h
namespace game::skyguard::config {

/**
 * @brief View configuration for SkyGuard.
 * 
 * Source of truth: skyguard_game.json
 * Fallback: compile-time defaults below
 * 
 * CRITICAL: These values define the gameplay area.
 * Changing them affects game balance, level design, enemy patterns.
 */
struct ViewConfig {
    // ============================================================
    // WORLD VIEW (fixed logical size — NEVER changes at runtime)
    // ============================================================
    
    /// Logical world size in world units.
    /// This is the canonical gameplay area visible to player.
    /// All level design, spawn positions, boundaries use these units.
    sf::Vector2f worldLogicalSize{1024.f, 768.f}; // NEVER USE MAGICAL NUMBERS, ALWAYS USE CONSTANTS, for ex. screen size values are from skyguard_game.json. If you do not know - ask.
    
    // ============================================================
    // CAMERA
    // ============================================================
    
    /// Camera offset from player position (world units).
    /// Negative Y = camera looks ahead of player (player at bottom).
    sf::Vector2f cameraOffset{0.f, -100.f};
    
    /// Minimum camera Y to avoid showing invalid world content.
    /// This is a PRESENTATION clamp, not part of simulation.
    /// Interpretation is game-specific:
    /// - SkyGuard: prevents showing below Y=0 (start area)
    /// - Other games: ground level, level boundaries, etc.
    /// 
    /// Default: worldLogicalSize.y * 0.5f (camera can't go below center)
    float cameraMinY{384.f};  // 768 / 2
    
    // ============================================================
    // UI VIEW (fixed logical size — uniform scaling)
    // ============================================================
    
    /// Logical UI size in UI units.
    /// UI scales uniformly with window size using fixed logical UI size.
    /// UI scaling is independent of WorldView letterbox.
    /// This ensures readable text/buttons on any resolution.
    /// 
    /// BENEFITS:
    /// - Large monitors = larger UI (accessibility)
    /// - No pixel-perfect positioning headaches
    /// - Same approach as world (consistency)
    sf::Vector2f uiLogicalSize{1024.f, 768.f};
};

} // namespace game::skyguard::config
```

**skyguard_game.json** (extended):
```json
{
  "window": {
    "width": 1024,
    "height": 768,
    "title": "SkyGuard"
  },
  "view": {
    "world_logical_size": [1024, 768],
    "ui_logical_size": [1024, 768],
    "camera_offset": [0, -100],
    "camera_min_y": 384
  }
}
```

---

### **0.2 Player Tag Component** (marker for camera target)

```cpp
// game/skyguard/ecs/components/player_tag_component.h
namespace game::skyguard::ecs {

/**
 * @brief Marker component — identifies player entity.
 * 
 * Purpose:
 * - Camera finds player to follow
 * - Collision system identifies player
 * - Future: scoring, power-ups, etc.
 * 
 * Multiplayer (max 2 players):
 * - Player 1: has PlayerTagComponent + LocalPlayerTagComponent
 * - Player 2: has PlayerTagComponent only (or own LocalPlayerTag on their client)
 * 
 * Invariants (Debug assertions):
 * - count >= 1 (at least one player must exist)
 * - count <= 2 (max 2 players for co-op)
 */
struct PlayerTagComponent {
    uint8_t playerIndex{0};  // 0 = player 1, 1 = player 2
};

/**
 * @brief Marker — identifies LOCAL player for camera follow.
 * 
 * In single-player: exactly 1 entity has this.
 * In split-screen: each viewport has its own camera following its LocalPlayer.
 * 
 * NOT serialized for replay/network (presentation-only).
 */
struct LocalPlayerTagComponent {};

} // namespace game::skyguard::ecs
```

---

### **0.3 Player Blueprint Update** (simplified — no scaling/lock)

**player.json** — FINAL FORMAT (fields removed):
```json
{
  "texture": "Player",
  "scale": { "x": 0.12, "y": 0.12 },
  "speed": 400.0,
  "acceleration": 800.0,
  "friction": 100.0,
  "turn_rate": 80.0,
  "initial_rotation_degrees": 270.0,
  
  "start_position": { "x": 512, "y": 700 },
  
  "controls": {
    "thrust_forward": "W",
    "thrust_backward": "S",
    "turn_left": "A",
    "turn_right": "D"
  }
}
```

**REMOVED FIELDS:**
- ~~`resize_scaling`~~ — удалён (scaling via View)
- ~~`lock_behavior`~~ — удалён (world coordinates)
- ~~`anchor`~~ — удалён (explicit start_position instead)

**CHANGED FIELDS:**
- `start_position`: `{512, 700}` — explicit world coordinates (center-bottom of 1024x768 logical area)

**Rationale:**
```
SpriteComponent.scale is an AUTHORING/ARTISTIC scale.
It is constant at runtime and UNRELATED to window resize.

- scale {0.12, 0.12} = artist decision, set once at spawn
- View letterbox handles visual scaling to window
- No ScalingSystem modifies component.scale at runtime
```

---

### **0.4 PlayerInitSystem Modifications**

```cpp
// Изменения в PlayerInitSystem::update()
void update(core::ecs::World& world, float) override {
    if (mHasRun) return;
    
    for (std::size_t i = 0; i < mPlayers.size(); ++i) {
        const auto& cfg = mPlayers[i];
        const core::ecs::Entity entity = world.createEntity();
        
        // ✅ NEW: Add player tag
        world.addComponent<PlayerTagComponent>(entity, PlayerTagComponent{
            .playerIndex = static_cast<uint8_t>(i)
        });
        
        // ✅ NEW: First player is local (single-player default)
        if (i == 0) {
            world.addComponent<LocalPlayerTagComponent>(entity);
        }
        
        // ✅ Start position from config (world coordinates)
        core::ecs::TransformComponent tr{};
        tr.position = cfg.startPosition;  // {512, 700} — world space
        tr.rotationDegrees = cfg.aircraftControl.initialRotationDegrees;
        
        // ✅ Sprite component
        // scale is AUTHORING/ARTISTIC — constant at runtime, unrelated to resize
        core::ecs::SpriteComponent spriteComp{};
        spriteComp.textureId = cfg.sprite.textureId;
        spriteComp.scale = cfg.sprite.scale;  // {0.12, 0.12} — artist decision, never changes
        spriteComp.origin = computeOrigin(textureSize);  // center or bottom-center
        spriteComp.zOrder = 0.f;
        // ... textureRect setup ...
        
        world.addComponent<core::ecs::SpriteComponent>(entity, spriteComp);
        world.addComponent<core::ecs::TransformComponent>(entity, tr);
        world.addComponent<core::ecs::VelocityComponent>(entity, vel);
        world.addComponent<core::ecs::MovementStatsComponent>(entity, movement);
        world.addComponent<AircraftControlComponent>(entity, control);
        world.addComponent<AircraftControlBindingsComponent>(entity, bindings);
        
        // ❌ REMOVED: SpriteScalingDataComponent (scaling via View)
        // ❌ REMOVED: ScalingBehaviorComponent (scaling via View)
        // ❌ REMOVED: LockBehaviorComponent (world coordinates)
    }
    
    mHasRun = true;
}
```

---

## 🎯 PHASE 1: VIEW ARCHITECTURE & RENDER PASSES

### **1.1 Letterbox Viewport Calculation** (core utility)

```cpp
// core/ui/viewport_utils.h
namespace core::ui {

/**
 * @brief Compute letterbox/pillarbox viewport for fixed logical size.
 * 
 * Maintains aspect ratio of logicalSize within windowSize.
 * Returns viewport rect in normalized coordinates [0, 1].
 * 
 * @param windowSize Current window size in pixels
 * @param logicalSize Fixed logical size (world units)
 * @return Viewport rect for sf::View::setViewport()
 * 
 * @note Fallback {0,0,1,1} does NOT preserve aspect ratio.
 *       Caller must ensure window size validity before relying on result.
 */
[[nodiscard]] inline sf::FloatRect computeLetterboxViewport(
    const sf::Vector2u& windowSize,
    const sf::Vector2f& logicalSize) noexcept {
    
    // NOTE: Fallback does NOT preserve aspect ratio.
    // This is a defensive measure for invalid input, not normal operation.
    if (windowSize.x == 0 || windowSize.y == 0 ||
        logicalSize.x <= 0.f || logicalSize.y <= 0.f) {
        return {0.f, 0.f, 1.f, 1.f};  // fallback: full window (no letterbox)
    }
    
    const float windowAspect = static_cast<float>(windowSize.x) / 
                               static_cast<float>(windowSize.y);
    const float logicalAspect = logicalSize.x / logicalSize.y;
    
    float viewportWidth = 1.f;
    float viewportHeight = 1.f;
    float viewportX = 0.f;
    float viewportY = 0.f;
    
    if (windowAspect > logicalAspect) {
        // Window wider than logical → pillarbox (black bars on sides)
        viewportWidth = logicalAspect / windowAspect;
        viewportX = (1.f - viewportWidth) * 0.5f;
    } else {
        // Window taller than logical → letterbox (black bars top/bottom)
        viewportHeight = windowAspect / logicalAspect;
        viewportY = (1.f - viewportHeight) * 0.5f;
    }
    
    return {viewportX, viewportY, viewportWidth, viewportHeight};
}

} // namespace core::ui
```

**Note:** Это core utility, game-agnostic. Любая игра может использовать letterbox.

---

### **1.2 ViewManager** (game layer, manages both views)

```cpp
// game/skyguard/presentation/view_manager.h
namespace game::skyguard::presentation {

/**
 * @brief Manages WorldView and UiView for SkyGuard.
 * 
 * Responsibilities:
 * - Initialize views from config
 * - Handle resize (update viewport, not size!)
 * - Provide views for render passes
 * 
 * NOT an ECS system. Owned by Game.
 */
class ViewManager {
public:
    /**
     * @brief Initialize views from configuration.
     * 
     * @param config View configuration
     * @param initialWindowSize Window size at startup
     */
    void init(const config::ViewConfig& config, 
              const sf::Vector2u& initialWindowSize) noexcept;
    
    /**
     * @brief Handle window resize.
     * 
     * Updates viewport (letterbox) for BOTH views.
     * Neither view size changes — only viewport.
     * 
     * @param newWindowSize New window size in pixels
     */
    void onResize(const sf::Vector2u& newWindowSize) noexcept;
    
    /**
     * @brief Update camera center (call after player movement).
     * 
     * @param targetPosition Player position in world space
     */
    void updateCamera(const sf::Vector2f& targetPosition) noexcept;
    
    // Accessors
    [[nodiscard]] const sf::View& getWorldView() const noexcept { return mWorldView; }
    [[nodiscard]] const sf::View& getUiView() const noexcept { return mUiView; }
    [[nodiscard]] sf::Vector2f getWorldLogicalSize() const noexcept { return mWorldLogicalSize; }
    [[nodiscard]] sf::Vector2f getUiLogicalSize() const noexcept { return mUiLogicalSize; }
    
private:
    sf::View mWorldView;
    sf::View mUiView;
    
    sf::Vector2f mWorldLogicalSize{1024.f, 768.f};
    sf::Vector2f mUiLogicalSize{1024.f, 768.f};
    sf::Vector2f mCameraOffset{0.f, -100.f};
    float mCameraMinY{384.f};
    
    sf::Vector2u mCurrentWindowSize{1024, 768};
};

} // namespace game::skyguard::presentation
```

**Implementation:**

```cpp
// game/skyguard/presentation/view_manager.cpp
namespace game::skyguard::presentation {

void ViewManager::init(const config::ViewConfig& config,
                       const sf::Vector2u& initialWindowSize) noexcept {
    mWorldLogicalSize = config.worldLogicalSize;
    mUiLogicalSize = config.uiLogicalSize;
    mCameraOffset = config.cameraOffset;
    mCameraMinY = config.cameraMinY;
    mCurrentWindowSize = initialWindowSize;
    
    // ============================================================
    // WORLD VIEW: Fixed logical size, letterbox viewport
    // ============================================================
    mWorldView.setSize(mWorldLogicalSize);
    mWorldView.setCenter(mWorldLogicalSize * 0.5f);  // initial: center of world
    
    const sf::FloatRect worldViewport = core::ui::computeLetterboxViewport(
        initialWindowSize, mWorldLogicalSize);
    mWorldView.setViewport(worldViewport);
    
    // ============================================================
    // UI VIEW: Fixed logical size, FULL WINDOW viewport
    // ============================================================
    // UI covers entire window (including letterbox black bars)
    // This allows HUD to render OVER the black bars, not inside them
    mUiView.setSize(mUiLogicalSize);
    mUiView.setCenter(mUiLogicalSize * 0.5f);
    mUiView.setViewport({0.f, 0.f, 1.f, 1.f});  // ALWAYS full window
}

void ViewManager::onResize(const sf::Vector2u& newWindowSize) noexcept {
    if (newWindowSize.x == 0 || newWindowSize.y == 0) {
        return;  // minimized window
    }
    
    mCurrentWindowSize = newWindowSize;
    
    // ============================================================
    // WORLD VIEW: Update viewport (letterbox)
    // ============================================================
    const sf::FloatRect worldViewport = core::ui::computeLetterboxViewport(
        newWindowSize, mWorldLogicalSize);
    mWorldView.setViewport(worldViewport);
    
    // ============================================================
    // UI VIEW: Viewport stays {0,0,1,1} — always full window
    // ============================================================
    // No viewport update needed — UI always covers full window
    // Size stays fixed (uiLogicalSize) — uniform scaling via View
    
    // NOTE: ScalingSystem and LockSystem NOT registered in SkyGuard
    // All scaling happens through View, not component manipulation
}

void ViewManager::updateCamera(const sf::Vector2f& targetPosition) noexcept {
    // Camera follows player with offset
    sf::Vector2f desiredCenter = targetPosition + mCameraOffset;
    
    // Clamp Y to prevent showing below world origin
    desiredCenter.y = std::max(desiredCenter.y, mCameraMinY);
    
    // SkyGuard-specific: horizontal camera locked (vertical scroller)
    // Other games may allow horizontal camera movement
    desiredCenter.x = mWorldLogicalSize.x * 0.5f;
    
    mWorldView.setCenter(desiredCenter);
}

} // namespace game::skyguard::presentation
```

---

### **1.3 Game Class Integration** (render passes)

```cpp
// game/skyguard/game.h — additions
namespace game::skyguard {

class Game {
private:
    // ... existing members ...
    
    // ✅ NEW: View management
    presentation::ViewManager mViewManager;
    
    // ✅ NEW: Render passes (explicit order)
    void renderWorldPass();
    void renderUiPass();
};

} // namespace game::skyguard
```

```cpp
// game/skyguard/game.cpp — render implementation

void Game::render() {
    mWindow.clear(sf::Color::Black);  // Black letterbox bars
    
    // ============================================================
    // PASS 1: WORLD (gameplay entities)
    // ============================================================
    renderWorldPass();
    
    // ============================================================
    // PASS 2: UI (HUD, debug overlay)
    // ============================================================
    renderUiPass();
    
    mWindow.display();
}

void Game::renderWorldPass() {
    mWindow.setView(mViewManager.getWorldView());
    
    // RenderSystem draws all world entities
    mRenderSystem->render(mWorld, mWindow);
    
    // Future: background, particles, effects (all in world space)
}

void Game::renderUiPass() {
    mWindow.setView(mViewManager.getUiView());
    
    // Debug overlay (screen space)
    if (mDebugOverlay->isEnabled()) {
        mDebugOverlay->draw(mWindow);  // NEW: separate draw method
    }
    
    // Future: HUD, menus, etc.
}
```

---

### **1.4 Resize Handling** (simplified — no ScalingSystem/LockSystem)

```cpp
// game/skyguard/game.cpp — processEvents()

else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
    const sf::Vector2u newSize{resized->size.x, resized->size.y};
    
    // Ignore invalid size (minimized window)
    if (newSize.x == 0 || newSize.y == 0) {
        continue;
    }
    
    // ✅ Update view manager (viewport only for both views)
    mViewManager.onResize(newSize);
    
    // NOTE: ScalingSystem/LockSystem NOT registered in SkyGuard
    // (no onResize calls needed — scaling via View)
}
```

---

### **1.5 Camera Update** (in game loop)

```cpp
// game/skyguard/game.cpp — update()

void Game::update(const sf::Time& dt) {
    const float dtSeconds = dt.asSeconds();
    
    // 1. ECS systems update (movement, etc.)
    mWorld.update(dtSeconds);
    
    // 2. Find local player and update camera
    updateCamera();
}

void Game::updateCamera() {
    auto view = mWorld.view<
        game::skyguard::ecs::LocalPlayerTagComponent,
        core::ecs::TransformComponent
    >();
    
    for (auto [entity, localTag, transform] : view.each()) {
        (void)entity;
        (void)localTag;
        mViewManager.updateCamera(transform.position);
        break;  // Only one local player
    }
}
```

---

## 🎯 PHASE 2: DEBUG OVERLAY FIX

### **2.1 DebugOverlaySystem Refactoring**

**Problem:** Current implementation calls `window.draw()` directly, mixing responsibilities.

**Solution:** Separate data preparation from rendering.

```cpp
// core/ecs/systems/debug_overlay_system.h — MODIFIED

namespace core::ecs {

class DebugOverlaySystem final : public ISystem {
public:
    // ... existing bind(), setEnabled(), etc. ...
    
    void update(World& world, float dt) override;
    
    // ❌ REMOVED: void render(World&, sf::RenderWindow&) override;
    
    /**
     * @brief Prepare overlay text for current frame.
     * 
     * Called once per frame from Game layer.
     * Does NOT draw — only updates internal sf::Text.
     */
    void prepareFrame(World& world);
    
    /**
     * @brief Draw overlay to window.
     * 
     * Called by Game layer in UI render pass.
     * Caller is responsible for setting correct view.
     */
    void draw(sf::RenderWindow& window) const;
    
private:
    // ... existing members ...
    bool mNeedsTextUpdate{true};
};

} // namespace core::ecs
```

**Implementation:**

```cpp
// core/ecs/systems/debug_overlay_system.cpp — MODIFIED

void DebugOverlaySystem::prepareFrame(World& world) {
    if (!mEnabled || !mFpsText) {
        return;
    }
    
    // Throttle text updates
    const sf::Time dt = mRenderClock.restart();
    mAccumulatedTime += dt;
    
    if (mUpdateInterval != sf::Time::Zero && mAccumulatedTime < mUpdateInterval) {
        mNeedsTextUpdate = false;
        return;
    }
    
    mAccumulatedTime = sf::Time::Zero;
    mNeedsTextUpdate = true;
    
    // Build text content (existing logic)
    mTextBuffer.clear();
    // ... FPS, entity count, render stats ...
    
    mFpsText->setString(mTextBuffer);
}

void DebugOverlaySystem::draw(sf::RenderWindow& window) const {
    if (!mEnabled || !mFpsText) {
        return;
    }
    
    window.draw(*mFpsText);
}

// render() removed — Game layer controls when/where to draw
// 
// NOTE: DebugOverlaySystem is NOT a simulation system.
// It's a presentation-only system that inherits ISystem for convenience.
// update() intentionally does nothing — all work happens in prepareFrame()/draw().
// This is acceptable because:
// - The system doesn't participate in game logic
// - It only prepares and renders debug text
// - Future refactor could move it out of ISystem hierarchy entirely
void DebugOverlaySystem::update(World&, float) {
    // Intentionally empty — this is a presentation-only system
}
```

---

### **2.2 Debug Overlay Position** (UI with anchoring)

**OPTION A: Simple position (current implementation)**
```json
{
  "enabled": true,
  "position": [20, 20],
  "characterSize": 20,
  "color": { "r": 255, "g": 255, "b": 0, "a": 255 }
}
```
Position `[20, 20]` = 20 logical units from top-left. Works but doesn't express intent.

**OPTION B: Anchor-based (implement THIS instead of option A)**
```json
{
  "enabled": true,
  "anchor": "TopLeft",
  "offset": [20, 20],
  "characterSize": 20,
  "color": { "r": 255, "g": 255, "b": 0, "a": 255 }
}
```
This clearly documents layout intent and supports future UI elements:
- HealthBar → `anchor: "TopLeft"`, `offset: [20, 20]`
- Score → `anchor: "TopCenter"`, `offset: [0, 20]`
- Pause button → `anchor: "TopRight"`, `offset: [-20, 20]`
- Lives → `anchor: "BottomLeft"`, `offset: [20, -20]`

**Implementation with LockSystem:**
```cpp
// UI entity for debug overlay
TransformComponent transform{};
transform.position = {0.f, 0.f};  // base position (ignored after lock)

LockBehaviorComponent lock{};
lock.kind = LockBehaviorKind::Screen;  // anchored to screen
lock.anchor = Anchor::TopLeft;
lock.offset = {20.f, 20.f};

world.addComponent<TransformComponent>(overlayEntity, transform);
world.addComponent<LockBehaviorComponent>(overlayEntity, lock);
```

**LockSystem usage contract for UI:**
```
✅ Used ONLY for UI entities in UiView
✅ UiView is static (center doesn't move) → Screen lock works correctly
✅ Transform.position for UI entities = UI logical space
✅ World entities MUST NOT have LockBehaviorComponent
```

**DECISION for Phase 2:**
- Start with Option A (simple position) — minimal change
- Migrate to Option B when HUD is added (Phase 5+)
- LockSystem for UI is explicitly ALLOWED per contract

---

## 🎯 PHASE 3: SKYGUARD-SPECIFIC CLEANUP (NOT core deletion!)

### **⚠️ CRITICAL ARCHITECTURE DECISION**

```
Core systems (ScalingSystem, LockSystem) are NOT deleted.
They remain in core for potential use by:
- Future games (4X strategy with pixel UI)
- Level editors
- UI tools
- Legacy compatibility
- Different game layers

SkyGuard simply DOES NOT REGISTER these systems.
This preserves "Core remains game-agnostic" contract.
```

---

### **3.1 ScalingSystem — NOT USED BY SKYGUARD**

**Status:** Remains in `core/`, marked as optional/legacy for SkyGuard.

**Why SkyGuard doesn't use it:**
- World entities: scale via WorldView letterbox (not component.scale modification)
- UI entities: scale via UiView (full window viewport, fixed logical size)
- No runtime scale changes needed — all scaling via View

**SkyGuard Game.cpp — DO NOT REGISTER:**
```cpp
// ❌ REMOVED from SkyGuard (but file stays in core):
// mScalingSystem = &mWorld.addSystem<core::ecs::ScalingSystem>();

// ❌ REMOVED from processEvents():
// mScalingSystem->onResize(mWorld, newView);
```

**Core files — KEEP:**
```
KEEP: include/core/ecs/systems/scaling_system.h
KEEP: include/core/ecs/components/scaling_behavior_component.h
KEEP: include/core/ecs/components/sprite_scaling_data_component.h
KEEP: include/core/ui/scaling_behavior.h
```

---

### **3.2 LockSystem — SPLIT DECISION**

**For WORLD entities:** Not used (world coordinates, no screen lock).

**For UI entities:** Anchoring mechanism NEEDED for future UI.

**Problem:**
```
Current DebugOverlay: position [20, 20] — hardcoded TopLeft
Future UI needs:
- HealthBar → TopLeft
- Score → TopCenter  
- Pause button → TopRight
- Lives → BottomLeft

Without anchors = hardcoded positions everywhere.
```

**Solution — Two options:**

**Option A: Keep LockSystem for UI only (minimal change)**
```cpp
// LockSystem works in UiView (static, topLeft ≈ 0,0)
// Only UI entities have LockBehaviorComponent
// World entities: no LockBehaviorComponent
```

**Option B: New lightweight UiAnchorSystem (cleaner, future)**
```cpp
// game/skyguard/ui/ui_anchor_system.h
enum class UiAnchor {
    TopLeft, TopCenter, TopRight,
    CenterLeft, Center, CenterRight,
    BottomLeft, BottomCenter, BottomRight
};

struct UiAnchorComponent {
    UiAnchor anchor{UiAnchor::TopLeft};
    sf::Vector2f offset{0.f, 0.f};  // offset from anchor point
    sf::Vector2f margin{0.f, 0.f};  // safe area margin
};
```

**DECISION for Phase 3:** Use Option A (reuse LockSystem for UI).
Option B can be implemented later if needed.

**SkyGuard Game.cpp — conditional registration:**
```cpp
// World entities: NO lock system involvement
// UI entities: LockSystem optional (if UI anchoring needed)

// For now (debug overlay only): no LockSystem registration
// Future (HUD, menus): register LockSystem for UI entities only
```

**Core files — KEEP:**
```
KEEP: include/core/ecs/systems/lock_system.h
KEEP: include/core/ecs/components/lock_behavior_component.h
KEEP: include/core/ui/lock_behavior.h
```

---

### **3.3 player.json — simplification**

**BEFORE:**
```json
{
  "resize_scaling": "Uniform",
  "lock_behavior": "ScreenLock",
  "anchor": "BottomCenter",
  ...
}
```

**AFTER:**
```json
{
  "start_position": { "x": 512, "y": 700 },
  ...
}
```

**Remove fields from player config:**
- `resize_scaling` — not used (View scaling)
- `lock_behavior` — not used (world coordinates)
- `anchor` — replaced by explicit `start_position`

---

### **3.4 PlayerInitSystem — remove unused components**

```cpp
// REMOVE these lines from PlayerInitSystem::update():

// ❌ No longer created for player (world entity):
// world.addComponent<SpriteScalingDataComponent>(entity, scalingDataComp);
// world.addComponent<ScalingBehaviorComponent>(entity, scalingComp);
// world.addComponent<LockBehaviorComponent>(entity, lockComp);
```

**Note:** These components EXIST in core, just not used for SkyGuard world entities.

---

### **3.5 Config loaders — remove unused parsing**

**player_blueprint.h:**
```cpp
// REMOVE from PlayerBlueprint struct (SkyGuard-specific):
// core::ui::ScalingBehaviorKind scalingBehavior;  // not used
// core::ui::LockBehaviorKind lockBehavior;        // not used
```

**config_loader.cpp:**
```cpp
// REMOVE parsing (SkyGuard player config):
// - "resize_scaling"
// - "lock_behavior"
// - "anchor"
```

---

### **3.6 Summary: What Changes vs What Stays**

```
┌─────────────────────────────────────────────────────────────────┐
│                    CORE (game-agnostic)                         │
├─────────────────────────────────────────────────────────────────┤
│ STAYS (available for other games):                              │
│ ├── scaling_system.h                                           │
│ ├── scaling_behavior_component.h                               │
│ ├── sprite_scaling_data_component.h                            │
│ ├── scaling_behavior.h                                         │
│ ├── lock_system.h                                              │
│ ├── lock_behavior_component.h                                  │
│ └── lock_behavior.h                                            │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                  SKYGUARD (game layer)                          │
├─────────────────────────────────────────────────────────────────┤
│ MODIFIED:                                                       │
│ ├── game.cpp (don't register ScalingSystem/LockSystem)         │
│ ├── player_init_system.h (don't create scaling/lock components)│
│ ├── player_blueprint.h (remove scaling/lock fields)            │
│ ├── config_loader.cpp (remove scaling/lock parsing)            │
│ └── player.json (remove scaling/lock/anchor fields)            │
└─────────────────────────────────────────────────────────────────┘
```

---

### **3.7 Future UI Anchoring Contract**

```
For SkyGuard UI (HUD, menus, etc.):

Option 1: Use existing LockSystem with UiView
- Register LockSystem
- Add LockBehaviorComponent to UI entities only
- Works because UiView is static (topLeft ≈ 0,0 in logical space)

Option 2: Implement UiAnchorSystem (Phase 5+)
- Cleaner API for UI-specific needs
- Supports margins, safe areas, responsive layout
- Independent of core LockSystem

Current Phase 3: No UI anchoring (only DebugOverlay with hardcoded position)
Future phases: Implement as needed when HUD is added
```

---

## 🎯 PHASE 4+: FUTURE PHASES (CONTRACTS ONLY)

### **4.1 Background System** (Phase 4)

**Contract** (implementation later):

```cpp
// game/skyguard/presentation/background_renderer.h

/**
 * @brief Renders scrolling background for SkyGuard levels.
 * 
 * NOT an ECS system — presentation layer only.
 * NOT in core — game-specific.
 * 
 * Ownership: Game or LevelState
 * 
 * Render order: BEFORE world entities (lowest z-order)
 */
class BackgroundRenderer {
public:
    void init(const LevelConfig& level, core::resources::ResourceManager& resources);
    void update(const sf::View& worldView);  // parallax offset
    void draw(sf::RenderWindow& window) const;
};
```

**Key points:**
- NOT in core (game-specific)
- NOT an ECS entity (presentation only)
- Rendered in World View (world space)
- Uses camera position for parallax

---

### **4.2 Escorts & Formations** (Phase 5)

**Contract** (implementation later):

```cpp
// Escorts follow player in world space
// Formation positions = player.position + offset
// ownerId: Entity reference (validated every frame)
// Max 3 escorts per player (design limit)
```

---

## 📂 FILE STRUCTURE

### **НОВЫЕ ФАЙЛЫ:**
```
include/game/skyguard/
    config/
        view_config.h              ← NEW (Phase 0)
    
    ecs/components/
        player_tag_component.h     ← NEW (Phase 0)
    
    presentation/                  ← NEW folder
        view_manager.h             ← NEW (Phase 1)

src/game/skyguard/
    presentation/
        view_manager.cpp           ← NEW (Phase 1)

include/core/ui/
    viewport_utils.h               ← NEW (Phase 1, letterbox)
```

### **МОДИФИЦИРУЕМЫЕ ФАЙЛЫ:**
```
src/game/skyguard/
    game.cpp                       ← MODIFIED (render passes, remove systems)

src/core/ecs/systems/
    debug_overlay_system.cpp       ← MODIFIED (Phase 2)

assets/game/skyguard/config/
    player.json                    ← MODIFIED (remove scaling/lock fields)
    skyguard_game.json             ← MODIFIED (add view config section)
```

### **CORE FILES (unchanged — available for other games):**
```
include/core/ecs/systems/
    scaling_system.h               ← KEPT (not used by SkyGuard)
    lock_system.h                  ← KEPT (may be used for UI anchoring)

include/core/ecs/components/
    scaling_behavior_component.h   ← KEPT
    lock_behavior_component.h      ← KEPT
    sprite_scaling_data_component.h ← KEPT

include/core/ui/
    scaling_behavior.h             ← KEPT
    lock_behavior.h                ← KEPT
```

---

## 📋 SYSTEM UPDATE ORDER (canonical)

```cpp
// Game::update()
void Game::update(const sf::Time& dt) {
    const float dtSec = dt.asSeconds();
    
    // ============================================================
    // SIMULATION (deterministic, world space)
    // ============================================================
    
    // 1. Input → Velocity
    mAircraftControlSystem->update(mWorld, dtSec);
    
    // 2. Movement (position += velocity * dt)
    mMovementSystem->update(mWorld, dtSec);
    
    // 3. Spatial index (for culling)
    mSpatialIndexSystem->update(mWorld, dtSec);
    
    // Future: collision, AI, spawning...
    
    // ============================================================
    // PRESENTATION (local, not serialized)
    // ============================================================
    
    // 4. Camera follows player
    updateCamera();  // calls mViewManager.updateCamera(playerPos)
    
    // 5. Debug overlay prepares text
    mDebugOverlay->prepareFrame(mWorld);
    
    // NOTE: ScalingSystem/LockSystem NOT registered in SkyGuard
    // (scaling via View, world coordinates for entities)
}

// Game::render()
void Game::render() {
    mWindow.clear(sf::Color::Black);  // Black letterbox bars
    
    // PASS 1: World (letterbox viewport)
    mWindow.setView(mViewManager.getWorldView());
    // Future: mBackgroundRenderer->draw(mWindow);
    mRenderSystem->render(mWorld, mWindow);
    
    // PASS 2: UI (letterbox viewport, same logical approach)
    mWindow.setView(mViewManager.getUiView());
    mDebugOverlay->draw(mWindow);
    
    mWindow.display();
}

// Game::processEvents() — resize
else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
    const sf::Vector2u newSize{resized->size.x, resized->size.y};
    if (newSize.x > 0 && newSize.y > 0) {
        mViewManager.onResize(newSize);
        // ❌ NO ScalingSystem/LockSystem calls!
    }
}
```

---

## 🔒 CRITICAL CONTRACTS SUMMARY

### **Contract 1: Fixed Logical Size for BOTH Views**
```
✅ WorldView.size = config.worldLogicalSize (NEVER changes)
✅ UiView.size = config.uiLogicalSize (NEVER changes)
✅ WorldView.viewport = letterbox (black bars preserve aspect ratio)
✅ UiView.viewport = {0,0,1,1} ALWAYS (full window, covers black bars)
✅ UI scales uniformly (accessibility: larger on big screens)

CRITICAL DISTINCTION:
- WorldView uses letterbox viewport → gameplay with black bars
- UiView ALWAYS covers full window → HUD renders OVER black bars

WorldView and UiView MAY have different logical sizes.
Example configurations:
- World: 1024×768, UI: 1024×768 (same — current SkyGuard)
- World: 1024×768, UI: 1920×1080 (different — high-res UI)
```

### **Contract 2: World/UI View Separation**
```
✅ WorldView: gameplay, camera follows player, letterbox viewport
✅ UiView: HUD, debug overlay, menus, FULL WINDOW viewport
✅ Different render passes (world first, UI second — on top)
✅ Never mix coordinate spaces
✅ UI renders OVER letterbox black bars (standard AAA approach)
```

### **Contract 3: Transform = World Space**
```
✅ Transform.position ALWAYS in world coordinates
✅ Never store screen coordinates in Transform
✅ Camera is a lens, not a coordinate system
```

### **Contract 4: SkyGuard Scaling/Locking Policy**
```
Core systems REMAIN available (game-agnostic):
✅ ScalingSystem — exists in core, available for other games
✅ LockSystem — exists in core, available for other games/UI

SkyGuard world entities:
❌ No ScalingSystem registration
❌ No LockSystem registration for world
❌ No ScalingBehaviorComponent on players/enemies
❌ No LockBehaviorComponent on players/enemies
✅ All scaling happens through View.viewport

SkyGuard UI entities:
✅ LockSystem MAY be used for UI anchoring (TopLeft, BottomRight, etc.)
✅ Works because UiView is static (center doesn't move)
✅ Transform.position for UI entities = UI logical space
⚠️ Alternative: lightweight UiAnchorSystem (Phase 5+ if needed)
```

### **Contract 4b: LockSystem for UI (explicit)**
```
LockSystem usage for UI is ALLOWED under these conditions:

✅ Used ONLY for UI entities rendered in UiView
✅ UiView.center is STATIC (never moves with camera)
✅ UiView.viewport = {0,0,1,1} (full window)
✅ UI logical space is stable → Screen lock works correctly

❌ NEVER use LockSystem for world entities (camera moves!)
❌ NEVER mix world and UI entities in same lock calculation
```

### **Contract 5: Core Remains Game-Agnostic**
```
✅ No skyguard types in core
✅ Background is game layer responsibility
✅ Camera is game layer (ViewManager)
✅ Letterbox utility is generic (core::ui)
✅ ScalingSystem/LockSystem remain in core (not deleted!)
```

### **Contract 6: Debug Overlay in UI Logical Space**
```
✅ DebugOverlaySystem::draw() called in UI pass
✅ Position in UI logical units (not pixels!)
✅ Does not move with camera
✅ Scales with window (uniform via View)
```

### **Contract 7: Maximum 2 Players**
```
✅ PlayerTagComponent.playerIndex: 0 or 1
✅ LocalPlayerTagComponent: exactly 1 per viewport
✅ Debug assertions enforce limits
```

### **Contract 8: Texture Lifetime**
```
✅ const sf::Texture* — owned by ResourceManager
✅ Valid as long as ResourceManager alive
✅ Game destructor order guarantees this
✅ NEVER delete through cached pointer
```

---

## ✅ IMPLEMENTATION CHECKLIST

### **Phase 0: Configuration & Foundation** (1-2 hours)
- [ ] Create `ViewConfig` struct with `worldLogicalSize` and `uiLogicalSize`
- [ ] Extend `skyguard_game.json` with view section
- [ ] Create `PlayerTagComponent` and `LocalPlayerTagComponent`
- [ ] Update `PlayerInitSystem` to add tags
- [ ] Update `player.json`: remove `resize_scaling`, `lock_behavior`, `anchor`
- [ ] Update `player.json`: add explicit `start_position` in world coordinates
- [ ] Test: player spawns with correct tags at correct world position

### **Phase 1: View Architecture** (2-3 hours)
- [ ] Create `core::ui::computeLetterboxViewport()`
- [ ] Create `ViewManager` class with both views using letterbox
- [ ] Initialize ViewManager in `Game::Game()`
- [ ] Update `Game::render()` with two passes
- [ ] Update resize handling (ViewManager only, no ScalingSystem/LockSystem)
- [ ] Test: both views letterbox correctly, camera follows player, overlay stays fixed

### **Phase 2: Debug Overlay Fix** (1 hour)
- [ ] Refactor `DebugOverlaySystem`: add `prepareFrame()` and `draw()`
- [ ] Remove `render()` override
- [ ] Update `Game::render()` to call `draw()` in UI pass
- [ ] Update debug_overlay.json positions (now in UI logical units)
- [ ] Test: overlay stays in top-left corner, scales with window

### **Phase 3: SkyGuard-Specific Cleanup** (30 min - 1 hour)
- [ ] Remove `ScalingSystem` registration from `game.cpp`
- [ ] Remove `LockSystem` registration from `game.cpp`
- [ ] Remove `onResize()` calls to ScalingSystem/LockSystem
- [ ] Remove `SpriteScalingDataComponent` creation from `PlayerInitSystem`
- [ ] Remove `ScalingBehaviorComponent` creation from `PlayerInitSystem`
- [ ] Remove `LockBehaviorComponent` creation from `PlayerInitSystem`
- [ ] Remove `scalingBehavior`/`lockBehavior` fields from `PlayerBlueprint`
- [ ] Remove parsing of `resize_scaling`/`lock_behavior`/`anchor` from config loader
- [ ] Clean up unused includes in modified files
- [ ] Test: everything compiles and runs correctly
- [ ] NOTE: Core files remain — NOT deleted!

---

## 📝 РЕШЁННЫЕ ПРОБЛЕМЫ

---

## ⚠️ ERROR HANDLING POLICY (проблема #14)

### **ViewConfig Loading**

```cpp
// game/skyguard/config/loader/view_config_loader.h

/**
 * @brief Load ViewConfig from JSON.
 * 
 * FAILURE POLICY (non-critical config):
 * - File not found → WARN + return defaults
 * - JSON parse error → WARN + return defaults  
 * - Invalid values → WARN per field + clamp/default
 * 
 * NEVER throws. Game continues with safe defaults.
 */
[[nodiscard]] ViewConfig loadViewConfig(const std::string& path);
```

### **Failure Scenarios:**

| Scenario | Action | Log Level |
|----------|--------|-----------|
| JSON file missing | Use defaults | WARN |
| JSON parse error | Use defaults | WARN |
| `worldLogicalSize <= 0` | Clamp to {1024, 768} | WARN |
| `cameraOffset` invalid | Use {0, -100} | WARN |
| Texture load failure | Existing fallback (ResourceManager) | ERROR |
| Player entity not found | Camera stays at initial position | DEBUG |

### **Runtime Invariants:**

```cpp
// ViewManager::init() — validate-on-write
void ViewManager::init(const ViewConfig& config, sf::Vector2u windowSize) noexcept {
    // Validate worldLogicalSize (critical for gameplay)
    if (!(config.worldLogicalSize.x > 0.f) || !(config.worldLogicalSize.y > 0.f)) {
        LOG_WARN(core::log::cat::Config,
                 "ViewManager: invalid worldLogicalSize, using fallback 1024x768");
        mWorldLogicalSize = {1024.f, 768.f};
    } else {
        mWorldLogicalSize = config.worldLogicalSize;
    }
    
    // Window size validated in onResize() (OS data)
    // Camera offset: any value is valid (including negative)
    mCameraOffset = config.cameraOffset;
    
    // ... rest of init ...
}
```

### **Player Not Found Handling:**

```cpp
void Game::updateCamera() {
    auto view = mWorld.view<LocalPlayerTagComponent, TransformComponent>();
    
    bool foundPlayer = false;
    for (auto [entity, localTag, transform] : view.each()) {
        (void)entity;
        (void)localTag;
        mViewManager.updateCamera(transform.position);
        foundPlayer = true;
        break;
    }
    
    // No local player — camera stays at current position
    // This is valid during:
    // - Level transition
    // - Player death (before respawn)
    // - Cutscenes
    if (!foundPlayer) {
        // Camera maintains last position (no-op)
        // Debug log only (not every frame)
        #if !defined(NDEBUG)
        static bool warnedOnce = false;
        if (!warnedOnce) {
            LOG_DEBUG(core::log::cat::Gameplay, 
                      "updateCamera: no LocalPlayerTagComponent found");
            warnedOnce = true;
        }
        #endif
    }
}
```

---

## 🔗 LIFETIME DEPENDENCIES (проблема #15)

### **Ownership Diagram:**

```
┌─────────────────────────────────────────────────────────────────┐
│                           Game                                  │
│  (owns everything, destructor order matters)                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐    ┌─────────────────┐                    │
│  │ ResourceManager │    │    ViewManager   │                    │
│  │  (owns textures,│    │  (owns sf::View) │                    │
│  │   fonts, sounds)│    │                  │                    │
│  └────────┬────────┘    └──────────────────┘                    │
│           │                                                     │
│           │ getTexture() returns const ref                      │
│           ▼                                                     │
│  ┌─────────────────┐                                           │
│  │      World      │                                           │
│  │  (owns systems, │                                           │
│  │   ECS registry) │                                           │
│  └────────┬────────┘                                           │
│           │                                                     │
│           │ systems hold raw pointers (non-owning)              │
│           ▼                                                     │
│  ┌─────────────────┐    ┌─────────────────┐                    │
│  │  RenderSystem   │───►│ SpatialIndex*   │ (non-owning)       │
│  │                 │───►│ ResourceManager*│ (non-owning)       │
│  └─────────────────┘    └─────────────────┘                    │
│                                                                 │
│  ┌─────────────────┐    ┌─────────────────┐                    │
│  │DebugOverlay     │───►│ TimeService*    │ (non-owning)       │
│  │     System      │───►│ sf::Font*       │ (non-owning,       │
│  └─────────────────┘    └─────────────────┘  owned by RM)      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### **Destruction Order (Game destructor):**

```cpp
// Implicit destruction order (reverse of declaration):
// 1. mDebugOverlay (system) — releases pointers
// 2. mWorld (ECS registry) — destroys entities & components
// 3. mViewManager — releases sf::View
// 4. mResources (ResourceManager) — releases textures/fonts
// 5. mWindow — closes window

// INVARIANT: Systems are destroyed BEFORE ResourceManager
// Therefore: systems' raw pointers are valid during their lifetime
```

### **Safe Patterns:**

```cpp
// ✅ SAFE: Raw pointer to long-lived owner
class RenderSystem {
    const SpatialIndex* mSpatialIndex{nullptr};  // owned by SpatialIndexSystem
    const ResourceManager* mResources{nullptr};   // owned by Game
    
    void bind(const SpatialIndex* idx, const ResourceManager* res) {
        mSpatialIndex = idx;
        mResources = res;
    }
};

// ✅ SAFE: Reference to long-lived owner
class DebugOverlaySystem {
    TimeService* mTime{nullptr};  // owned by Game
    
    void bind(TimeService& time, const sf::Font& font) {
        mTime = &time;
        // font owned by ResourceManager, outlives this system
    }
};

// ❌ UNSAFE: Storing pointer to temporary
void bad() {
    sf::Texture temp;
    mTexture = &temp;  // dangling after scope
}
```

### **Future Background System (Phase 4):**

```cpp
// game/skyguard/presentation/background_renderer.h

/**
 * @brief Background layer data.
 * 
 * LIFETIME CONTRACT:
 * - cachedTexture points to texture owned by ResourceManager
 * - ResourceManager outlives BackgroundRenderer (Game destruction order)
 * - Pointer valid for entire BackgroundRenderer lifetime
 * - NEVER delete or modify texture through this pointer
 */
struct BackgroundLayer {
    const sf::Texture* cachedTexture{nullptr};  // NON-OWNING, owned by ResourceManager
    float parallaxFactor{1.0f};
    sf::Vector2f scrollSpeed{0.f, 0.f};
    // ... other layer data ...
};

class BackgroundRenderer {
private:
    // ✅ Pointers to textures owned by ResourceManager
    // Valid as long as ResourceManager alive (which outlives BackgroundRenderer)
    std::vector<BackgroundLayer> mLayers;
    
    // ✅ Vertex arrays owned by this class
    std::vector<sf::VertexArray> mLayerVertices;
    
public:
    /**
     * @brief Initialize from level config.
     * 
     * PRECONDITION: resources must outlive this BackgroundRenderer
     * (guaranteed by Game destruction order)
     */
    void init(const LevelConfig& level, ResourceManager& resources) {
        for (const auto& layerCfg : level.backgroundLayers) {
            BackgroundLayer layer{};
            layer.cachedTexture = &resources.getTexture(layerCfg.textureId).get();
            layer.parallaxFactor = layerCfg.parallaxFactor;
            // ...
            mLayers.push_back(layer);
        }
    }
};
```

**Invariant:** `cachedTexture` валиден, пока жив `ResourceManager`

---

## 📐 BACKGROUND MATH & VALIDATION (проблема #18)

### **Problem Statement:**

Background tiling and parallax involves complex math:
- `floor()` with negative numbers
- Tile offset calculations
- Parallax factor multiplication
- Edge cases at world boundaries

### **Mitigation Strategy (Phase 4):**

```cpp
// game/skyguard/presentation/background_math.h

namespace game::skyguard::presentation {

/**
 * @brief Compute tile start position for seamless tiling.
 * 
 * FORMULA:
 *   startY = floor((visibleTop - offset) / tileHeight) * tileHeight + offset
 * 
 * EDGE CASES:
 *   - negative visibleTop: floor() handles correctly
 *   - offset > tileHeight: works due to modular arithmetic
 *   - tileHeight = 0: caller must validate (assert)
 * 
 * @param visibleTop Top of visible area (world units)
 * @param offset Current parallax offset (world units)
 * @param tileHeight Height of one tile (world units, must be > 0)
 * @return Y position to start tiling from
 */
[[nodiscard]] constexpr float computeTileStartY(
    float visibleTop,
    float offset, 
    float tileHeight) noexcept;

/**
 * @brief Compute number of tiles needed to cover visible area.
 * 
 * Always adds +1 to handle partial tiles at edges.
 */
[[nodiscard]] constexpr int computeTileCount(
    float visibleHeight,
    float tileHeight) noexcept;

} // namespace game::skyguard::presentation
```

### **Required Unit Tests (Phase 4):**

```cpp
// tests/background_math_test.cpp

TEST(BackgroundMath, TileStartY_PositiveValues) {
    // visibleTop=100, offset=0, tileHeight=64
    // floor((100-0)/64)*64 + 0 = floor(1.5625)*64 = 1*64 = 64
    EXPECT_FLOAT_EQ(computeTileStartY(100.f, 0.f, 64.f), 64.f);
}

TEST(BackgroundMath, TileStartY_NegativeVisibleTop) {
    // visibleTop=-50, offset=0, tileHeight=64
    // floor((-50-0)/64)*64 + 0 = floor(-0.78)*64 = -1*64 = -64
    EXPECT_FLOAT_EQ(computeTileStartY(-50.f, 0.f, 64.f), -64.f);
}

TEST(BackgroundMath, TileStartY_WithOffset) {
    // visibleTop=100, offset=30, tileHeight=64
    // floor((100-30)/64)*64 + 30 = floor(1.09)*64 + 30 = 64 + 30 = 94
    EXPECT_FLOAT_EQ(computeTileStartY(100.f, 30.f, 64.f), 94.f);
}

TEST(BackgroundMath, TileCount_ExactFit) {
    // visibleHeight=128, tileHeight=64 → 128/64 + 1 = 3
    EXPECT_EQ(computeTileCount(128.f, 64.f), 3);
}

TEST(BackgroundMath, TileCount_PartialTile) {
    // visibleHeight=100, tileHeight=64 → ceil(100/64) + 1 = 2 + 1 = 3
    EXPECT_EQ(computeTileCount(100.f, 64.f), 3);
}
```

### **Visual Debugging (Phase 4):**

```cpp
#if !defined(NDEBUG)
// Draw tile boundaries for debugging
void BackgroundRenderer::debugDrawTileBounds(sf::RenderWindow& window) const {
    for (const auto& tile : mDebugTileBounds) {
        sf::RectangleShape rect(tile.size);
        rect.setPosition(tile.position);
        rect.setFillColor(sf::Color::Transparent);
        rect.setOutlineColor(sf::Color::Magenta);
        rect.setOutlineThickness(1.f);
        window.draw(rect);
    }
}
#endif
```

---

## 🔢 FLOAT PRECISION CONSIDERATIONS (проблема #19)

### **Problem Statement:**

In a vertical scroller, `camera.center.y` grows continuously as player advances.
After extended play sessions:
- `camera.y` could reach 100,000+ world units
- Parallax calculations: `offset = camera.y * 0.001f` lose precision
- Sprite positions relative to camera may jitter

### **Analysis:**

```cpp
// float has ~7 significant decimal digits
// At camera.y = 1,000,000:
//   - representable precision: ~0.1 units
//   - parallax offset = 1,000,000 * 0.001 = 1000.0 (OK)
//   - but: 1,000,000.5 - 1,000,000.0 may not be exactly 0.5

// For SkyGuard with typical session:
//   - 10 minutes at 400 units/sec = 240,000 units
//   - Still within safe float range (~0.03 unit precision)
```

### **Mitigation Strategies (Future):**

#### **Option A: World Origin Reset (recommended for endless modes)**

```cpp
// Every N units, reset world origin
constexpr float kWorldResetThreshold = 100000.f;

void Game::update(float dt) {
    // ... normal update ...
    
    if (mWorldOriginY > kWorldResetThreshold) {
        resetWorldOrigin();
    }
}

void Game::resetWorldOrigin() {
    const float resetAmount = kWorldResetThreshold;
    
    // Move all entities down
    auto view = mWorld.view<TransformComponent>();
    for (auto [entity, transform] : view.each()) {
        transform.position.y -= resetAmount;
    }
    
    // Adjust camera
    mViewManager.adjustOrigin(-resetAmount);
    
    // Adjust background offset
    mBackgroundRenderer.adjustOrigin(-resetAmount);
    
    mWorldOriginY -= resetAmount;
    
    LOG_DEBUG(core::log::cat::Gameplay, "World origin reset by {}", resetAmount);
}
```

#### **Option B: Double precision for camera (overkill for SkyGuard)**

```cpp
// NOT recommended — unnecessary complexity
// double has ~15 significant digits, but SFML uses float internally
```

#### **Option C: Relative coordinates (complex)**

```cpp
// Store positions relative to "chunk" origin
// NOT recommended for SkyGuard — adds complexity for minimal benefit
```

### **Recommendation for SkyGuard:**

```
✅ For typical play sessions (< 1 hour): float is sufficient
✅ Monitor in profiling: add debug display of camera.y
⚠️ For endless/marathon modes: implement Option A (world reset)
❌ Don't over-engineer: premature optimization
```

### **Debug Monitoring (add to DebugOverlay):**

```cpp
#if !defined(NDEBUG)
// In DebugOverlaySystem::prepareFrame()
mTextBuffer.append("\nCam.Y: ");
appendFloat(mTextBuffer, mViewManager->getWorldView().getCenter().y);

// Warn if approaching precision limits
if (cameraY > 500000.f) {
    mTextBuffer.append(" [PRECISION WARNING]");
}
#endif
```

---

## 📋 РЕШЁННЫЕ ПРОБЛЕМЫ (полный список 19/19)

| # | Проблема | Решение | Раздел плана |
|---|----------|---------|--------------|
| 1 | Debug Overlay в world coordinates | UI View + render passes | Phase 1, 2 |
| 2 | Нет World/UI View разделения | ViewManager с двумя view | Phase 1 |
| 3 | anchor_utils topLeft=(0,0) | World coordinates, explicit start_position | Phase 0 |
| 4 | core/game separation | Background в game layer | Phase 4 contract |
| 5 | CameraSystem наследует ISystem | ViewManager (не ISystem) | Phase 1 |
| 6 | Magic numbers | Из конфигурации ViewConfig | Phase 0 |
| 7 | Aspect ratio data-flow | Data Ownership раздел | Data Ownership |
| 8 | ScalingSystem конфликт | **Не регистрируется** в SkyGuard (scaling via View) | Phase 3 |
| 9 | ScreenLock несовместим | **Не регистрируется** для world entities | Phase 3 |
| 10 | 4 игрока | Max 2, assertions | Phase 0 |
| 11 | const_cast UB | Убран | — |
| 12 | Resize handling | ViewManager.onResize() only | Phase 1 |
| 13 | View initialization | ViewManager.init() | Phase 1 |
| 14 | Error handling | Error Handling Policy | Error Handling |
| 15 | Lifetime dependencies (cachedTexture) | Документировано: owned by RM | Lifetime |
| 16 | DebugOverlay window.draw() | prepareFrame() + draw() | Phase 2 |
| 17 | World::render() passes | Explicit render passes | Phase 1 |
| 18 | Background math | Unit tests + debug viz | Background Math |
| 19 | Float precision | Monitoring + reset option | Float Precision |

### **Дополнительно решено:**

| Проблема | Решение |
|----------|---------|
| UI pixel-space (плохое зрение) | UI logical size + letterbox |
| Core game-agnostic | ScalingSystem/LockSystem остаются в core |
| UI anchoring (future) | LockSystem для UI или UiAnchorSystem |

---

**END OF PLAN v4.0**