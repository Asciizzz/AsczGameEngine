| Name               | Who They Are                                | How They Relate                                                                                         |
| ------------------ | ------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| `VkInstance`       | The **global context** of your app          | You create this first. It’s like saying “I exist now in Vulkan world.”                                  |
| `VkSurfaceKHR`     | The **window handle for Vulkan**            | You ask your window system (SDL, GLFW) to create this **for Vulkan** — it connects to your `VkInstance` |
| `VkPhysicalDevice` | The **actual GPU**                          | You ask Vulkan: “Show me your GPUs” and pick one that works for your surface                            |
| `VkDevice`         | The **logical link to GPU**                 | You create this from the PhysicalDevice once you pick a GPU. Used for commands, memory, pipelines       |
| `VkQueue`          | The **mailman for commands**                | You get these when you create `VkDevice`. They submit draw/compute commands to the GPU                  |
| `VkCommandPool`    | The **buffer allocator for queues**         | You allocate `VkCommandBuffer` from this, which the `VkQueue` can submit                                |
| `VkSwapchainKHR`   | The **chain of images shown to the screen** | Created once you have device + surface + formats. The final image renderer                              |
| `VkCommandBuffer`  | The **script for drawing commands**         | Recorded with commands, sent by `VkQueue`                                                               |
| `VkPipeline`       | The **fixed sequence of shader + states**   | You bind this inside a command buffer when drawing                                                      |

```cpp
VkInstance
   │
   ├── VkSurfaceKHR (via SDL)
   │
   ├── VkPhysicalDevice (GPU choice)
   │      │
   │      └── VkDevice (Logical Device)
   │              ├── VkQueue (Graphics / Present)
   │              └── VkCommandPool
   │                  └── VkCommandBuffer (Draw commands)
   │
   └── VkSwapchainKHR (Images to present)
```