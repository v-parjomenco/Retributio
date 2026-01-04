# GPU Testing Guide — SkyGuard / Titan 4X

**Your Hardware:**
- Primary: **NVIDIA RTX 4080** (discrete, development)
- Integrated: **AMD Radeon 610M** (Ryzen 9 7945HX built-in)
- Optional: **Intel integrated** (wife's laptop, if available)

---

## 🎯 TESTING MATRIX

| GPU                 | Use Case                      | Texture Arrays | Priority   |
|---------------------|-------------------------------|----------------|------------|
| **RTX 4080**        | Development, primary testing  | ✅ 16K layers  | **HIGH**   |
| **Radeon 610M**     | Integrated GPU testing        | ⚠️ 2K layers   | **MEDIUM** |
| **Intel UHD**       | Worst-case testing (optional) | ⚠️ 2K layers   | **LOW**    |
| **Forced Fallback** | Multi-atlas path validation   | ❌ Disabled    | **HIGH**   |

---

## 🔧 GPU SWITCHING (Linux)

### **Check Available GPUs:**
```bash
# List all GPUs
glxinfo | grep "OpenGL renderer"

# Expected output:
#   OpenGL renderer string: NVIDIA GeForce RTX 4080
#   OpenGL renderer string: AMD Radeon 610M
```

### **Run on RTX 4080 (default):**
```bash
./skyguard
# Uses primary GPU automatically
```

### **Run on Radeon 610M (integrated):**
```bash
DRI_PRIME=1 ./skyguard

# Or set globally:
export DRI_PRIME=1
./skyguard
```

### **Query GPU in runtime:**
```cpp
// Add to debug overlay or startup log
const GLubyte* renderer = glGetString(GL_RENDERER);
LOG_INFO("Active GPU: {}", reinterpret_cast<const char*>(renderer));

GLint maxLayers;
glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
LOG_INFO("Max texture array layers: {}", maxLayers);
```

---

## 🧪 TESTING SCENARIOS

### **1. RTX 4080 Testing (Development)**

**When:** Daily development (Months 1-6)

**Commands:**
```bash
# Normal build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
./skyguard

# Profile build
cmake -DCMAKE_BUILD_TYPE=Profile ..
make -j$(nproc)
./skyguard --stress-test
```

**What to Check:**
- [ ] 240 FPS @ empty scene
- [ ] 240 FPS @ 5K entities (SkyGuard target)
- [ ] 144 FPS @ 50K entities (4X stress test)
- [ ] Texture arrays active (`glGetIntegerv` confirms 16K layers)
- [ ] No memory leaks (Valgrind)
- [ ] No crashes in 2-hour session

---

### **2. Radeon 610M Testing (Integrated GPU)**

**When:** Before each milestone (Months 2, 4, 6, 12, 24)

**Commands:**
```bash
# Switch to integrated GPU
DRI_PRIME=1 ./skyguard

# Profile on integrated
DRI_PRIME=1 ./skyguard --stress-test --log-performance
```

**What to Check:**
- [ ] 60 FPS minimum @ empty scene
- [ ] 60 FPS minimum @ 5K entities (SkyGuard target)
- [ ] 30 FPS acceptable @ 50K entities (4X stress test)
- [ ] Texture arrays supported? (check max layers: 2048 typical)
- [ ] Fallback activates if layers < required
- [ ] No visual artifacts (compare screenshots vs RTX 4080)
- [ ] Memory usage acceptable (<2GB for SkyGuard)

**Expected Performance:**
- SkyGuard (5K entities): **60-120 FPS** ✅
- 4X Stress (50K entities): **30-60 FPS** ⚠️
- If FPS < 60 on SkyGuard → optimize or reduce quality settings

---

### **3. Forced Fallback Testing (Multi-Atlas Path)**

**When:** After implementing texture system (Month 7-8, then every major change)

**Commands:**
```bash
# Build with forced fallback
cmake -DCMAKE_BUILD_TYPE=Release -DSFML1_FORCE_MULTI_ATLAS=ON ..
make -j$(nproc)
./skyguard

# Test on RTX 4080 (validates fallback even on good hardware)
./skyguard --stress-test
```

**What to Check:**
- [ ] Multi-atlas path active (log confirms "Using multi-atlas renderer")
- [ ] Visual correctness (identical to texture array path)
- [ ] Performance acceptable (minor degradation ok, <20% slower)
- [ ] No crashes or memory leaks
- [ ] Atlas switching works (different categories: units, buildings, terrain)
- [ ] 240 FPS @ 5K entities still achievable on RTX 4080
- [ ] 60 FPS @ 5K entities still achievable on Radeon 610M

---

### **4. Intel Integrated Testing (Optional, Wife's Laptop)**

**When:** Before Steam releases (SkyGuard Month 6, Titan 4X Month 33)

**Hardware Examples:**
- Intel UHD 630 (common in office laptops)
- Intel Iris Xe (newer laptops 2020+)
- Intel HD 620 (older laptops)

**Commands (Windows or Linux):**
```bash
# Windows: Set integrated GPU in NVIDIA/AMD control panel
# Linux: Usually default if no discrete GPU

./skyguard
./skyguard --stress-test
```

**What to Check:**
- [ ] Game launches without crash
- [ ] 60 FPS minimum @ empty scene
- [ ] 60 FPS minimum @ 5K entities (may need quality settings: LOW)
- [ ] Texture arrays: check max layers (often 2048, sometimes 512!)
- [ ] Fallback activates gracefully if arrays unsupported
- [ ] No visual artifacts
- [ ] Settings menu works (resolution, quality, fullscreen)
- [ ] 1v1 multiplayer playable (Month 4 SkyGuard)

**Expected Performance:**
- Intel UHD 630: **60 FPS @ LOW settings** (5K entities)
- Intel Iris Xe: **60-90 FPS @ MEDIUM** (5K entities)
- Intel HD 620: **30-60 FPS @ LOW** (may struggle)

**If FPS < 60 on Intel:**
- Add graphics quality presets (LOW/MEDIUM/HIGH/ULTRA)
- LOW preset: reduce particle count, disable shadows, lower resolution
- Target: 60 FPS @ LOW on Intel UHD 630 (minimum spec)

---

## 📋 TESTING CHECKLIST BY PHASE

### **Phase 1 (Weeks 1-4):**
- [x] RTX 4080: Prototype runs @ 240 FPS
- [ ] Implement GPU capability query (max texture array layers)
- [ ] Radeon 610M: Basic rendering works @ 60 FPS

### **Phase 2-3 (Months 2-6, SkyGuard):**
- [ ] RTX 4080: 240 FPS @ 5K entities + particles
- [ ] Radeon 610M: 60 FPS @ 5K entities
- [ ] Forced fallback: Multi-atlas path validated
- [ ] Optional: Intel laptop test (friend/wife) before Steam release

### **Phase 4 (Months 7-12, 4X Foundation):**
- [ ] RTX 4080: Texture system (arrays + multi-atlas both paths)
- [ ] Radeon 610M: Multi-atlas primary (arrays optional)
- [ ] Forced fallback: 100K textures across 4-8 atlases works
- [ ] Stress test: 100K entities @ 144 FPS (RTX 4080), 60 FPS (Radeon 610M)

### **Phase 5-6 (Months 13-33, 4X Production):**
- [ ] RTX 4080: 1M entities stress test (60 FPS minimum, 144 FPS target)
- [ ] Radeon 610M: 100K entities @ 30 FPS (acceptable for integrated)
- [ ] Beta testers: Community testing on Intel/AMD/NVIDIA mix
- [ ] Intel laptop: Final validation before Early Access release

---

## 🚨 COMMON ISSUES & FIXES

### **Issue: Radeon 610M only shows 512 texture array layers**
**Solution:**
```cpp
// Auto-fallback to multi-atlas if layers < 2048
if (maxLayers < 2048) {
    LOG_WARN("GPU only supports {} texture array layers, using multi-atlas fallback", maxLayers);
    mUseMultiAtlas = true;
}
```

### **Issue: DRI_PRIME=1 doesn't switch GPU**
**Check:**
```bash
# Verify DRI_PRIME is recognized
echo $DRI_PRIME  # Should output: 1

# Check if both GPUs visible
DRI_PRIME=1 glxinfo | grep "OpenGL renderer"

# If still uses NVIDIA, try:
__GLX_VENDOR_LIBRARY_NAME=mesa DRI_PRIME=1 ./skyguard
```

### **Issue: Intel laptop crashes on startup**
**Debug:**
```bash
# Check OpenGL version
glxinfo | grep "OpenGL version"
# Require OpenGL 3.3+ minimum

# If < 3.3, add compatibility fallback:
# - Reduce shader version (GLSL 330 → 150)
# - Disable texture arrays entirely on old Intel
```

### **Issue: FPS < 60 on integrated GPU**
**Optimize:**
- Reduce particle count (10K → 5K)
- Disable MSAA antialiasing
- Lower resolution (1920x1080 → 1280x720)
- Add quality presets (LOW/MEDIUM/HIGH)

---

## 🎯 MINIMUM SPECS (SkyGuard)

Based on testing, document in Steam page:

**Minimum (60 FPS @ LOW settings):**
- GPU: Intel UHD 630 / AMD Radeon 610M / NVIDIA GTX 1050
- RAM: 4GB
- VRAM: 1GB
- OpenGL: 3.3+

**Recommended (240 FPS @ HIGH settings):**
- GPU: NVIDIA RTX 3060 / AMD RX 6600 / Intel Arc A770
- RAM: 8GB
- VRAM: 4GB
- OpenGL: 4.6+

---

## 📊 BENCHMARK COMMANDS

### **Quick Performance Check:**
```bash
# SkyGuard stress (5K entities)
./skyguard --stress-test --entities=5000 --duration=60

# 4X stress (100K entities)
./skyguard --stress-test --entities=100000 --duration=60

# Profile mode (Tracy capture)
./skyguard --profile --stress-test --entities=50000
```

### **Automated Testing (CI):**
```bash
# Run all GPU paths
./run_gpu_tests.sh

# Expected output:
# [RTX 4080] 240 FPS @ 5K entities ✅
# [Radeon 610M] 60 FPS @ 5K entities ✅
# [Forced Fallback] 240 FPS @ 5K entities ✅
```

---

## ✅ FINAL CHECKLIST (Before Steam Release)

### **Month 6 (SkyGuard):**
- [ ] RTX 4080: 240 FPS @ 5K entities (HIGH settings)
- [ ] Radeon 610M: 60 FPS @ 5K entities (MEDIUM settings)
- [ ] Forced fallback: Multi-atlas validated
- [ ] Intel laptop (optional): 60 FPS @ LOW settings
- [ ] No crashes in 4-hour stress test (all GPUs)
- [ ] Memory leaks: 0 (Valgrind clean)
- [ ] Visual comparison: screenshots identical across all GPUs

### **Month 33 (Titan 4X Early Access):**
- [ ] RTX 4080: 144 FPS @ 20 civs, 10K provinces
- [ ] Radeon 610M: 60 FPS @ 20 civs, 10K provinces (LOW settings)
- [ ] Forced fallback: 100K textures across 4-8 atlases
- [ ] Beta testers: 50+ people tested on mixed hardware
- [ ] Intel/AMD/NVIDIA: all paths validated
- [ ] Crash rate: <1% across all GPUs (Steam analytics)

---

## 📝 LOGGING GPU INFO

**Add to startup:**
```cpp
void logGPUInfo() {
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* version = glGetString(GL_VERSION);
    
    LOG_INFO("GPU Renderer: {}", reinterpret_cast<const char*>(renderer));
    LOG_INFO("GPU Vendor: {}", reinterpret_cast<const char*>(vendor));
    LOG_INFO("OpenGL Version: {}", reinterpret_cast<const char*>(version));
    
    GLint maxLayers, maxTextureSize;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    
    LOG_INFO("Max Texture Array Layers: {}", maxLayers);
    LOG_INFO("Max Texture Size: {}x{}", maxTextureSize, maxTextureSize);
    
    // Decide rendering path
    if (maxLayers >= 2048) {
        LOG_INFO("Using Texture Arrays (GPU supports {} layers)", maxLayers);
    } else {
        LOG_INFO("Using Multi-Atlas Fallback (GPU only supports {} layers)", maxLayers);
    }
}
```

---

**END OF GUIDE**

Test early, test often, ship confident. 🚀
