Weighted, Blended Order-Independent Transparency
I'm leaving this up for historical reasons, but I recommend my more recent post on blended order-independent transparency for implementors.

Glass, smoke, water, and alpha-cutouts like foliage are important visual elements in games that are all challenging to render because they transmit light from the background. This post explains a new method called Weighted, Blended OIT for rendering all of these effects quickly and with pretty good quality on a wide set of target platforms. (As is common game developer jargon, I refer to all of these as "transparency" effects. The appendix of my earlier transparent shadow paper with Eric Enderton explains the subtle differences between them.)

The traditional approach is to sort the transparent polygons, and then render them in order from farthest from the camera to nearest to the camera. That sorting is itself slow, and causes further performance problems because it triggers more draw calls and state changes than otherwise needed and prevents common performance optimizations such as rendering amorphous surfaces like smoke at low resolution.

Most games have ad-hoc and scene-dependent ways of working around transparent surface rendering limitations. These include limited sorting, additive-only blending, and hard-coded render and composite ordering. Most of these methods also break at some point during gameplay and create visual artifacts. One not-viable alternative is depth peeling, which produces good images but is too slow for scenes with many layers both in theory and practice. There are many asymptotically fast solutions for transparency rendering, such as bounded A-buffer approximations using programmable blending (e.g., Marco Salvi's recent work), ray tracing (e.g., using OptiX), and stochastic transparency (as explained by Eric Enderton and others). One or more of these will probably dominate in five years, but all are impractical today on the game platforms that I develop for, including PC DX11/GL4 GPUs, mobile devices with OpenGL ES 3.0 GPUs, and next-generation consoles such as PlayStation 4.


A transparent CAD view of a car engine rendered by our technique.

Weighted, Blended Order-Independent Transparency is a new technique that Louis Bavoil and I invented at NVIDIA and published in the Journal of Computer Graphics Techniques to address the transparency problem on a broad class of current gaming platforms. Our technique renders non-refractive, monochrome transmission through surfaces that themselves have color, without requiring sorting or new hardware features. In fact, it can be implemented with a simple shader for any GPU that supports blending to render targets with more than 8 bits per channel. It works best on GPUs with multiple render targets and floating-point texture, where it is faster than sorted transparency and avoids sorting artifacts and popping for particle systems. It also consumes less bandwidth than even a 4-deep RGBA8 k-buffer and allows mixing low-resolution particles with full-resolution surfaces such as glass. For the mixed resolution case, the peak memory cost remains that of the higher resolution render target but bandwidth cost falls based on the proportional of low-resolution surfaces.

The basic idea of our OIT method is to compute the coverage of the background by transparent surfaces exactly, but to only approximate the light scattered towards the camera by the transparent surfaces themselves. The algorithm imposes a heuristic on inter-occlusion factor among transparent surfaces that increases with distance from the camera. After all transparent surfaces have been rendered, it then performs a full-screen normalization and compositing pass to reduce errors where the heuristic was a poor approximation of the true inter-occlusion.

The CAD-style visualization of the engine above shows that our method can handle many overlapping transparent surfaces with different colors. Below we show a scene with many overlapping glass layers using a glass chess set. Note that the post-processed depth of field even works reasonably well because we fill in the depth buffer values after compositing transparent surfaces. The reflections are rendered with an environment map and there is no refraction.


A glass chess set rendered with our technique.


In the pond video below, we show many overlapping transparent primitives that have highly varying transmission (alpha) and color values. The water is both reflective and transmissive according to Fresnel's equations. The fog has a Perlin noise texture and is rendered with low-coverage billboards. All of the tree leaves and the grass are rendered with alpha-cutouts applied to simple quads. In fact, sorted transparency cannot even render this scene because the fog and tree leaves pass through each other, as do the water plants and pond surface. So, there is no valid triangle ordering for producing the correct image and a per-pixel method such as ours is required to produce a convincing image.


Pond scene showing different kinds of transparent surfaces simultaneously: water with transmission varying by Fresnel's equations, alpha-cutout foliage, and large fog billboards, rendered in real-time by our technique.

The primary limitation of the technique is that the weighting heuristic must be tuned for the anticipated depth range and opacity of transparent surfaces. We implemented it in OpenGL for the G3D Innovation Engine and DirectX for the Unreal Engine to produce the results here and in the paper. Dan Bagnell and Patrick Cozzi implemented it in WebGL for their open-source Cesium engine (see also their new blog post discussing it). From that experience we found a good set of weight functions, which we report in the journal paper. In the paper, we also discuss how to spot artifacts from a poorly-tuned weighting function and fix them.


Smoke scene. This example shows high opacity used for concealment during gameplay, and smooth transitions during simulation unlike the popping from traditional ordered billboard blendeding.

Implementing Weighted, Blended OIT
The shader modifications to implement the technique are very simple. During transparent surface rendering, shade surfaces as usual but output to two render targets. The first render target must have at least RGBA16F precision and the second must have at least R8 precision. Clear the first render target to vec4(0) and the second render target to 1 (using a pixel shader or glClearBuffer + glClear).

Then, render the surfaces in any order to these render targets, adding the following to the bottom of the pixel shader and using the specified blending modes:

// Output linear (not gamma encoded!), unmultiplied color from
// the rest of the shader.
vec4 color = ... // regular shading code



// Insert your favorite weighting function here. The color-based factor
// avoids color pollution from the edges of wispy clouds. The z-based
// factor gives precedence to nearer surfaces.
float weight = 
      max(min(1.0, max(max(color.r, color.g), color.b) * color.a)), color.a) *
      clamp(0.03 / (1e-5 + pow(z / 200, 4.0)), 1e-2, 3e3);

// Blend Func: GL_ONE, GL_ONE
// Switch to premultiplied alpha and weight
gl_FragData[0] = vec4(color.rgb * color.a, color.a) * weight;

// Blend Func: GL_ZERO, GL_ONE_MINUS_SRC_ALPHA
gl_FragData[1].a = color.a;

If you are rendering some calls (such as particles or out of focus regions) at low resolution, then you can run the above code to different resolution textures independently and combine them during compositing. It will work even if the low- and high-resolution content are interleaved in depth.

Finally, after all surfaces have been rendered, composite the result onto the screen using a full-screen pass:

vec4 accum = texelFetch(RT0, int2(gl_FragCoord.xy), 0);
float reveal = texelFetch(RT1, int2(gl_FragCoord.xy), 0).r;

// Blend Func: GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA
gl_FragColor = vec4(accum.rgb / max(accum.a, 1e-5), reveal);

The compositing must occur before gamma-encoding (that is, I'm assuming that the renderer performs gamma-encoding (if needed by your target), color grading, etc. in a subsequent full-screen pass.)

See the paper for full details and fallback paths for GPUs or APIs that do not support multiple render targets or per-render target blending functions.

Keep the depth buffer that was rendered for opaque surfaces bound for depth testing when rendering transparent surfaces, but do not write to it. This preserves early-Z culling, which can significantly improve throughput. I recommend rendering surfaces that have alpha cutouts during both the transparent and opaque passes. During the opaque pass, use an alpha test of GL_EQUAL to 1.0. This makes the fully-opaque regions of the surfaces appear in the depth buffer and avoids any fading of that part of the surface with depth.

To help others with implementing this method, I rendered a simple scene using weight equation 9 from the paper. I recommend trying to reproduce this image before evaluating on more complex scenes:


Color values are given before pre-multiplication, e.g., the premultiplied blue value is actually (0,0,0.75,0.75).
Note that compositing is in "linear" RGB space, but the whole image is converted to sRGB for final display.

Note that weighted, blended OIT is a blending operation. If the results that you see are outside the range of the original surfaces, that must be a bug in your implementation. For example, regions darker than the original colors are likely underflow during blending. Regions that are brighter than the original surfaces correspond to overflow (saturation)...or due to forgetting to use premultiplied alpha.

The curves in the paper are tuned for avoiding precision problems with 1-100 surfaces using 16-bit float blending, assuming that the alpha values are roughly on the range [0.2-0.9] and that in scenes with very high transparent surface counts, those surfaces have lower alpha values (e.g., smoke clouds).

Louis and I thank the graphics team at Vicarious Visions for their support and feedback while developing this algorithm and Analytic Graphics for their WebGL expertise.