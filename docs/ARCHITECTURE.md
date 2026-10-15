# Arquitectura inicial

## Decisión M3.3 — fachadas procedurales (autorizada)

M3.1 y M3.2 fueron aceptados técnica y visualmente por el usuario. M3.3 añade presentación sobre los datos locales ya cargados: `FBuildingFootprintData` → `FFacadeLayoutGenerator` → `FFacadeMeshGenerator` → `AFacadeBlockoutActor`. El layout contiene estilos, segmentos exteriores y piezas identificables por footprint/edificio/planta. El generador geométrico no lee archivos ni modifica el massing. Un actor por lote mantiene una malla agregada sin colisión; el massing M3.2 sigue siendo dueño de la colisión. No se necesita un subsystem sin estado global o ciclo de vida independiente: el GameMode compone ambos actores y una referencia débil vincula la presentación a su fuente. Reload reconstruye un candidato antes de sustituir la malla.

Estilos Classic, Modern, Residential y Civic son reglas genéricas sintéticas, seleccionadas por seed existente. Los anillos interiores se excluyen de módulos decorativos; los edges cortos pueden quedar sin huecos. Se preservan coordenadas, footprints, alturas y floors. Ventanas/puertas son overlays opacos, no perforaciones ni interacción. Se reutilizan GeometryCore/GeometryFramework y el material Engine vertex-color ya comprobado, sin dependencias, plugins, texturas ni assets binarios nuevos. M3.4/M4 no están autorizados; la aceptación visual M3.3 queda pendiente.

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
### Decisión M3.1/M3.2 (autorizada por el usuario)
M3 fue aceptado visualmente. Se autoriza únicamente un mapa mínimo propio `MVD_Prototype` y extrusión de footprints oficiales; En aquella fase M3.3/M4 seguían fuera de alcance; M3.3 fue autorizado posteriormente (decisión al inicio de este documento). El mapa se crea con editor scripting oficial, sin geometría GIS guardada. El GameMode existente conserva la composición runtime y las clases Third Person.

Se reutilizan FGeoReference, el contrato geométrico y FGeospatialBlockoutLoader: un loader específico de edificios añade validación de IDs/alturas/semillas sin duplicar conversiones. Un generador C++ puro extruye footprints triangulados offline por GEOS/Shapely (dependencia ya aprobada, con soporte de concavidad y huecos). Un actor agregado mantiene un DynamicMesh con grupos asociados a IDs; no se añade un subsistema sin necesidad ni un actor por pared. Carga y geometría quedan separadas del actor cartográfico M3. Se reutilizan Json, GeometryCore, GeometryFramework y materiales debug del Engine: sin nuevas dependencias ni plugins. Métricas separan lectura JSON, conversión, generación y colisión; el lote sigue acotado y no implementa streaming.

La capa oficial Construcciones notables describe siluetas de edificaciones destacadas, no todos los edificios. Sus atributos no incluyen plantas ni altura; se usarán exclusivamente alturas `synthetic_blockout` deterministas, nunca alturas oficiales supuestas. Los predios tributarios y puntos de permisos no se tratarán como footprints.

### Decisión M3: blockout de desarrollo sin assets nuevos
M3 autoriza visualización horizontal 1:1 en un plano de desarrollo Z=0; las alturas de datos continúan null. Se añaden únicamente módulos incluidos en UE 5.8.3: `Json` (lectura estricta del contrato), `GeometryCore` (FDynamicMesh3) y `GeometryFramework` (UDynamicMeshComponent, render y colisión estática). Se inspeccionaron sus headers en la instalación registrada. No se habilitan plugins: ProceduralMeshComponent requeriría un plugin innecesario, mientras crear StaticMesh/assets persistentes requeriría aprobación. Estas dependencias tienen la licencia/mantenimiento del propio Engine y no incorporan bibliotecas externas al juego.

Python conserva las dependencias M2.5: recorta fuentes oficiales a un cuadrado UTM de 700 m de lado (esquinas a ~495 m del origen), triangula polígonos conservando huecos mediante GEOS y emite WGS84 e índices deterministas. Geospatial carga/verifica el contrato y convierte con FGeoReference; World construye mallas transitorias genéricas y una escena de desarrollo. Un GameMode C++ dedicado reutiliza los Blueprints Third Person y el mapa Entry existente del Engine, sin alterar Lvl_ThirdPerson. La fecha UTC real de procesamiento se registra como parámetro explícito de lote y se conserva en reconstrucciones; ejecuciones posteriores no cambian bytes por el reloj.

El único origen numérico sigue en el derivado M2.5: el manifiesto M3 lo referencia con hash. Altura cero es exclusivamente una convención matemática para proyectar horizontalmente; se descarta Z geodésico y se muestra explícitamente el plano ficticio. Los estilos de líneas y offsets para evitar z-fighting no representan anchos ni elevaciones reales. M3 fue posteriormente aceptado visualmente por el usuario al autorizar M3.1/M3.2; la aceptación de edificios M3.2 fue confirmada posteriormente al autorizar M3.3.

### Decisión M2.5: herramientas de procedencia (antes de incorporar dependencias)
El pipeline offline de Espacios Públicos utilizará Python 3.12 y `pyshp==2.3.1` (MIT) para leer Shapefile sin GDAL, `pyproj==3.7.2` (MIT, basado en PROJ) para interpretar el PRJ y transformar EPSG:32721 a WGS84 con orden de ejes explícito, y `shapely==2.1.2` (BSD-3-Clause, GEOS bajo LGPL-2.1) para validar geometrías, área, centroide y contención. Son herramientas mantenidas del ecosistema geoespacial; versiones fijadas en requirements y entorno virtual local aislado. Las transitivas `numpy==2.5.4` (BSD-3-Clause; arrays de Shapely) y `certifi==2026.7.22` (MPL-2.0; certificados de pyproj) también se fijan explícitamente. No son dependencias de Unreal ni se distribuyen con el juego. Las ruedas de pyproj y shapely incluyen sus bibliotecas nativas; conservar sus avisos si se redistribuyen herramientas.

Alternativas: implementar lectores y proyecciones propios añadiría riesgo de errores de CRS/topología; GDAL/GeoPandas incorporan más dependencias de las necesarias. Se eligen estas tres bibliotecas para procesar una sola capa y conservar todas las transformaciones fuera de Data/raw. No se activa el origen en Config: el dataset aún no aporta altura fiable.

- Sin Cesium ni dependencias geoespaciales externas en Unreal; las herramientas Python offline M2.5 se limitan a las justificadas arriba.
- Sin importador runtime/editor todavía. Su implementación futura separará procesamiento Python, datos de intercambio e integración de editor de Unreal.
- Procedencia y licencias acompañan a los derivados; los binarios Unreal no se editan automáticamente.
- M2.5 fija CRS y origen horizontal en datos versionados; la prueba de desarrollo recibe el centroide verificado desde Python, sin hardcodearlo ni añadir un parser runtime. Pendientes: elevación/datum vertical, extensión exacta del slice, hardware objetivo, presupuestos de rendimiento y formato de persistencia. Ver `M2_5_DATA_PROVENANCE.md`.
- Toda nueva dependencia deberá documentar necesidad, alternativas, licencia, mantenimiento e impacto antes de incorporarse.

## Decisiones de M1
- Un único módulo runtime `MontevideoGame`; targets Game y Editor existentes. No se crean módulos ni plugins.
- Se evolucionan `AMontevideoGameCharacter`, `AMontevideoGamePlayerController` y `AMontevideoGameGameMode`. Mantener nombres, propiedades, funciones y subobjetos serializados evita migrar Blueprints. Los redirects `TP_ThirdPerson` de Config son parte de la compatibilidad.
- GameMode configura en C++ los dos Blueprints existentes de Pawn y Controller mediante rutas de contenido estables. Esta dependencia de presentación es deliberada: conserva malla, animación, Input Actions, contextos y UI táctil sin reescribir assets. Los Blueprints pueden sobrescribir configuración; no implementan en exclusiva locomoción ni cámara.
- Character conserva movimiento/cámara y concentra entrada de teclado, mando y UI en `DoMove`, `DoLook`, `DoJumpStart` y `DoJumpEnd`. Controller administra contextos locales y el ciclo de vida de la UI táctil; no se añaden sistemas de interacción/HUD/vehículos.
- Build.cs conserva Core, CoreUObject, Engine, InputCore, EnhancedInput, AIModule, StateTreeModule, GameplayStateTreeModule y UMG. Las variantes originales consumen IA/StateTree y widgets; no son dependencias prescindibles mientras esas fuentes y assets permanezcan. Slate pasa a dependencia privada, usado por `SVirtualJoystick` en los `.cpp`. No se incorpora ninguna dependencia nueva.
- Las variantes originales no forman parte del gameplay M1, pero siguen compilándose y se preservan para no romper el template. Su retirada o separación futura requiere revisar sus assets y referencias.
