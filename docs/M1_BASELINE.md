# M1 — baseline C++ y verificación

## Inspección previa
Inspección realizada el 14 de octubre de 2026 antes de editar código. Git estaba limpio. Se leyeron `.uproject`, todas las clases principales, targets, Build.cs, Config y documentación; se inventariaron clases y referencias binarias de Content sin guardar paquetes. Una ejecución de la baseline con `UnrealEditor-Cmd -game -nullrhi` confirmó las propiedades efectivas mediante `getall` (salida 0).

- Motor asociado por GUID del `.uproject`, resuelto desde `HKCU:\Software\Epic Games\Unreal Engine\Builds`: `C:/Program Files/Epic Games/UE_5.8`. `Engine/Build/Build.version` confirma **5.8.3**, changelist **58210709**.
- Un módulo de proyecto: `MontevideoGame`, Runtime, LoadingPhase Default. Registro con `IMPLEMENT_PRIMARY_GAME_MODULE` en `MontevideoGame.cpp`; log común `LogMontevideoGame`.
- `Source/MontevideoGame/MontevideoGame.Build.cs`: Core/CoreUObject/Engine, InputCore/EnhancedInput, AIModule, StateTreeModule, GameplayStateTreeModule, UMG; Slate privado. IA y StateTree son necesarios por las variantes preexistentes. UMG y Slate también sostienen controles táctiles. No se añaden módulos ni plugins.
- `Source/MontevideoGame.Target.cs`: Game; `Source/MontevideoGameEditor.Target.cs`: Editor. Ambos usan BuildSettings V7, IncludeOrder Unreal5_8 y el mismo módulo. Sin cambios.
- Plugins explícitos preexistentes: ModelingToolsEditorMode (solo Editor), StateTree y GameplayStateTree. Sin cambios en `.uproject`.

## Composición real conservada
Las consultas runtime previas a los cambios confirmaron:

| Elemento | Valor efectivo |
| --- | --- |
| Mapa | `/Game/ThirdPerson/Lvl_ThirdPerson` |
| WorldSettings.DefaultGameMode | `BP_ThirdPersonGameMode_C` |
| GameMode.DefaultPawnClass | `BP_ThirdPersonCharacter_C` |
| GameMode.PlayerControllerClass | `BP_ThirdPersonPlayerController_C` |
| Controller.DefaultMappingContexts | `/Game/Input/IMC_Default` (prioridad C++ 0) |
| Controller.MobileExcludedMappingContexts | `/Game/Input/IMC_MouseLook` (prioridad C++ 0, excluido con touch) |
| Character.JumpAction | `/Game/Input/Actions/IA_Jump` |
| Character.MoveAction | `/Game/Input/Actions/IA_Move` |
| Character.LookAction | `/Game/Input/Actions/IA_Look` |
| Character.MouseLookAction | `/Game/Input/Actions/IA_MouseLook` |

Jerarquía: `ACharacter → AMontevideoGameCharacter → BP_ThirdPersonCharacter`; `APlayerController → AMontevideoGamePlayerController → BP_ThirdPersonPlayerController`; `AGameModeBase → AMontevideoGameGameMode → BP_ThirdPersonGameMode`.

Los tres assets guardan padres originales `/Script/TP_ThirdPerson.TP_ThirdPerson*`; los redirects existentes los resuelven a las clases `MontevideoGame*`. No renombrar esas clases ni retirar redirects sin migrar assets. No se crean clases duplicadas con nombres abreviados.

Character conserva cápsula 42/96 cm, MaxWalkSpeed 500 cm/s, MinAnalogWalkSpeed 20, JumpZVelocity 500, AirControl 0.35, giro orientado al movimiento y RotationRate yaw 500. `CameraBoom` es un SpringArm de 400 cm con control rotation; `FollowCamera` se adjunta a su socket y no usa directamente pawn control rotation. Se preservan nombres de subobjetos, propiedades y funciones BlueprintCallable. Los assets del personaje referencian `SKM_Quinn_Simple`, `ABP_Unarmed` y `BPI_TouchInterface`; Controller referencia `UI_TouchSimple`.

El mapa ya usa World Partition y paquetes externos; M1 no cambia su configuración. Mapas existentes: ThirdPerson/Lvl_ThirdPerson, Variant_Combat/Lvl_Combat, Variant_Platforming/Lvl_Platforming y Variant_SideScrolling/Lvl_SideScrolling.

## Cambios de M1
- GameMode deja de ser abstracto y configura en su constructor C++ los Blueprints actuales de Pawn y Controller. Sus rutas son una dependencia explícita del contenido Third Person; fallos de carga emiten diagnóstico. El Blueprint GameMode y el override del mapa siguen vigentes.
- Character llama a la implementación base al configurar input, valida acciones antes de enlazarlas y usa los mismos métodos C++ de salto para hardware y touch. Se libera salto tanto en Completed como en Canceled. Movimiento y cámara no cambian.
- Controller valida contextos y clase de UI táctil antes de utilizarlos y retira su widget en EndPlay. Se mantiene la política de contextos del template.
- Las tres clases exportan `MONTEVIDEOGAME_API`. Se elimina una declaración de log del template sin uso. Slate se limita a dependencia privada y se eliminan comentarios de dependencias opcionales no utilizadas.
- No se implementan HUD, interacción, vehículos ni sistemas futuros. Las variantes originales permanecen intactas; no se consideran sistemas propios de MontevideoGame.

## Config relevante y deuda preexistente
- DefaultEngine.ini selecciona Lvl_ThirdPerson para inicio/editor y BP_ThirdPersonGameMode como GameMode global. Conserva los redirects de módulo y tres clases `TP_ThirdPerson`.
- DefaultInput.ini selecciona `EnhancedPlayerInput` y `EnhancedInputComponent`; DefaultTouchInterface=None, con UI táctil gestionada por C++/UMG.
- DefaultEditor.ini y DefaultEditorPerProjectUserSettings.ini aún apuntan a rutas antiguas `/Game/ThirdPersonCPP`. No gobiernan el mapa efectivo; se dejan como deuda de limpieza.
- DefaultGame.ini conserva ProjectName="Third Person Game Template". DefaultEngine.ini contiene entradas repetidas de RHI/autoexposición y configuración de Android File Server con un token de desarrollo preexistente (no reproducido aquí). No se modifica configuración ajena a M1.
- Las variantes mantienen acoplamiento de compilación a IA/StateTree. Separarlas o retirarlas exige revisión futura de contenido y licencias, no borrar fuentes a ciegas.
- Los contextos aún son propiedad del Controller durante toda la sesión local, como en el template. Antes de posesión de vehículos habrá que definir activación/desactivación por Pawn y viajes de mundo. No se implementa ese sistema en M1.

## Compilación y límites de validación
Toolchain descubierto: Visual Studio Community 18, MSVC 14.51.36260 (carpeta 14.51.36231), Windows SDK 10.0.26100.0 y .NET 10.0 incluido en Unreal.

Comando PowerShell reproducible desde la raíz, resolviendo la instalación asociada en lugar de asumir una ruta:

```powershell
$projectPath = (Resolve-Path ./MontevideoGame.uproject).Path
$association = (Get-Content $projectPath -Raw | ConvertFrom-Json).EngineAssociation
$engineRoot = (Get-ItemProperty 'HKCU:\Software\Epic Games\Unreal Engine\Builds').$association
if (-not $engineRoot) { throw 'Instalación asociada no encontrada; revisar registro/instalación.' }
Get-Content (Join-Path $engineRoot 'Engine/Build/Build.version')
& (Join-Path $engineRoot 'Engine/Build/BatchFiles/Build.bat') MontevideoGameEditor Win64 Development $projectPath -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Build falló: $LASTEXITCODE" }
```

Compilación final: **Result: Succeeded**, código de salida **0**, **172.70 segundos**, siete acciones, con UHT y enlace de `UnrealEditor-MontevideoGame.dll`. No hubo errores de compilación ni fue necesario modificar el motor. Log local: `Saved/M1/build-output.log`.

Warnings de compilación: MSVC 14.51.36260 no es la versión preferida 14.50.35717; C4996 en el header del motor `Engine/Classes/GameFramework/Character.h:798`, por `APawn::GetMovementBase` obsoleto (tres apariciones). Las fuentes del proyecto no llaman a ese método. No se ocultan warnings ni se modifica la instalación.

La ejecución posterior de Lvl_ThirdPerson con `-game -nullrhi` terminó con salida 0 y confirmó GameMode, Pawn poseído, Controller, ambas listas de contextos, las cuatro acciones y un `EnhancedInputComponent` real. Sin errores ni ensures. Reaparecen los 21 warnings de registro de widgets `LogEditorDataStorageUI` ya presentes en la ejecución previa y el aviso de tiempo de `TickDeferredCommands` por las consultas de diagnóstico.

Una segunda ejecución con URL `/Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/MontevideoGame.MontevideoGameGameMode` terminó también con salida 0. Confirmó `Game class is 'MontevideoGameGameMode'`, los mismos dos Blueprints configurados desde C++ y el Pawn poseído; sin errores ni ensures. Esto comprueba que el GameMode nativo funciona sin depender de la configuración exclusiva del Blueprint GameMode. Log: `Saved/Logs/M1-NativeGameMode.log`. Las consultas `getall` se ejecutaron mediante `-ExecCmds` y finalizaron con `quit`; no guardan paquetes.

SHA-256 comparado para **753 paquetes** `.uasset`/`.umap`: ninguno modificado, eliminado ni añadido. Logs locales no versionados: `Saved/Logs/M1-Baseline.log` y `Saved/Logs/M1-After.log`; manifiesto previo en `Saved/M1/asset-hashes-before.json`.

La prueba headless confirma carga y composición, pero no sustituye una prueba visual de movimiento/cámara/salto, mando o touch. La validación manual comunicada por el usuario corresponde a la baseline anterior a estos cambios. Se recomienda repetir PIE antes de autorizar M2; no se probaron packaging ni las variantes. `git diff --check` terminó correctamente. M1 queda completado en el alcance autorizado de consolidación C++; M2 no comienza.

## Inventario de fuentes y dependencias de contenido
Las tablas siguientes registran declaraciones existentes y referencias a padres nativos encontradas en los paquetes. La inspección binaria identifica dependencias; solo la composición Third Person se verificó además mediante ejecución. No se afirma haber probado las variantes.

### Declaraciones C++ existentes

| Archivo | Declaraciones |
| --- | --- |
| `Source/MontevideoGame/MontevideoGameCharacter.h` | `AMontevideoGameCharacter` |
| `Source/MontevideoGame/MontevideoGameGameMode.h` | `AMontevideoGameGameMode` |
| `Source/MontevideoGame/MontevideoGamePlayerController.h` | `AMontevideoGamePlayerController` |
| `Source/MontevideoGame/Variant_Combat/AI/CombatAIController.h` | `ACombatAIController` |
| `Source/MontevideoGame/Variant_Combat/AI/CombatEnemy.h` | `ACombatEnemy` |
| `Source/MontevideoGame/Variant_Combat/AI/CombatEnemySpawner.h` | `ACombatEnemySpawner` |
| `Source/MontevideoGame/Variant_Combat/AI/CombatStateTreeUtility.h` | `FStateTreeCharacterGroundedConditionInstanceData`, `FStateTreeCharacterGroundedCondition`, `FStateTreeIsInDangerConditionInstanceData`, `FStateTreeIsInDangerCondition`, `FStateTreeAttackInstanceData`, `FStateTreeComboAttackTask`, `FStateTreeChargedAttackTask`, `FStateTreeWaitForLandingTask`, `FStateTreeFaceActorInstanceData`, `FStateTreeFaceActorTask`, `FStateTreeFaceLocationInstanceData`, `FStateTreeFaceLocationTask`, `FStateTreeSetCharacterSpeedInstanceData`, `FStateTreeSetCharacterSpeedTask`, `FStateTreeGetPlayerInfoInstanceData`, `FStateTreeGetPlayerInfoTask` |
| `Source/MontevideoGame/Variant_Combat/AI/EnvQueryContext_Danger.h` | `UEnvQueryContext_Danger` |
| `Source/MontevideoGame/Variant_Combat/AI/EnvQueryContext_Player.h` | `UEnvQueryContext_Player` |
| `Source/MontevideoGame/Variant_Combat/Animation/AnimNotify_CheckChargedAttack.h` | `UAnimNotify_CheckChargedAttack` |
| `Source/MontevideoGame/Variant_Combat/Animation/AnimNotify_CheckCombo.h` | `UAnimNotify_CheckCombo` |
| `Source/MontevideoGame/Variant_Combat/Animation/AnimNotify_DoAttackTrace.h` | `UAnimNotify_DoAttackTrace` |
| `Source/MontevideoGame/Variant_Combat/CombatCharacter.h` | `ACombatCharacter` |
| `Source/MontevideoGame/Variant_Combat/CombatGameMode.h` | `ACombatGameMode` |
| `Source/MontevideoGame/Variant_Combat/CombatPlayerController.h` | `ACombatPlayerController` |
| `Source/MontevideoGame/Variant_Combat/Gameplay/CombatActivationVolume.h` | `ACombatActivationVolume` |
| `Source/MontevideoGame/Variant_Combat/Gameplay/CombatCheckpointVolume.h` | `ACombatCheckpointVolume` |
| `Source/MontevideoGame/Variant_Combat/Gameplay/CombatDamageableBox.h` | `ACombatDamageableBox` |
| `Source/MontevideoGame/Variant_Combat/Gameplay/CombatDummy.h` | `ACombatDummy` |
| `Source/MontevideoGame/Variant_Combat/Gameplay/CombatLavaFloor.h` | `ACombatLavaFloor` |
| `Source/MontevideoGame/Variant_Combat/Interfaces/CombatActivatable.h` | `UCombatActivatable`, `ICombatActivatable` |
| `Source/MontevideoGame/Variant_Combat/Interfaces/CombatAttacker.h` | `UCombatAttacker`, `ICombatAttacker` |
| `Source/MontevideoGame/Variant_Combat/Interfaces/CombatDamageable.h` | `UCombatDamageable`, `ICombatDamageable` |
| `Source/MontevideoGame/Variant_Combat/UI/CombatLifeBar.h` | `UCombatLifeBar` |
| `Source/MontevideoGame/Variant_Platforming/Animation/AnimNotify_EndDash.h` | `UAnimNotify_EndDash` |
| `Source/MontevideoGame/Variant_Platforming/PlatformingCharacter.h` | `APlatformingCharacter` |
| `Source/MontevideoGame/Variant_Platforming/PlatformingGameMode.h` | `APlatformingGameMode` |
| `Source/MontevideoGame/Variant_Platforming/PlatformingPlayerController.h` | `APlatformingPlayerController` |
| `Source/MontevideoGame/Variant_SideScrolling/AI/SideScrollingAIController.h` | `ASideScrollingAIController` |
| `Source/MontevideoGame/Variant_SideScrolling/AI/SideScrollingNPC.h` | `ASideScrollingNPC` |
| `Source/MontevideoGame/Variant_SideScrolling/AI/SideScrollingStateTreeUtility.h` | `FStateTreeGetPlayerInstanceData`, `FStateTreeGetPlayerTask` |
| `Source/MontevideoGame/Variant_SideScrolling/Gameplay/SideScrollingJumpPad.h` | `ASideScrollingJumpPad` |
| `Source/MontevideoGame/Variant_SideScrolling/Gameplay/SideScrollingMovingPlatform.h` | `ASideScrollingMovingPlatform` |
| `Source/MontevideoGame/Variant_SideScrolling/Gameplay/SideScrollingPickup.h` | `ASideScrollingPickup` |
| `Source/MontevideoGame/Variant_SideScrolling/Gameplay/SideScrollingSoftPlatform.h` | `ASideScrollingSoftPlatform` |
| `Source/MontevideoGame/Variant_SideScrolling/Interfaces/SideScrollingInteractable.h` | `USideScrollingInteractable`, `ISideScrollingInteractable` |
| `Source/MontevideoGame/Variant_SideScrolling/SideScrollingCameraManager.h` | `ASideScrollingCameraManager` |
| `Source/MontevideoGame/Variant_SideScrolling/SideScrollingCharacter.h` | `ASideScrollingCharacter` |
| `Source/MontevideoGame/Variant_SideScrolling/SideScrollingGameMode.h` | `ASideScrollingGameMode` |
| `Source/MontevideoGame/Variant_SideScrolling/SideScrollingPlayerController.h` | `ASideScrollingPlayerController` |
| `Source/MontevideoGame/Variant_SideScrolling/UI/SideScrollingUI.h` | `USideScrollingUI` |

### Assets con referencias a padres/clases nativas del template

| Asset | Referencias nativas guardadas (prefijo /Script/TP_ThirdPerson) |
| --- | --- |
| `Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.uasset` | `TP_ThirdPersonCharacter` |
| `Content/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.uasset` | `TP_ThirdPersonGameMode` |
| `Content/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.uasset` | `TP_ThirdPersonPlayerController` |
| `Content/Variant_Combat/Blueprints/AI/BP_CombatAIController.uasset` | `CombatAIController` |
| `Content/Variant_Combat/Blueprints/AI/BP_CombatEnemy.uasset` | `CombatEnemy` |
| `Content/Variant_Combat/Blueprints/AI/BP_CombatEnemySpawner.uasset` | `CombatEnemySpawner` |
| `Content/Variant_Combat/Blueprints/AI/EnvQuery_Evade.uasset` | `EnvQueryContext_Danger`, `EnvQueryContext_Player` |
| `Content/Variant_Combat/Blueprints/BP_CombatCharacter.uasset` | `CombatCharacter` |
| `Content/Variant_Combat/Blueprints/BP_CombatGameMode.uasset` | `CombatGameMode` |
| `Content/Variant_Combat/Blueprints/BP_CombatPlayerController.uasset` | `CombatPlayerController` |
| `Content/Variant_Combat/Blueprints/Interactables/BP_CombatActivationVolume.uasset` | `CombatActivationVolume` |
| `Content/Variant_Combat/Blueprints/Interactables/BP_CombatCheckpointVolume.uasset` | `CombatCheckpointVolume` |
| `Content/Variant_Combat/Blueprints/Interactables/BP_CombatDamageableBox.uasset` | `CombatDamageableBox` |
| `Content/Variant_Combat/Blueprints/Interactables/BP_CombatDummy.uasset` | `CombatDummy` |
| `Content/Variant_Combat/Blueprints/Interactables/BP_CombatLavaFloor.uasset` | `CombatLavaFloor` |
| `Content/Variant_Combat/UI/UI_LifeBar.uasset` | `CombatLifeBar` |
| `Content/Variant_Platforming/Blueprints/BP_PlatformingCharacter.uasset` | `PlatformingCharacter` |
| `Content/Variant_Platforming/Blueprints/BP_PlatformingGameMode.uasset` | `PlatformingGameMode` |
| `Content/Variant_Platforming/Blueprints/BP_PlatformingPlayerController.uasset` | `PlatformingPlayerController` |
| `Content/Variant_SideScrolling/Blueprints/AI/BP_SideScrollingAIController.uasset` | `SideScrollingAIController` |
| `Content/Variant_SideScrolling/Blueprints/AI/BP_SideScrollingNPC.uasset` | `SideScrollingNPC` |
| `Content/Variant_SideScrolling/Blueprints/BP_SideScrollingCameraManager.uasset` | `SideScrollingCameraManager` |
| `Content/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.uasset` | `SideScrollingCharacter` |
| `Content/Variant_SideScrolling/Blueprints/BP_SideScrollingGameMode.uasset` | `SideScrollingGameMode` |
| `Content/Variant_SideScrolling/Blueprints/BP_SideScrollingPlayerController.uasset` | `SideScrollingPlayerController` |
| `Content/Variant_SideScrolling/Blueprints/Items/BP_SideScrollingMovingPlatform.uasset` | `SideScrollingMovingPlatform` |
| `Content/Variant_SideScrolling/Blueprints/Items/BP_SideScrollingPickup.uasset` | `SideScrollingPickup` |
| `Content/Variant_SideScrolling/Blueprints/Items/BP_SideScrollingSoftPlatform.uasset` | `SideScrollingSoftPlatform` |
| `Content/Variant_SideScrolling/UI/UI_SideScrolling.uasset` | `SideScrollingUI` |
