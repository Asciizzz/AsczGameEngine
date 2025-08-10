In Vulkan, you don’t “magically” grab the depth from the screen — you have to explicitly set up your pipeline to store that depth information somewhere you can sample it later.

Here’s the general flow to make that happen for post-process effects like fog:

1. Create a depth attachment that’s also a sampled image

Normally your depth buffer is just a transient attachment (used for depth testing, then discarded).

Instead, make it a VkImage with usage flags:

VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | 
VK_IMAGE_USAGE_SAMPLED_BIT
Pick a depth format that’s supported for sampling on your GPU — common ones:

VK_FORMAT_D32_SFLOAT

VK_FORMAT_D24_UNORM_S8_UINT (if you need stencil too)

1. Set up an image view for sampling

Just like you do for textures, create a VkImageView for the depth image.

The aspectMask should be VK_IMAGE_ASPECT_DEPTH_BIT (and STENCIL_BIT if you need it).

3. Render scene normally, writing to depth

In your first pass, depth testing happens as usual, filling the depth attachment.

Because it’s a regular sampled image, it will persist after the pass ends.

4. Transition depth image layout before sampling

After your geometry pass, transition the depth image from
VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
to
VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.

This makes it readable in your post-processing pass.

Use a VkImageMemoryBarrier in a pipeline barrier for this.

5. Sample it in a fragment shader

Bind it as a sampler2D (or sampler2DShadow if you want hardware compare) in your post-process pass.

The raw sampled value will be non-linear depth (from projection space), usually between 0 and 1.

You can linearize it with:


float LinearizeDepth(float depth, float near, float far) {
    float z = depth * 2.0 - 1.0; // Back to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

6. Apply your fog equation
Once you have linear depth in world units, you can blend your fog color based on distance:

float fogFactor = clamp((distance - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
vec3 finalColor = mix(sceneColor, fogColor, fogFactor);

✅ Important: You can also avoid an extra pass by using a single combined G-buffer in deferred rendering, but for forward rendering, the above “depth image + sample in post pass” approach is the way to go.