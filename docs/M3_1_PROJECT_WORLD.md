# M3.1 — Dedicated Montevideo Prototype World

M3 fue validado visualmente por el usuario. La autorización M3.1 permite crear exclusivamente `Content/Montevideo/Maps/MVD_Prototype.umap`. El mapa del Engine y los 753 assets anteriores se conservan.

## Creación y configuración

Mapa Unreal: `/Game/Montevideo/Maps/MVD_Prototype.MVD_Prototype`.

Se creó mediante `Scripts/unreal/create_prototype_world.py`, ejecutado con el `UnrealEditor-Cmd.exe` de la instalación descubierta por EngineAssociation y registro: UE **5.8.3 CL58210709**. Usa `LevelEditorSubsystem.new_level(..., is_partitioned_world=False)` y `save_current_level`, no escritura manual de binarios. El script rechaza sobrescribir un mapa existente; **no debe ejecutarse de nuevo para abrirlo**.

Único cambio de Config, en `Config/DefaultEngine.ini`, sección `[/Script/EngineSettings.GameMapsSettings]`:

```ini
GameDefaultMap=/Game/Montevideo/Maps/MVD_Prototype.MVD_Prototype
EditorStartupMap=/Game/Montevideo/Maps/MVD_Prototype.MVD_Prototype
```

`GlobalDefaultGameMode=/Script/MontevideoGame.MontevideoBlockoutGameMode` se conserva. El mapa también declara ese GameMode en WorldSettings. El override del mapa Third Person permanece intacto.

## Contenedor mínimo y runtime

No hay edificios, plaza, manzanas, vías, materiales nuevos ni iluminación colocados manualmente. La inspección de solo lectura `Scripts/unreal/inspect_prototype_world.py` enumera los actores internos de Unreal en `Saved/M3_2/world-audit.json`.

Al pulsar Play, `AMontevideoBlockoutGameMode` crea el actor cartográfico M3, el actor agregado de edificios, PlayerStart, luz direccional, SkyLight, SkyAtmosphere y volumen de exposición de debug. Reutiliza el Character y PlayerController Third Person. Antes de Play el mapa mínimo puede verse vacío; no implica que falten los datos.

La creación de edificios no coloca actores persistentes ni cambia el sistema geográfico. Las alturas de edificios son sintéticas; el suelo continúa en Z=0 de desarrollo.

## Asset autorizado y Git LFS

Único asset nuevo: `Content/Montevideo/Maps/MVD_Prototype.umap`, **6.605 bytes**.

SHA-256: `3e4b66253e8e90afc5b4c24c7bd53f19d1bc0730fb1e206c8b290230338578da`.

`.gitattributes` ya aplica `filter=lfs diff=lfs merge=lfs -text` a `.umap`. El índice contiene un pointer LFS de 129 bytes con ese OID y tamaño; `git lfs ls-files` identifica el mapa y su contenido local coincide con el hash. No se cambiaron reglas LFS ni assets anteriores.

## Abrir y validar

1. Abrir `MontevideoGame.uproject` con UE 5.8.3 tras compilar, reiniciando el Editor si estaba abierto.
2. Si restaura otro nivel, abrir Content Browser → **Content / Montevideo / Maps / MVD_Prototype**. Ya no es necesario activar Engine Content ni abrir Entry.
3. Play → Selected Viewport. Comprobar personaje, plaza verde, manzanas azul/gris, vías amarillas y edificios beige.
4. Ejecutar `Montevideo.M3.Status`, `Montevideo.Buildings.Status` y `Montevideo.Buildings.Bounds` en consola. Ver [M3.2](M3_2_BUILDING_GENERATOR.md) para inspección de edificios y colisión.
5. Detener Play retira la geometría runtime. No usar “Keep Simulation Changes” ni guardar geometría del PIE en el mapa.

La aceptación visual de M3.2 corresponde al usuario; no se avanza a M3.3/M4.
