# Things To Do

---

##### [FINISHED] 1. Pending Deletion (CRITICAL):

* TinyFS need a proper pending deletion system to avoid dangling references. Current implementation is straight up missing this feature.

##### [FINISHED] 2. Refactor TinySceneRT to use local runtime registry (CRITICAL):

* Ensure each TinySceneRT instance uses its own local rtRegistry for runtime data, eliminating all shared/global registry usage. This prevents cross-scene pointer invalidation and data corruption. Confirm all component management (writeComp, rtComp, etc.) is scene-local and robust.

##### [0/2] 3. Implement working Materials/Textures architecture (priority: CRITICAL):

* **[In Progress]** Choosing which design pattern to use and weighing their pros and cons.
* **[ ]** Actually implement the chosen design.

##### [1/2] Animation System (priority: High):


* **[FINISHED]** Add animation for nodes and skeletons.
* **[\*Halted]** Add animation for morph targets.
    ###### \* Require #3 to be finished first.

##### [?] Ray Tracing (priority: LOW):

* Adding ray tracing support via Vulkan RTX extensions? Who knows.

* Bit of history, but I used to make a ray tracing engine from scratch using CUDA GPGPU, you can find it here: [AsczEngineRT-v0](https://github.com/Asciizzz/AsczEngineRT-v0), this is the first engine where every technical implementations of 3D rendering (specifically ray tracing) were fully correct.
