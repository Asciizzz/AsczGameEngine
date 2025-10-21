# Things To Do

---

##### ~~Pending Deletion (priority: CRITICAL)~~ ✅ COMPLETED

* ~~TinyFS need a proper pending deletion system to avoid dangling references. Current implementation is straight up missing this feature.~~ ✅ **DONE! Implemented full Vulkan synchronization with fence waiting and safe resource cleanup!**

##### Animation System (priority: CRITICAL 🔥)

* Add animation system now that skeletons are working properly. ~~Just don't delete them right now unless you want to turn your pc into a work of art (descriptor set being deleted mid <Pipeline - Command Buffer> usage issue).~~ **Deletion is now safe thanks to proper pending deletion system!**

##### Ray Tracing? (priority: LOW)

* Adding ray tracing support via Vulkan RTX extensions? Who knows.