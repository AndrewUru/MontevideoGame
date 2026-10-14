# Sistemas de gameplay

M1 conserva las clases reales del template: `AMontevideoGameCharacter : ACharacter`, `AMontevideoGamePlayerController : APlayerController` y `AMontevideoGameGameMode : AGameModeBase`. Sus Blueprints derivados configuran presentación y entrada. Character y Controller siguen siendo bases abstractas para esos assets; GameMode es concreto y configura Pawn/Controller. No existe una jerarquía paralela con los nombres abreviados propuestos.

El personaje conserva locomoción analógica (MaxWalkSpeed 500 cm/s), salto, SpringArm de 400 cm y FollowCamera. No se añade sprint ni interacción. Controller conserva Enhanced Input y UI táctil; HUD e intercambio de posesión quedan pendientes. Véase [M1_BASELINE.md](M1_BASELINE.md).

La tabla siguiente mantiene contratos futuros orientativos; no autoriza su implementación. Las reglas de juego serán C++; Blueprints podrán exponer configuración y presentación.

| Clase aproximada | Responsabilidad | Límites |
| --- | --- | --- |
| `AMontevideoCharacter` | Personaje en tercera persona, locomoción a pie, caminar/correr, integración de cámara y animación | No controla tráfico, misiones ni nivel de búsqueda. Solicita acciones mediante Interaction. |
| `AMontevideoPlayerController` | Intenciones del jugador, entrada, posesión y coordinación con UI | Adapta entrada al pawn poseído; no contiene física de vehículos ni reglas de búsqueda. |
| `AMontevideoGameMode` | Reglas de sesión, selección de clases y condiciones de inicio/reaparición | No es un contenedor global de todos los sistemas ni un almacén persistente. |
| `AMontevideoVehicle` | Movimiento y estado del vehículo, ocupantes y puntos de entrada/salida | Expone un contrato de conducción para jugador o IA; no depende de Traffic ni Police. |
| `UVehicleInteractionComponent` | Consultar elegibilidad y coordinar solicitudes de entrar/salir | Valida distancia, asiento disponible y salida segura junto al vehículo; no modifica directamente estado de otros sistemas. |
| `UTrafficSubsystem` | Población activa de vehículos, rutas y coordinación de circulación | Usa grafo vial y contrato de conducción; mantiene cupos por sector, no simula toda la ciudad con actores activos. |
| `UPedestrianSubsystem` | Creación, retirada y coordinación de peatones por zona | Consume navegación; publica observaciones mediante contratos sin depender de Police o Missions. |
| `UPoliceSubsystem` | Asignación de unidades, persecución y respuesta a incidentes | Consume el estado de Wanted; no es dueño del nivel de búsqueda ni del progreso de misiones. |
| `UWantedSubsystem` | Estado de búsqueda, reglas de incremento/reducción y eventos de cambio | Recibe incidentes validados; no crea vehículos policiales ni actualiza widgets directamente. |
| `UMissionSubsystem` | Activación, objetivos, progreso y finalización de misiones | Se suscribe a eventos y persiste IDs/estado; no requiere que todos los actores de una misión estén cargados. |
| `UGeospatialSubsystem` | Contexto geográfico del mundo y conversión entre referencias geográficas y locales | CRS, origen, ejes y unidades explícitos; no descarga datos ni genera edificios durante el gameplay. |

## Ciclos de vida y contratos
Los subsistemas de simulación se proponen con alcance de mundo, revisando su clase base concreta al implementarlos. El cambio o cierre de mundo libera referencias, tareas y suscripciones. El estado que deba sobrevivir se serializa a través de SaveGame, con esquema versionado, sin guardar punteros a actores.

En M2, `UGeospatialSubsystem : UGameInstanceSubsystem` conserva únicamente el marco geográfico durante la sesión, aislado por GameInstance/PIE. No guarda actores ni simula sectores. `FGeoReference` contiene las conversiones reutilizables. M3 añade `FGeospatialBlockoutLoader` y `AGeospatialBlockoutActor` para visualización transitoria, sin introducir reglas de gameplay. `AMontevideoBlockoutGameMode` deriva del GameMode existente, reutiliza los mismos Blueprints de Character/Controller y compone escena, iluminación y PlayerStart de desarrollo. No modifica movimiento, cámara, salto ni Enhanced Input. Véanse `M2_GEOSPATIAL.md` y `M3_MONTEVIDEO_BLOCKOUT.md`.

Las operaciones de interacción devolverán éxito o motivo de rechazo. Vehicles es dueño de la ocupación; el controlador cambia de posesión tras la aceptación y recupera el personaje al salir. La lógica deberá impedir ocupación doble y resolver retirada o descarga del vehículo.

Los eventos de incidentes y objetivos usarán IDs estables, contexto espacial convertido y datos mínimos. Wanted determina la búsqueda; Police determina la respuesta; UI muestra el resultado. Missions observa contratos públicos sin dirigir detalles internos de esos sistemas.

## Validaciones cuando se implementen
- Caminar/correr y posesión de vehículo conservan control y estado coherentes.
- Descargar una celda no deja referencias inválidas ni duplica entidades al recargar.
- Un mismo incidente no se contabiliza dos veces; búsqueda y misiones se prueban sin UI.
- Guardar/cargar restaura progreso con entidades ausentes y esquema versionado.
- Conversión geográfica y streaming se verifican por separado y después juntos.
