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
report=dict(map=world.get_path_name(),game_mode=mode.get_path_name() if mode else None,
            actors=[dict(name=a.get_name(),type=a.get_class().get_name()) for a in actors])
if any(a['type'] not in ['WorldSettings','Brush','DefaultPhysicsVolume'] for a in report['actors']):
    raise RuntimeError('Unexpected persisted actor in minimal world: '+str(report))
path=Path(unreal.Paths.project_dir())/'Saved/M3_2/world-audit.json'
path.parent.mkdir(parents=True,exist_ok=True)
path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
unreal.log('M3.1 WORLD AUDIT '+json.dumps(report))
