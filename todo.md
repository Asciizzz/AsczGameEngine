# Things To Do

---

##### [FINISHED] Pending Deletion (CRITICAL):

* TinyFS need a proper pending deletion system to avoid dangling references. Current implementation is straight up missing this feature.

##### [FINISHED] Refactor TinySceneRT to use local runtime registry (CRITICAL):

* Ensure each TinySceneRT instance uses its own local rtRegistry for runtime data, eliminating all shared/global registry usage. This prevents cross-scene pointer invalidation and data corruption. Confirm all component management (writeComp, rtComp, etc.) is scene-local and robust.

##### [ ] Fix Skeleton race condition (CRITICAL):

* Right now we are using 1 descriptor set per skeleton, which is not sufficient, and can cause, we need 1 descriptor set per frame per skeleton.

##### [0/2] Implement working Materials/Textures architecture (priority: CRITICAL):

* **[In Progress]** Choosing which design pattern to use and weighing their pros and cons.
* **[ ]** Actually implement the chosen design.

##### [1/2] Animation System (priority: High):

* **[FINISHED]** Add animation for nodes and skeletons.
* **[\*Halted]** Add animation for morph targets.
    ###### \* Require the above architecture to be finished first.

##### [ ] Fixing the r*tarded TinyLoader's glTF loader (priority: High):

* Idk man it genuinely hurts my brain reasoning with glTF's “lossy abstraction boundary”.

##### [?] Scripting (priority: High):

* Yo chat, should I use Lua, or write my own language? Top comment wins.

##### [?] Ray Tracing (priority: low):

* Adding ray tracing support via Vulkan RTX extensions? Who knows.

* Bit of history, but I used to make a ray tracing engine from scratch using CUDA GPGPU, you can find it here: [AsczEngineRT-v0](https://github.com/Asciizzz/AsczEngineRT-v0), this is the first engine where every technical implementations of 3D rendering (specifically ray tracing) were fully correct.
