# Arquitectura inicial

## Alcance y principios
Arquitectura propuesta, sin implementación en M0. Unreal Engine 5 y C++ gobiernan el runtime; Python prepara datos fuera del juego. VS Code es el editor previsto. La versión menor de UE5, el compilador y su instalación quedan pendientes de verificación en M1.

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

## Decisiones y pendientes
- Sin Cesium ni dependencias externas por ahora; se evaluarán solo frente a necesidades concretas.
- Sin importador runtime/editor todavía. Su implementación futura separará procesamiento Python, datos de intercambio e integración de editor de Unreal.
- Procedencia y licencias acompañan a los derivados; los binarios Unreal no se editan automáticamente.
- Pendientes: versión UE5, extensión exacta del slice, CRS, origen, elevación, hardware objetivo, presupuestos de rendimiento y formato de persistencia.
- Toda nueva dependencia deberá documentar necesidad, alternativas, licencia, mantenimiento e impacto antes de incorporarse.
