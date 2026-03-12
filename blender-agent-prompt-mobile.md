# Blender Agent: K1 Animation — Mobile Render

## Your Task

Render a **mobile-optimised** version of the K1 animation for use as a hero video on a portrait-oriented landing page. You will work from the same Blender project used for the desktop render, but with a different camera setup and output resolution.

**You are NOT modifying the desktop render.** You are creating a second output alongside it.

## Context: What Already Exists

A desktop render (`k1-animation-dark-bg.mp4`, 1920×1080, 30fps, 53s) was already produced from this project with the world background set to `#0A0A0C`. If the background is already correct, leave it. If it's not `#0A0A0C`, set it using the script in the "World Background" section below.

## The Project

The Blender project is at:

```
/Users/spectrasynq/SpectraSynq.LandingPage/Blender/
```

List the `.blend` files and open the main project file.

## What the Scene Contains

The K1 is a bar-shaped LED device. Key elements:

- **Main body**: A ~330mm × ~66mm bar (roughly 5:1 aspect ratio), rear tapered toward the middle (V-profile). Two halves joined at centre.
- **Centre connector**: A complex geometric piece bridging both halves. Triangle-inspired design. Likely a different material from the body.
- **End caps**: Separate pieces at each end.
- **LGP (Light Guide Plate)**: The ~51mm wide front display surface with **UV-mapped video overlays** showing real light effects recorded from the actual device. These produce liquid colour gradients radiating from the centre outward.
- **Mounting clips**: Small bracket pieces.

## CRITICAL: Do Not Break Anything

- Do NOT modify any materials, textures, or UV mappings
- Do NOT move, scale, or rotate the K1 model or any of its parts
- Do NOT modify the LGP video textures — they are the entire point
- Do NOT change lighting, HDRI, or emission materials
- Do NOT change the render engine
- Do NOT modify or delete the existing desktop camera

## What to Create: A New Mobile Camera

### Step 1: Create a New Camera

Do NOT modify the existing camera (it produced the desktop render and must remain intact). Instead:

```python
import bpy
import math

# Create a new camera for mobile
mobile_cam_data = bpy.data.cameras.new(name="Camera_Mobile")
mobile_cam_obj = bpy.data.objects.new("Camera_Mobile", mobile_cam_data)
bpy.context.scene.collection.objects.link(mobile_cam_obj)

# Set as active camera for this render
bpy.context.scene.camera = mobile_cam_obj
```

### Step 2: Frame the K1 for Portrait

The K1 is a horizontal bar. On a portrait canvas, a straight-on front shot would make it a thin sliver with vast empty space above and below. That's weak. Instead, frame it with **a subtle 3/4 elevated perspective** — the camera slightly above and angled down, showing the top surface and the glowing LGP front face. This gives the device physical presence and depth on a tall canvas.

**Camera positioning strategy:**

1. Find the K1's bounding box centre (the model's world-space centre)
2. Position the new camera:
   - Slightly above the K1 (elevated by roughly 30-40% of the device's length)
   - Slightly in front (angled to see the LGP face clearly — the LGP must be prominently visible)
   - The K1 should fill roughly 60-70% of the frame width
   - The ambient light spill below the device should be visible (it's a key visual feature — light bleeds downward from the LGP)
3. Aim the camera at the K1's centre
4. Focal length: 50-65mm (avoid wide-angle distortion on a product shot)

```python
import bpy
import math
from mathutils import Vector

# --- Find the K1's centre ---
# Look for the main body mesh objects (likely the largest meshes in the scene)
# The K1 body, LGP, connectors, and endcaps are all near the scene origin
# If you can't identify the exact centre, use the scene's geometry bounding box

# Collect all mesh objects to find the scene's centre of mass
all_meshes = [obj for obj in bpy.data.objects if obj.type == 'MESH']
if all_meshes:
    # Average of all mesh origins as rough centre
    centre = sum((obj.location for obj in all_meshes), Vector((0, 0, 0))) / len(all_meshes)
else:
    centre = Vector((0, 0, 0))

print(f"Estimated K1 centre: {centre}")

# --- Position the mobile camera ---
# These are STARTING VALUES — you MUST adjust based on what you see in the viewport.
# The goal: K1 fills 60-70% of frame width, LGP face clearly visible, slight top-down angle.

cam = bpy.data.objects.get("Camera_Mobile")
if cam:
    # Position: above and in front of the K1
    # Adjust these values after checking the viewport!
    cam.location = (centre.x, centre.y - 0.25, centre.z + 0.15)

    # Point at K1 centre using a Track To constraint
    track = cam.constraints.new('TRACK_TO')
    track.target = all_meshes[0]  # or create an empty at 'centre'
    track.track_axis = 'TRACK_NEGATIVE_Z'
    track.up_axis = 'UP_Y'

    # Focal length
    cam.data.lens = 58  # mm — adjust for desired framing
```

**IMPORTANT:** The values above are rough starting points. You MUST:
1. Check the camera view in the viewport (Numpad 0)
2. Adjust position and focal length until the K1 looks right
3. The LGP (glowing front face) must be clearly visible and prominent
4. Leave breathing room — don't crop the device edges
5. The ambient spill (light bleeding below the LGP) should be visible in the lower portion of the frame

### Step 3: Handle Animation

Check if the existing desktop camera has animation keyframes (it may orbit or move during the animation). Two options:

- **If the desktop camera is static (no keyframes):** Your mobile camera can also be static. A locked-off product shot with only the LGP colours changing is perfectly fine.
- **If the desktop camera is animated (orbiting, dolly, etc.):** Replicate a similar but gentler motion on the mobile camera. Keep any movement slow and subtle — mobile screens are small and fast camera motion is disorienting.

If replicating animation is complex, a static mobile camera is the safe default. The LGP light effects provide all the visual motion needed.

## World Background

Should already be `#0A0A0C` from the desktop render. Verify, and if not, apply:

```python
import bpy

world = bpy.context.scene.world
if world is None:
    world = bpy.data.worlds.new("World")
    bpy.context.scene.world = world

world.use_nodes = True
bg_node = world.node_tree.nodes.get("Background")
if bg_node:
    # sRGB #0A0A0C → linear RGB
    bg_node.inputs['Color'].default_value = (0.003035, 0.003035, 0.003564, 1.0)
    bg_node.inputs['Strength'].default_value = 1.0
```

## Render Output Settings

```python
import bpy

scene = bpy.context.scene

# Ensure mobile camera is active
scene.camera = bpy.data.objects.get("Camera_Mobile")

# Output format
scene.render.image_settings.file_format = 'FFMPEG'
scene.render.ffmpeg.format = 'MPEG4'
scene.render.ffmpeg.codec = 'H264'
scene.render.ffmpeg.constant_rate_factor = 'MEDIUM'
scene.render.ffmpeg.ffmpeg_preset = 'GOOD'
scene.render.ffmpeg.gopsize = 18
scene.render.ffmpeg.audio_codec = 'NONE'

# MOBILE RESOLUTION: 1080 × 1920 (9:16 portrait)
scene.render.resolution_x = 1080
scene.render.resolution_y = 1920
scene.render.resolution_percentage = 100

# Frame rate — match the desktop render (likely 30fps, check first)
# scene.render.fps = <match existing>

# Output path
scene.render.filepath = '//k1-animation-dark-bg-mobile.mp4'

# Background baked in, not transparent
scene.render.film_transparent = False
```

**Output location:** Save to `/Users/spectrasynq/SpectraSynq.LandingPage/Blender/k1-animation-dark-bg-mobile.mp4`

## Pre-render Checklist

1. [ ] New "Camera_Mobile" camera exists and is set as active scene camera
2. [ ] Original desktop camera is UNTOUCHED
3. [ ] Resolution is 1080×1920 (9:16 portrait)
4. [ ] K1 fills ~60-70% of frame width in camera view
5. [ ] LGP (glowing display face) is clearly visible and prominent
6. [ ] Ambient light spill below the device is visible
7. [ ] Device is not cropped at the edges
8. [ ] World background is `#0A0A0C`
9. [ ] LGP video textures are connected and paths resolve
10. [ ] `film_transparent` is OFF
11. [ ] Output is H.264 MP4, no audio
12. [ ] Frame range matches the desktop render
13. [ ] No objects, materials, or textures have been modified

## After Rendering

1. Verify the LGP shows colour effects throughout the animation
2. Confirm the background is solid near-black with no banding
3. Confirm the K1 framing looks good on a portrait canvas — not too small, not cropped
4. **CRITICAL: Restore the original desktop camera as the active scene camera before saving the .blend file** (so the desktop render isn't broken if someone hits render without checking):

```python
# Restore original camera (find it by name — likely "Camera" or similar)
original_cam = None
for obj in bpy.data.objects:
    if obj.type == 'CAMERA' and obj.name != 'Camera_Mobile':
        original_cam = obj
        break
if original_cam:
    bpy.context.scene.camera = original_cam
```

5. Report: file location, duration, resolution, file size, frame count, and a description of how the K1 is framed (angle, what's visible)

## What NOT to Do

- Do NOT modify or delete the existing desktop camera
- Do NOT modify any materials, textures, or UV mappings
- Do NOT move, scale, or rotate any scene objects
- Do NOT change lighting or HDRI
- Do NOT change the render engine
- Do NOT enable film transparency
- Do NOT re-encode or compress the output after rendering
- Do NOT leave Camera_Mobile as the active camera when saving the .blend file
