# Things To Do

---

##### [FINISHED] Pending Deletion (CRITICAL):

* TinyFS need a proper pending deletion system to avoid dangling references. Current implementation is straight up missing this feature.

##### [FINISHED] Refactor TinyScene to use local runtime registry (CRITICAL):

* Ensure each TinyScene instance uses its own local rtRegistry for runtime data, eliminating all shared/global registry usage. This prevents cross-scene pointer invalidation and data corruption. Confirm all component management (writeComp, rtComp, etc.) is scene-local and robust.

##### [1/2] Animation System (priority: High)

* **[FINISHED]** Add animation for nodes and skeletons.
* **[ ]** Add animation for morph targets.

##### [?] Ray Tracing (priority: LOW)

* Adding ray tracing support via Vulkan RTX extensions? Who knows.

* Bit of history, but I used to make a ray tracing engine from scratch using CUDA GPGPU, you can find it here: ![AsczEngineRT-v0](https://github.com/Asciizzz/AsczEngineRT-v0), this is the first engine where every technical implementations of 3D rendering were fully correct.

##### 