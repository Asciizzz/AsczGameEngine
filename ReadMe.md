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

### the smallest change

- renamed everything from `Tiny_` to `tiny_` because it's just appropriate.

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

* **TinyFS + Registry System** - dual-layer asset management.
* Organized file structures with proper resource handling.

### Data Pipeline

#### 1. **TinyLoader** - Asset Import
* Converts glTF models to `TinyModel` format.
* Extracts meshes, materials, textures, skeletons, and scene hierarchy.
* Materials and meshes now receive proper names from source data.
* Raw data stage before engine processing.

#### 2. **TinyFS** - Virtual File System
* Two-layer architecture:
  * **Filesystem layer**: Hierarchical folder organization.
  * **Registry layer**: GPU resources and processed data storage.
* Auto-generates folder structures: `Textures/`, `Materials/`, `Meshes/`, `Skeletons/`, `Scenes/`.
* **TypeHandle system**: Type-safe links between filesystem and registry layers.
* Full drag & drop support for file management.

#### 3. **Scene Management**
* **Library scenes**: Reusable templates stored in TinyFS.
* **Runtime instances**: Active game objects in the scene hierarchy.
* Scene instantiation via drag & drop from library to runtime nodes.
* **Handle-based references**: Type-safe resource management without pointer issues.

### UI System

* **Scene Manager**: File browser with drag & drop scene instantiation.
* **Inspector**: Context-sensitive panel for nodes and folder management.
* **Gimbal lock mitigation**: Rotation controls that work past ±90°.
* **Visual feedback**: Color-coded file types with hover states.

---

### Workflow

**Asset Import Process**
* Place glTF models in the Assets folder.
* TinyLoader processes the model and extracts components.
* TinyFS automatically creates organized folder structure.

**Generated Structure:**
```
Human/
├── Textures/
│   └── skin_texture.png
├── Materials/
│   ├── head_material
│   ├── body_material
│   ├── arms_material
│   └── legs_material (all reference skin_texture)
├── Meshes/
│   ├── head_mesh
│   ├── body_mesh
│   ├── arms_mesh
│   └── legs_mesh
├── Skeletons/
│   └── armature_skeleton
└── Scenes/
    └── Human_scene (complete prefab)
```

**Usage**
* **Normal drag**: File organization between folders.
* **Ctrl+drag scene to node**: Scene instantiation in runtime hierarchy.
* **Selection**: Inspector displays node properties or folder management options.

**Asset Management**
* Properly named resources from source data.
* No auto-generated names like "Material_47".
* Organized hierarchy maintains relationships.

**Handle System**
* `TinyHandle` provides type-safe resource references.
* **Type safety**: Prevents incorrect resource type usage.
* **Version tracking**: Handles detect when referenced data is deleted.
* **Cross-layer linking**: Filesystem nodes reference registry data safely.
* Eliminates dangling pointer issues.

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
