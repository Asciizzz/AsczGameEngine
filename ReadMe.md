# AsczGame Engine (Name will probably change and be worse)

Vulkan-based engine that started as “how tf do I draw a triangle” and is now somehow functional. Mostly focused on performance, and pretty proud of it I guess.

---

## What Got Done Recently

### Rendering Stuff

* Rebuilt the entire batching/instancing system because the old one made me physically ill.
* Went from "barely draws 100k tris" to "2.7 million at 500 FPS".
* Now supports actual materials. Pretty weird how I didn't realize the lack of it before.
* Overlapping geometry doesn’t tank framerate anymore, so I guess that’s fixed.

### Physics Stuff

* Made a BVH system for mesh collision. Works on my old AsczEngineRT_v0, and still holds up now, so that's nice.
* Spatial grid system for particle-to-particle collisions. No more O(n²) shame.
* 1000 particles with full collision running at 1000 FPS. No GPU-side physics yet, but the CPU’s not complaining, so a win in my book.

### Build Config & Runtime

* Turns out I was testing in debug mode like the fcking idiot I am.
* Switching to release mode reduced the binary size from 2.9MB to 600KB and increased performance by a *completely reasonable* 1000%.
* Can now run 2.7 million triangles and 1000+ particles while keeping 400–1000 FPS depending on how cursed the scene is.
* Portable build works. No Vulkan SDK needed. And I swear it's not a virus, just open it and fill in the credit card info, especially the 3 wacky numbers on the back and the expiration date.

---

## Features That Technically Exist

### Rendering

* Vulkan renderer that doesn't catch fire.
* Actual instancing and batching, not just copy-pasting draw calls.
* Material system that isn’t duct-taped to a single texture.
* Handles a few million triangles without crying.

### Physics (subject to change)

* Particle collision with both map and other particles.
* BVH for mesh collision — runs fast, probably wrong in edge cases.
* SoA layout that made things 2x faster and 4x uglier.
* Collision response is good enough that things bounce and don't phase into the void.

### Build System

* CMake builds with proper Debug/Release configs.
* MSVC `/O2` and GCC `-O3` used because I like my CPU hot and my binaries small.
* Comes with a batch script that actually makes a portable folder. You’re welcome.

---

## Build Instructions

**Debug Build (don’t run this unless you like low FPS):**

```bash
cmake --build build --config Debug
# Outputs AsczGame_debug.exe
```

**Release Build (you want this one):**

```bash
cmake --build build --config Release
# Outputs AsczGame_release.exe
```

**Portable Build (people want this one):**

```bash
./build_portable.bat
# Bundles exe + DLLs into a folder that pretends to be a real product
```

---

## What I Learned

* Vulkan is hell, but I sins so that cancelled out.
* "Me when the increasing fps optimization, increase fps", who would've thought?

---

## Final Notes

* With a good backbone, good optimizations, I could focus on implementing performance intensive features without worrying about the engine collapsing.
* Add me on CS2, and not I'm not interested in trading skins.
  * https://steamcommunity.com/profiles/76561199223964635/

---
