"""Run with UnrealEditor-Cmd -run=pythonscript -script=...; creates ONLY the authorized new map."""
from pathlib import Path
import unreal

asset = '/Game/Montevideo/Maps/MVD_Prototype'
filename = Path(unreal.Paths.project_content_dir()) / 'Montevideo/Maps/MVD_Prototype.umap'
if filename.exists() or unreal.EditorAssetLibrary.does_asset_exist(asset):
    raise RuntimeError('Refusing to overwrite an existing map: ' + asset)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.new_level(asset, is_partitioned_world=False):
    raise RuntimeError('Unreal could not create the authorized map')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.load_class(None, '/Script/MontevideoGame.MontevideoBlockoutGameMode'))
if not levels.save_current_level():
    raise RuntimeError('Unreal could not save the new map')
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    unreal.log('M3.1 SAVED ACTOR ' + actor.get_class().get_name() + ' ' + actor.get_name())
unreal.log('M3.1 CREATED ' + str(filename))
