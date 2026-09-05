import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Maps/L_TestMatch')
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor, (unreal.StaticMeshActor, unreal.NavMeshBoundsVolume)):
        unreal.log('EF_ARENA {} {} loc={} scale={} bounds={}'.format(actor.get_actor_label(), actor.get_class().get_name(), actor.get_actor_location(), actor.get_actor_scale3d(), actor.get_actor_bounds(False)))
