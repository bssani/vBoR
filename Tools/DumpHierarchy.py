# Dump selected Outliner hierarchy to Saved/vBoR_hierarchy.txt (Company-PC-Guide §4.1)
# Names, attachment, transforms, mesh bounds, collision only. No geometry
# Usage: select top nodes of moving parts in Outliner, then in Output Log (Python):
#   <repo>/Tools/DumpHierarchy.py
# Far bounds_center = mesh pivot at car origin. Near = own pivot + Location
import unreal

OUT = unreal.Paths.project_saved_dir() + "vBoR_hierarchy.txt"
actor_sys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sm_sys = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
lines = []

def v(x): return "(%.1f, %.1f, %.1f)" % (x.x, x.y, x.z)
def r(x): return "(P%.1f Y%.1f R%.1f)" % (x.pitch, x.yaw, x.roll)

def mesh_info(comp):
    mesh = comp.static_mesh
    if not mesh:
        return "  mesh=None"
    box = mesh.get_bounding_box()
    center = (box.min + box.max) * 0.5
    size = box.max - box.min
    try: simple = sm_sys.get_simple_collision_count(mesh)
    except Exception: simple = "?"
    try: trace = str(mesh.get_editor_property("body_setup").get_editor_property("collision_trace_flag")).split(".")[-1]
    except Exception: trace = "?"
    return "  mesh=%s  bounds_center=%s  size=%s  simple_collision=%s  trace=%s" % (
        mesh.get_name(), v(center), v(size), simple, trace)

def walk(comp, depth, prev_owner):
    owner = comp.get_owner()
    if owner != prev_owner:
        lines.append("  " * depth + "[A] %s <%s> folder=%s" % (
            owner.get_actor_label(), owner.get_class().get_name(), owner.get_folder_path()))
        depth += 1
    line = "  " * depth + "[C] %s <%s> rel=%s %s scale=%s world=%s" % (
        comp.get_name(), comp.get_class().get_name(),
        v(comp.get_editor_property("relative_location")), r(comp.get_editor_property("relative_rotation")),
        v(comp.get_editor_property("relative_scale3d")), v(comp.get_world_location()))
    if isinstance(comp, unreal.StaticMeshComponent):
        line += mesh_info(comp)
    lines.append(line)
    for child in comp.get_children_components(False):
        walk(child, depth + 1, owner)

selected = actor_sys.get_selected_level_actors()
for actor in selected:
    # Child of another selected node is printed under its parent (avoid duplicates)
    if actor.get_attach_parent_actor() in selected:
        continue
    walk(actor.root_component, 0, None)

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
unreal.log("vBoR hierarchy: %d lines -> %s" % (len(lines), OUT))
