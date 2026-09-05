"""Editor-only authoring. Creates real assets using Unreal APIs; safe to rerun."""
import unreal

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/L_TestMatch')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
cube = unreal.load_asset('/Engine/BasicShapes/Cube')

def material(name, color):
    path = '/Game/Prototype/Siege/' + name
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = tools.create_asset(name, '/Game/Prototype/Siege', unreal.Material, unreal.MaterialFactoryNew())
    node = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
    node.set_editor_property('constant', unreal.LinearColor(*color, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(node, '', unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assets.save_loaded_asset(mat)
    return mat

road = material('M_SiegeRoad', (0.22, 0.19, 0.15))
water = material('M_RiverMarker', (0.025, 0.2, 0.35))
bridge = material('M_SiegeBridge', (0.4, 0.37, 0.28))
existing = {a.get_actor_label(): a for a in actors.get_all_level_actors()}

# These three blockers belonged to the M0 pathfinding smoke test. They are not
# part of the lane design and make the first creep clash look like a navigation
# failure, so the siege arena deliberately removes them.
for obsolete_label in (
    'EF_PathObstacle_Center',
    'EF_PathObstacle_North',
    'EF_PathObstacle_South',
):
    obsolete_actor = existing.pop(obsolete_label, None)
    if obsolete_actor is not None and not actors.destroy_actor(obsolete_actor):
        raise RuntimeError('Failed to remove obsolete actor: ' + obsolete_label)

def box(label, location, scale, mat, collision=False):
    actor = existing.get(label)
    if actor is None:
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
        actor.set_actor_label(label)
    actor.set_actor_location(unreal.Vector(*location), False, False)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    mesh = actor.static_mesh_component
    mesh.set_static_mesh(cube)
    mesh.set_material(0, mat)
    mesh.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    actor.set_editor_property('is_spatially_loaded', False)
    return actor

box('EF_SiegeLane', (0, 0, 15), (145, 18, 0.2), road, True)
box('EF_RiverNorth', (0, 650, 27), (5, 9, 0.04), water)
box('EF_RiverSouth', (0, -650, 27), (5, 9, 0.04), water)
box('EF_CentralBridge', (0, 0, 28), (6, 4, 0.04), bridge)

nav = existing.get('EF_NavMeshBounds_TestMatch')
if nav is None:
    raise RuntimeError('Expected existing navigation bounds; map left unsaved')
# Transform-only edits to an external World Partition actor are not always
# dirtied by commandlet Python. Explicitly transact and dirty the actor package
# so it is included by save_dirty_packages.
nav.modify()
nav.set_actor_scale3d(unreal.Vector(80, 60, 8))
# The match starts near the two bases, while this World Partition actor is
# centred on the river. Keep it loaded so PIE cannot stream the navigation
# bounds out before the server creates heroes and the first creep wave.
nav.set_editor_property('is_spatially_loaded', False)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
saved_nav_scale = nav.get_actor_scale3d()
if abs(saved_nav_scale.x - 80.0) > 0.01:
    raise RuntimeError('Navigation bounds scale was not applied: {}'.format(saved_nav_scale))
unreal.log('EF_SIEGE_ARENA_SAVED: lane +/-7250, navigation +/-8000, obsolete blockers removed')
