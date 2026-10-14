# Sistemas de gameplay futuros

Contratos propuestos, sin declaraciones ni implementaciones C++ en M0. Los nombres son orientativos y se ajustarán al crear el proyecto. Las reglas de juego serán C++; Blueprints podrán exponer configuración y presentación.

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

Geospatial podrá compartir una biblioteca C++ de conversión con herramientas de editor; el importador no dependerá de una instancia de gameplay para hacer sus transformaciones. Python y C++ deberán usar la misma definición documentada de referencias y validar resultados contra puntos de control comunes.

Las operaciones de interacción devolverán éxito o motivo de rechazo. Vehicles es dueño de la ocupación; el controlador cambia de posesión tras la aceptación y recupera el personaje al salir. La lógica deberá impedir ocupación doble y resolver retirada o descarga del vehículo.

Los eventos de incidentes y objetivos usarán IDs estables, contexto espacial convertido y datos mínimos. Wanted determina la búsqueda; Police determina la respuesta; UI muestra el resultado. Missions observa contratos públicos sin dirigir detalles internos de esos sistemas.

## Validaciones cuando se implementen
- Caminar/correr y posesión de vehículo conservan control y estado coherentes.
- Descargar una celda no deja referencias inválidas ni duplica entidades al recargar.
- Un mismo incidente no se contabiliza dos veces; búsqueda y misiones se prueban sin UI.
- Guardar/cargar restaura progreso con entidades ausentes y esquema versionado.
- Conversión geográfica y streaming se verifican por separado y después juntos.
