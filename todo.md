# Things To Do

---

##### [FINISHED] Pending Deletion (CRITICAL):

* tinyFS need a proper pending deletion system to avoid dangling references. Current implementation is straight up missing this feature.

##### [FINISHED] Refactor tinySceneRT to use local runtime registry (CRITICAL):

* Ensure each tinySceneRT instance uses its own local rtRegistry for runtime data, eliminating all shared/global registry usage. This prevents cross-scene pointer invalidation and data corruption. Confirm all component management (writeComp, rtComp, etc.) is scene-local and robust.

##### [FINISHED] Fix Skeleton race condition (CRITICAL):

* Right now we are using 1 descriptor set per skeleton, which is not sufficient, and can cause race condition, we need 1 descriptor set per frame per skeleton.
* Hi, me after post-nut-clarity here, even better idea: dynamic offsets, create 1 buffer with region for each frame, and use dynamic offset to select region when binding descriptor set. This can be applied to literally every other per-frame resource as well, reducing descriptor set count significantly.

##### [2/2] Implement working Materials/Textures architecture (priority: CRITICAL):

* **[FINISHED]** Choosing which design pattern to use and weighing their pros and cons.
* **[FINISHED]** Actually implement the chosen design.

##### [ ] Fix the Materials-Textures dangling pointer issue (priority: CRITICAL - but not urgent (what?)):

* Godfcking dammit slop code lol, the design was implemented too quickly.

##### [1/2] Animation System (priority: High):

* **[FINISHED]** Add animation for nodes and skeletons.
* **[70%!!!]** Add animation for morph targets.
  * **[x]** Adding successful morph shader and vulkan support.
  * **[ ]** Fix the GODDAMN TINYLOADER TO SUPPORT THIS.

##### [ ] Fixing the r*tarded tinyLoader's glTF loader (priority: High):

* Idk man it genuinely hurts my brain reasoning with glTF's “lossy abstraction boundary”.

##### [?] Scripting (priority: High):

* Yo chat, should I use Lua, or write my own language? Top comment wins.

##### [?] Ray Tracing (priority: low):

* Adding ray tracing support via Vulkan RTX extensions? Who knows.

* Bit of history, but I used to make a ray tracing engine from scratch using CUDA GPGPU, you can find it here: [AsczEngineRT-v0](https://github.com/Asciizzz/AsczEngineRT-v0), this is the first engine where every technical implementations of 3D rendering (specifically ray tracing) were fully correct.
