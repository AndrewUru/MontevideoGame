# Arquitectura inicial

## Alcance y principios
M1 consolida el runtime C++ del template Third Person en Unreal Engine 5.8.3. Python queda reservado para herramientas futuras; VS Code es el editor de trabajo. Inventario, toolchain y evidencias: [M1_BASELINE.md](M1_BASELINE.md). Los dominios futuros de este documento siguen siendo propuestas.

C++ es la fuente de verdad del gameplay. Blueprints podrán configurar y presentar sistemas C++, sin contener por sí solos reglas críticas. Los límites siguientes son módulos conceptuales, no una orden de generar un módulo Unreal por cada fila. Su distribución física se decidirá conforme aparezcan necesidades reales de compilación y reutilización.

## Módulos conceptuales
| Módulo | Responsabilidad | Límites y contratos |
| --- | --- | --- |
| World | Sectores, ciclo de vida del mundo, integración con World Partition | Consume datos locales convertidos; no contiene lógica de tráfico ni misiones. |
| Player | Personaje, control, cámara y locomoción | Emite intenciones de interacción; no gobierna vehículos o policía. |
| Vehicles | Movimiento, ocupantes y estado de vehículos | Expone control y ocupación tanto a jugador como a controladores de IA. |
| Traffic | Rutas, demanda y simulación de circulación | Consume un grafo vial y contratos de Vehicles; no conoce reglas de misiones. |
| Pedestrians | Población, desplazamiento y comportamiento peatonal | Consume navegación y eventos del mundo; no dirige Traffic. |
| Police | Respuesta policial y asignación de unidades | Consume nivel de búsqueda y observaciones; no decide progreso de misiones. |
| Missions | Objetivos, progreso y condiciones | Consume eventos de dominio y solicita acciones mediante interfaces. |
| Interaction | Descubrimiento de acciones, elegibilidad y ejecución | Contratos comunes para entidades interactuables; sin reglas particulares de cada misión. |
| Geospatial | Referencias espaciales, conversión y validación | Única frontera entre datos geográficos y posiciones locales del juego. |
| UI | Presentación, HUD y entrada de menús | Lee modelos de vista y envía comandos; no es dueña del estado de gameplay. |
| SaveGame | Persistencia y migración de estado | Guarda IDs y datos versionados; no referencias a actores que deban estar cargados. |

Wanted es inicialmente una responsabilidad diferenciada dentro del dominio Police, con contrato propio; puede extraerse si su reutilización lo justifica.

## Comunicación y propiedad
Cada sistema es dueño de su estado y publica eventos o consultas mediante interfaces C++ pequeñas. Los eventos incluyen identificadores estables y datos mínimos. Las suscripciones se liberan al terminar su ciclo de vida. Evitar cadenas de casts, singletons globales y referencias fuertes entre sectores.

Ejemplo futuro: Interaction solicita ocupar un vehículo; Vehicles valida y emite el resultado; Player cambia la posesión únicamente si la solicitud fue aceptada. Un incidente reportado actualiza Wanted; Police decide su respuesta y Missions puede observar el evento sin que estos sistemas se llamen mutuamente de forma circular.

Los subsistemas de simulación se proponen ligados al mundo (`UWorldSubsystem` o variante adecuada cuando haya actualización periódica). SaveGame coordina datos duraderos; no debe preservar instancias de simulación entre mundos. El modelo de red queda sin definir: no se promete multijugador en el primer slice.

## Mundo y escala
World Partition se contempla desde el primer mapa, con una zona inicial limitada. El tamaño de celdas, distancias de carga, HLOD y presupuestos de actores se medirán con el hardware objetivo antes de fijarse. No cargar Montevideo completa ni el grafo urbano completo como actores.

Separar datos persistentes, simulación abstracta y actores activos. Tráfico y peatones tienen cupos por zona y niveles de simulación por distancia; descargar una celda no debe perder progreso de misión o duplicar entidades. Usar IDs estables para reconstruir estado al volver a cargar. La navegación y colisión deberán validarse en fronteras de sectores.

## Frontera geográfica
Geospatial declara CRS de entrada, CRS métrico de trabajo, origen local, unidades, ejes y referencia vertical. Las transformaciones se realizan con precisión adecuada antes de producir posiciones Unreal. No tratar grados como metros o centímetros. World Partition resuelve streaming; no reemplaza la conversión de coordenadas. Ver `MAP_PIPELINE.md`.

### Decisión M2 (antes de implementar)
M2 autoriza únicamente infraestructura geoespacial. `UGeospatialSubsystem` deriva de `UGameInstanceSubsystem`: el marco geográfico debe sobrevivir a cambios de mapa, sin contener actores ni simulación del mundo. Cada GameInstance/PIE tiene su propio origen. El cálculo reutilizable vive en `FGeoReference`, independiente de GameInstance, mapas y gameplay, dentro del módulo existente. No requiere nuevos módulos, plugins ni bibliotecas.

Se adopta WGS84 geodésico 3D (grados y altura elipsoidal en metros), conversión a ECEF y rotación a plano tangente con X norte, Y este, Z arriba; salida en centímetros y doble precisión. El dominio M2 se limita explícitamente a 20 km de distancia tridimensional al origen y alturas de -1000 a 10000 m. No equivale a terreno plano ni a una proyección cartográfica global. El origen comienza sin configurar; no se asignan coordenadas supuestas a Plaza Independencia. Cambiar origen invalida las posiciones locales calculadas anteriormente; no desplaza actores ni modifica World Partition. Detalle y pruebas en `M2_GEOSPATIAL.md`.

## Decisiones y pendientes
- Sin Cesium ni dependencias externas por ahora; se evaluarán solo frente a necesidades concretas.
- Sin importador runtime/editor todavía. Su implementación futura separará procesamiento Python, datos de intercambio e integración de editor de Unreal.
- Procedencia y licencias acompañan a los derivados; los binarios Unreal no se editan automáticamente.
- Pendientes: extensión exacta del slice, CRS, origen, elevación, hardware objetivo, presupuestos de rendimiento y formato de persistencia.
- Toda nueva dependencia deberá documentar necesidad, alternativas, licencia, mantenimiento e impacto antes de incorporarse.

## Decisiones de M1
- Un único módulo runtime `MontevideoGame`; targets Game y Editor existentes. No se crean módulos ni plugins.
- Se evolucionan `AMontevideoGameCharacter`, `AMontevideoGamePlayerController` y `AMontevideoGameGameMode`. Mantener nombres, propiedades, funciones y subobjetos serializados evita migrar Blueprints. Los redirects `TP_ThirdPerson` de Config son parte de la compatibilidad.
- GameMode configura en C++ los dos Blueprints existentes de Pawn y Controller mediante rutas de contenido estables. Esta dependencia de presentación es deliberada: conserva malla, animación, Input Actions, contextos y UI táctil sin reescribir assets. Los Blueprints pueden sobrescribir configuración; no implementan en exclusiva locomoción ni cámara.
- Character conserva movimiento/cámara y concentra entrada de teclado, mando y UI en `DoMove`, `DoLook`, `DoJumpStart` y `DoJumpEnd`. Controller administra contextos locales y el ciclo de vida de la UI táctil; no se añaden sistemas de interacción/HUD/vehículos.
- Build.cs conserva Core, CoreUObject, Engine, InputCore, EnhancedInput, AIModule, StateTreeModule, GameplayStateTreeModule y UMG. Las variantes originales consumen IA/StateTree y widgets; no son dependencias prescindibles mientras esas fuentes y assets permanezcan. Slate pasa a dependencia privada, usado por `SVirtualJoystick` en los `.cpp`. No se incorpora ninguna dependencia nueva.
- Las variantes originales no forman parte del gameplay M1, pero siguen compilándose y se preservan para no romper el template. Su retirada o separación futura requiere revisar sus assets y referencias.
