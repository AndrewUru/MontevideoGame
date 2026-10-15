"""Read-only audit of the authorized map. Never saves assets or creates actors."""
import json
from pathlib import Path
import unreal

asset='/Game/Montevideo/Maps/MVD_Prototype'
world=unreal.EditorLoadingAndSavingUtils.load_map(asset)
if not world:
    raise RuntimeError('Cannot load prototype world')
settings=world.get_world_settings()
mode=settings.get_editor_property('default_game_mode')
actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
editable=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
report=dict(map=world.get_path_name(),game_mode=mode.get_path_name() if mode else None,
            actors=[dict(name=a.get_name(),type=a.get_class().get_name()) for a in actors],
            editable_actors=[dict(name=a.get_name(),type=a.get_class().get_name()) for a in editable])
# GameplayStatics includes engine managers instantiated while loading the world.
# Do not mistake these for authored GIS actors persisted in the map.
internal_types={'WorldSettings','Brush','DefaultPhysicsVolume','BuoyancyManager',
                'GameplayDebuggerPlayerManager','AbstractNavData'}
if report['editable_actors'] or any(a['type'] not in internal_types for a in report['actors']):
    raise RuntimeError('Unexpected actor in minimal loaded world: '+str(report))
path=Path(unreal.Paths.project_dir())/'Saved/M3_2/world-audit.json'
path.parent.mkdir(parents=True,exist_ok=True)
path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
unreal.log('M3.1 WORLD AUDIT '+json.dumps(report))
