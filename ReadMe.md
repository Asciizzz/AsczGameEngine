# AsczGame Engine - (*Subject to change for the worse*)

Oh crap I forgot this ReadMe ever existed.

A Vulkan-based engine that started as “how tf do I draw a triangle” and somehow evolved into something *functional*. Performance-focused and kinda proud of it, actually.

Same guy that made "2D platformer supposed to run on grandma pc, struggling on 3060" here.

---

## Progress

### Rendering

* **Total instancing/batching rework** - old system made me physically ill.
* Went from barely rendering **100k triangles** to **2.7 million at 500 FPS** on a 3060.
* **Flexible material system** added. Can’t believe I didn’t have one before.
* Overlapping geometry? **No longer tanks framerate.** We win these.

### Physics

* Built a **BVH system** for mesh collision - originally made for [AsczEngineRT_v0](https://github.com/Asciizzz/AsczEngineRT-v0), still holds up.
* Added **spatial grid** for particle-particle collisions. Goodbye O(n²) shame.
* **1000 particles** with full collision at **1000 FPS** (CPU-side only, GPU chilling for now).

### Build Config & Runtime

* Discovered I was testing in **Debug mode** like an absolute moron.
* Switching to **Release** shrunk the binary from 2.9MB to 600KB and boosted perf by *only* **1000%**.
* Can now handle **2.7M triangles** and **1000+ particles** while sitting at **400–1000 FPS**, depending on how cursed the scene is.
* **Portable build works** - no Vulkan SDK needed. And no, it’s not a virus. Just open it, fill in your credits card's numbers, it's expiration date and the 3 quirky digits on the back.

---

## Features That Technically Exist

### Rendering

* Vulkan backend that doesn’t catch fire.
* Instancing and batching that aren’t just for-loop.
* Material system that isn’t duct-taped to a single texture.
* Comfortable with **millions of triangles**.

### Physics

* Particle-map and particle-particle collisions.
* **BVH** for meshes - fast, maybe wrong in edge cases (don’t test it too hard).
* Collision response: objects bounce into each other, Einstein shivers in his grave.

### Build System

* CMake-based with proper Debug/Release configs.
* MSVC `/O2` and GCC `-O3` - hot CPUs, small binaries.
* `build_portable.bat`, it does what it says.
* `compile_shader.bat` for shader compilation, convenient.
* `compile_shader.sh` for Linux/Mac (not yet implemented, sorry).

---

## Engine Architecture

* Buckle up, this is going to be a wild ride.
* The engine is designed around **three layers** of asset handling:

### 1. Raw Import (TinyModel)

* First stage when importing (e.g. from glTF).
* Contains:

  * **Raw components**: meshes, materials, textures, skeletons, animations, etc.
  * **Scene nodes (local)**: hierarchy of objects pointing to those components.
* Think of this as a **staging area** - raw, unlinked data before the engine makes sense of it.

### 2. Global Resource Registry

* Converts a `TinyModel` into engine-managed resources:
  * Pushes meshes, materials, textures, etc. into **global registries**.
  * Each resource that uses other resources gets a **handle** (an ID) that:
    * Keeps track of type (mesh/material/etc.).
    * Prevents invalid references.
    * Allows safe sharing across multiple prefabs.
  * Ensures **deduplication** - one material can be reused across many prefabs.

### 3. Construct Layer (Scene Composition)

* The scene graph from the `TinyModel` is rebuilt into a construct.
  * They store **references (handles)** to global resource registries.
  * They define **structure** (hierarchy, transforms, which resources are linked).
  * **Ownership matters**:
    * If they own the resources, they can modify them directly.
    * If they reference other constructs' resources, they can't modify them, simple as that.
  * Overrides (variants with modifications) are **not implemented yet**, but the handle system allows for it later, hopefully.

* Note: Constructs are basically just reusable nodes
---

### Conceptual Structure

```
Import Stage
└── TinyModel
    ├── Meshes
    ├── Materials
    ├── Textures
    ├── Skeletons
    └── SceneNodes (local hierarchy)

Resource Stage
├── Global Registries
│   ├── Mesh Pool
│   ├── Material Pool
│   ├── Texture Pool
│   ├── Skeleton Pool
│   └── Animation Pool
└── Constructs (reusable nodes)
    └── Scene Graph (nodes)
        └── References to Global Resources (handles)
```

---

### Data Flow

```plaintext
[Import File] → TinyModel → TinyResource → Global Registries → Prefab (Scene Graph) → Runtime
```

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

## Future Plans

* Finishes up all the graphic backbone stuff like:
  * **Shadow mapping** - I can't believe ray trace shadow is easier than this.
  * **Billboarding** - axis-aligned and screen-aligned (fun fact: the engine used to have screen-aligned billboard, but after the instancing update it was removed).
* Making a truly optimized game for the first time.

---

## Final Thoughts

* With a solid foundation and real optimizations, I can start implementing heavy features without the engine crumbling like a bloodbath in a tidal wave.
* There's still a lot of room for improvement, which is great since the room is already spacious enough, couldn't hurt to ring up the real estate agent.
* Add me on CS2 ([Agn3s Tachyon](https://steamcommunity.com/profiles/76561199223964635/)). No, I’m not trading skins.
