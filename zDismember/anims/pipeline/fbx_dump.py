# Uruchamiane w Blenderze: blender -b --factory-startup -P fbx_dump.py -- wejscie.fbx wyjscie.json
# Zrzuca szkielet (poza spoczynkowa) i macierze swiatowe kosci w kazdej klatce.
import bpy, sys, json

args = sys.argv[sys.argv.index("--") + 1:]
src, dst = args[0], args[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src, automatic_bone_orientation=False)
arm = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
scene = bpy.context.scene
act = arm.animation_data.action if arm.animation_data else None
f0, f1 = (int(act.frame_range[0]), int(act.frame_range[1])) if act else (scene.frame_start, scene.frame_end)

def m(mat):
    return [list(r) for r in mat]

out = {
    "source": src,
    "fps": scene.render.fps / scene.render.fps_base,
    "frame_start": f0, "frame_end": f1,
    "armature_world": m(arm.matrix_world),
    "bones": [{"name": b.name, "parent": b.parent.name if b.parent else None,
               "rest": m(arm.matrix_world @ b.matrix_local)} for b in arm.data.bones],
    "frames": [],
}
names = [b.name for b in arm.data.bones]
for f in range(f0, f1 + 1):
    scene.frame_set(f)
    out["frames"].append([m(arm.matrix_world @ arm.pose.bones[n].matrix) for n in names])
json.dump(out, open(dst, "w"))
print("ZDUMP", src, "bones", len(names), "frames", f0, f1, "fps", out["fps"])
