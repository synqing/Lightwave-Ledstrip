# Blender Agent: K1 Animation Re-render

## Your Task

Re-render the existing K1 Blender animation with a new background colour so the output integrates seamlessly into a dark landing page. The `.blend` file already exists on this machine — you need to find it, modify the world background, verify the scene is intact, and render the animation.

## Finding the Project

The Blender project is at:

```
/Users/spectrasynq/SpectraSynq.LandingPage/Blender/
```

List the `.blend` files in that directory and open the main project file. The scene contains a 3D model of the "K1 Lightwave" — a bar-shaped LED device approximately 330mm long. You'll recognise it by the bar/slab geometry with a flat front face (the Light Guide Plate / LGP display area).

## What the Scene Contains

The K1 model was built from actual CAD files. Key elements:

- **Main body**: A ~330mm × ~66mm bar, rear tapered toward the middle (V-profile). Two halves joined at centre.
- **Centre connector**: A complex geometric piece bridging both halves. Triangle-inspired design (from the SpectraSynq logo). Likely a different material/colour from the body.
- **End caps**: Separate pieces at each end. In some renders they were gold/yellow — this was experimental. Their current colour doesn't matter for this task.
- **LGP (Light Guide Plate)**: The ~51mm wide front display surface. This is the critical element — it has **UV-mapped video overlays** applied as textures showing the K1 performing real light effects. These are actual video clips (recorded on a Sony A7S3) of the K1 running effects, mapped onto the LGP surface.
- **Mounting clips**: Small clip/bracket pieces (likely silver/chrome material).

## CRITICAL: The LGP Video Overlays

The LGP surface has one or more video textures (image sequences or movie clips) UV-mapped onto it. These show the K1's actual light effects — liquid colour gradients radiating from the centre outward. **Do not modify, remove, or break these video textures.** They are the entire point of the animation.

Before rendering, verify:
1. The LGP material node tree still has its Image Texture / Movie Clip node connected
2. The video source file paths resolve correctly (they may be relative or absolute)
3. If paths are broken, check for the video files nearby and relink them

## What to Change

### World Background
Change the world/environment background to: **`#0A0A0C`** (RGB: 10, 10, 12)

This is a near-black with a very slight cool blue cast. In Blender:

```python
import bpy

world = bpy.context.scene.world
if world is None:
    world = bpy.data.worlds.new("World")
    bpy.context.scene.world = world

world.use_nodes = True
bg_node = world.node_tree.nodes.get("Background")
if bg_node:
    # sRGB hex #0A0A0C → linear RGB (Blender uses linear colour space)
    bg_node.inputs['Color'].default_value = (0.003035, 0.003035, 0.003564, 1.0)
    bg_node.inputs['Strength'].default_value = 1.0
```

**Note on colour space:** Blender works in linear colour space internally. The hex `#0A0A0C` is sRGB. The conversion above accounts for this (sRGB → linear via the standard 2.2 gamma approximation). If you use the colour picker UI instead of the script, make sure you're entering the hex value in the colour picker's sRGB/hex field, NOT the linear RGB fields.

### Lighting
Do NOT change any existing lights, HDRI, or emission materials. The existing lighting setup was designed to show the K1's physical form. Only the world background colour changes.

If the existing scene uses an HDRI for the world background, you have two options:
- **Option A (preferred):** Keep the HDRI for lighting but set the world surface to use the flat colour for camera rays only. In the World shader nodes: add a Light Path node, use "Is Camera Ray" to mix between the HDRI (for lighting) and the flat `#0A0A0C` colour (for what the camera sees as background).
- **Option B (simpler):** If the scene doesn't rely on HDRI lighting (uses area lights / point lights instead), just replace the world background entirely with the flat colour.

### Camera and Animation
Do NOT change camera position, focal length, animation keyframes, frame range, or any other animation settings. The existing animation is correct.

## Render Output Settings

Configure the output for web use:

```python
import bpy

scene = bpy.context.scene

# Output format
scene.render.image_settings.file_format = 'FFMPEG'
scene.render.ffmpeg.format = 'MPEG4'
scene.render.ffmpeg.codec = 'H264'
scene.render.ffmpeg.constant_rate_factor = 'MEDIUM'  # CRF ~23, good quality/size balance
scene.render.ffmpeg.ffmpeg_preset = 'GOOD'
scene.render.ffmpeg.gopsize = 18
scene.render.ffmpeg.audio_codec = 'NONE'  # No audio needed

# Resolution (keep existing or set to 1920x1080)
scene.render.resolution_x = 1920
scene.render.resolution_y = 1080
scene.render.resolution_percentage = 100

# Frame rate (keep existing — likely 60fps or 30fps, don't change it)
# scene.render.fps = <leave as-is>

# Output path
scene.render.filepath = '//k1-animation-dark-bg.mp4'

# If using EEVEE (likely for this type of scene):
# Ensure film is NOT transparent (we want the #0A0A0C background baked in)
scene.render.film_transparent = False
```

**Output location:** Save to `/Users/spectrasynq/SpectraSynq.LandingPage/Blender/k1-animation-dark-bg.mp4`.

## Render Engine

Check what engine the scene is already using (likely EEVEE for a product visualisation with video textures). Do NOT switch engines. If it's EEVEE, render with EEVEE. If it's Cycles, render with Cycles.

## Pre-render Checklist

Before hitting render, verify:

1. [ ] World background is `#0A0A0C` (visually near-black, very slight blue tint)
2. [ ] LGP video texture(s) are still connected and paths resolve
3. [ ] `film_transparent` is OFF (background must be baked into the video)
4. [ ] Camera and animation keyframes are unchanged
5. [ ] Lighting is unchanged
6. [ ] Output is H.264 MP4, no audio
7. [ ] Resolution is 1920×1080 (or whatever the original was — don't downscale)
8. [ ] Frame range matches the original (likely ~1293 frames at 60fps = ~21.5 seconds)

## After Rendering

1. Verify the output plays correctly and the LGP shows the colour effects
2. Check the background is solid near-black with no artifacts or banding
3. Report the file location, duration, resolution, file size, and frame count
4. If there were multiple video overlays / effect states on the LGP, note what they show (e.g., "amber/fire effect 0-5s, cyan/blue 5-10s, magenta 10-15s" etc.)

## What NOT to Do

- Do NOT modify any materials except the world background
- Do NOT move, scale, or rotate any objects
- Do NOT change camera settings
- Do NOT add or remove objects from the scene
- Do NOT change the render engine
- Do NOT enable film transparency (the dark background must be IN the video)
- Do NOT re-encode or compress the output further after rendering
- Do NOT modify the LGP video textures or their UV mapping
