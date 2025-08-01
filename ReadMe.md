# AsczGame Engine - (*Subject to change for the worse*)

A Vulkan-based engine that started as “how tf do I draw a triangle” and somehow evolved into something *functional*. Performance-focused and kinda proud of it, actually.

Same guy that made "2D platformer supposed to run grandma pc, struggling on 3060" here.

---

## Recent Progress

### Rendering

* **Total instancing/batching rework** — old system made me physically ill.
* Went from barely rendering **100k triangles** to **2.7 million at 500 FPS**.
* **Flexible material system** added. Can’t believe I didn’t have one before.
* Overlapping geometry? **No longer tanks framerate.** We win these.

### Physics

* Built a **BVH system** for mesh collision — originally made for [AsczEngineRT_v0](https://github.com/Asciizzz/AsczEngineRT-v0), still holds up.
* Added **spatial grid** for particle-particle collisions. Goodbye O(n²) shame.
* **1000 particles** with full collision at **1000 FPS** (CPU-side only, GPU chilling for now).

### Build Config & Runtime

* Discovered I was testing in **Debug mode** like an absolute moron.
* Switching to **Release** shrunk the binary from 2.9MB → 600KB and boosted perf by *only* **1000%**.
* Can now handle **2.7M triangles** and **1000+ particles** while sitting at **400–1000 FPS**, depending on how cursed the scene is.
* **Portable build works** — no Vulkan SDK needed. And no, it’s not a virus. Just open it, fill in your credits card's numbers, it's expiration date and the 3 quirky digits on the back.

---

## Features That Technically Exist

### Rendering

* Vulkan backend that doesn’t catch fire.
* Instancing and batching that aren’t just for-loop.
* Material system that isn’t duct-taped to a single texture.
* Comfortable with **millions of triangles**.

### Physics

* Particle ↔ map and particle ↔ particle collisions.
* **BVH** for meshes — fast, maybe wrong in edge cases (don’t test it too hard).
* Collision response: objects bounce, don’t disappear into the void.

### Build System

* CMake-based with proper Debug/Release configs.
* MSVC `/O2` and GCC `-O3` — hot CPUs, small binaries.
* `build_portable.bat` script that actually does what it says.

---

## Build Instructions

**Debug Build** (don’t):

```bash
cmake --build build --config Debug
# Outputs AsczGame_debug.exe
```

**Release Build** (yes):

```bash
cmake --build build --config Release
# Outputs AsczGame_release.exe
```

**Portable Build** (what people want):

```bash
./build_portable.bat
# Creates a folder with exe + required DLLs
```

---

## Things I Learned

* Vulkan is hell, but I sinned, so it cancels out.
* FPS go up when you optimize things. Revolutionary.
* "The hell is a Release build?" - "Oh."

---

## Final Thoughts

With a solid foundation and real optimizations, I can start implementing heavy features without the engine crumbling like a sandcastle in a tidal wave.

Add me on CS2. No, I’m not trading skins.
* [Agn3s Tachyon](https://steamcommunity.com/profiles/76561199223964635/)
