# M3.3 — Procedural Facade System

**Autorizado por el usuario tras aceptar visualmente M3.1/M3.2. La aceptación visual de M3.3 está pendiente. No autoriza M3.4 ni M4.**

Las fachadas son una demostración procedural de presentación. Classic, Modern, Residential y Civic son estilos **genéricos y sintéticos**; no representan una clasificación verificada de Montevideo. “Construcciones notables” sigue siendo una fuente oficial parcial: 9 edificios / 12 footprints, no cobertura urbana completa. No se consultan fuentes nuevas ni se modifican datos GIS, alturas o footprints.

## Arquitectura y contratos

`FBuildingFootprintData` → massing M3.2 existente → `FFacadeLayoutGenerator` → `FFacadeMeshGenerator` → `AFacadeBlockoutActor`.

- `FFacadeStyleDefinition`: dimensiones en centímetros, tratamiento de planta baja, probabilidad de balcones y color lineal.
- `FFacadeSegment`: un edge del anillo exterior; identifica footprint, edificio, source_feature_id, edge, estilo, ejes locales, longitud, altura, floors y límites Z.
- `FFacadeElement`: caja orientada con rangos U (a lo largo del edge), D (hacia afuera) y Z, tipo y color. Una ventana comprende panel y marcos; un balcón comprende losa y tres barandas.
- `FBuildingFacadeData`: segmentos, elementos, contadores y tiempo de layout. No almacena UObject ni conoce archivos GIS.
- `FFacadeLayoutGenerator`: función pura sobre los datos locales M3.2; valida entradas, resuelve reglas y límites geométricos.
- `FFacadeMeshGenerator`: construye un candidato FDynamicMesh3 con normales y colores por esquina. Cada grupo identifica un segmento y desde él su footprint/edificio. No abre ni cambia el sólido M3.2.
- `AFacadeBlockoutActor`: un actor y un UDynamicMeshComponent para todo el lote, sin Tick, colisión ni navegación. Una referencia débil enlaza con el actor de massing; no duplica la carga JSON. Reload sustituye únicamente tras validar layout, mesh y material. Fallos conservan el resultado anterior.

El GameMode compone el actor de fachadas después de cargar edificios; si falla la presentación, mantiene el recorrido M3.2. No se necesita un subsystem adicional para este lote. No se agregan módulos, plugins ni dependencias. El generador M3.2 y su contrato permanecen compatibles e intactos.

## Determinismo y estilos

`style = seed % 4`, o un override temporal de debug. La seed proviene del ID estable ya calculado en M3.2. Los balcones emplean una mezcla uint32 explícita de seed, índice de footprint, edge, planta y bay; no se usa FMath::Rand ni reloj. Para los mismos datos ordenados y la misma build, layout, colores, coordenadas e índices se repiten exactamente. Reordenar/cambiar footprints en un dataset futuro puede alterar balcones: no se promete estabilidad entre versiones de datos.

| ID | Estilo genérico | Bay objetivo cm | Ventana ancho × alto cm | Balcones | Planta baja / media | Remate cm | Color RGB lineal |
| ---: | --- | ---: | --- | ---: | ---: | ---: | --- |
| 0 | Classic | 300 | 120 × 150 | 24% | 1,10 | 18 | 0,62 / 0,49 / 0,32 |
| 1 | Modern | 340 | 180 × 150 | 0% | 1,15 | 8 | 0,36 / 0,43 / 0,49 |
| 2 | Residential | 300 | 120 × 150 | 40% | 1,10 | 12 | 0,55 / 0,29 / 0,20 |
| 3 | Civic | 380 | 140 × 170 | 0% | 1,20 | 20 | 0,62 / 0,61 / 0,51 |

Las probabilidades son umbrales deterministas, no garantía de porcentajes exactos ni de balcones en cada edificio. Clearance puede rechazarlos. El lote actual no tiene por qué seleccionar todos los estilos: el override permite comprobar cada uno.

## Layout, esquinas y patios

Solo se recorren los anillos exteriores (`Rings[0]`); los anillos interiores oficiales identifican patios y quedan sin módulos. Se normaliza la orientación de cada edge hacia exterior, conservando sus coordenadas. Los lados cóncavos y las partes separadas se procesan independientemente, agrupando las puertas por BuildingId.

Margen de módulos: **75 cm por extremo**. Bays = floor((longitud − 150 cm) / ancho objetivo), mínimo cero. El espacio útil se divide uniformemente entre los bays, sin huecos parciales. Edges cortos conservan el massing y pueden recibir solo revestimiento/remate; no se fusionan edges ni se simplifica el footprint.

Antes de colocar piezas se comprueba su proyección rectangular contra todos los footprints: esquinas dentro de masa, vértices de masa dentro del rectángulo y cruces de segmentos rechazan la colocación. Esto protege concavidades, otras partes y volúmenes próximos. No se interpreta orientación a calle ni accesibilidad: un exterior puede ser un límite artificial del recorte M3.2. La política es conservadora y puede omitir decoración válida por solapamiento horizontal aunque las alturas difieran.

## Plantas, puertas, ventanas y balcones

Media de planta = `height_m * 100 / floors`, aceptada entre 220 y 600 cm. Planta baja = media × factor de estilo; el resto se distribuye uniformemente hasta la altura exacta M3.2. Una sola planta ocupa toda la altura. No hay entrepisos. Las bandas horizontales muestran las divisiones.

Una puerta de desarrollo por edificio cuando existe un exterior con bay y clearance suficiente: edge más largo entre todas las partes, desempate por orden estable, bay central. Puerta de 150 cm de ancho y hasta 245 cm de alto. No interacción ni abertura física; la colisión de pared continúa detrás.

Ventanas: panel opaco oscuro, cuatro marcos; planta baja tiene ancho ×1,15. Se dejan márgenes superiores y antepecho, limitando el alto al espacio real de planta. No transparencia, fotografías ni textura artística. Los balcones solo aparecen en plantas superiores y bays de al menos 280 cm: losa de 200 cm de ancho, profundidad exterior máxima 45 cm, barandas blockout de 91 cm sobre losa. No tienen colisión ni son transitables; no se pueden usar para saltar al edificio.

## Geometría, cubierta y materiales

La cubierta plana existente M3.2 se conserva. El remate superior queda dentro de los últimos 18 cm de altura; nunca incrementa la altura máxima del edificio. No tejados inclinados.

Material existente: `/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly`, unlit, PrimaryColors, NoTransform. Colores de estilo en la piel exterior, planta baja oscurecida; marcos/remates claros y paneles oscuros. Un factor determinista por normal hace legibles los volúmenes con material unlit.

Offsets hacia fuera desde la pared, en cm:

| Pieza | Intervalo D |
| --- | --- |
| Piel exterior | 0,5–1 |
| Tratamiento planta baja | 1,2–1,7 |
| Panel ventana/puerta | 2–3 |
| Marcos | 3–8 |
| Bandas de planta | 2–4 |
| Cornisa | 2–8/12/18/20 según estilo |
| Balcón | 2–45 |

Los offsets son de presentación para evitar z-fighting; no cambian GIS ni colisión. La piel usa margen lateral de hasta 2 cm; las cornisas y bandas se retiran hasta 75 cm en esquinas. Pueden existir juntas visibles en remates: no se hacen mitras ni booleans. Cada caja tiene 8 vértices compartidos y 12 triángulos con overlays por esquina. No hay actores por módulo. Presupuestos defensivos: 2.000 footprints, 10.000 segmentos, 1.000 bays por edge y 100.000 piezas; el exceso se rechaza, no se trunca silenciosamente.

## Comandos y recarga

- `Montevideo.Facades.Status`: edificios, edges/fachadas, bays, ventanas, puertas, balcones, estilos, descartes conservadores, vértices/triángulos y tiempos.
- `Montevideo.Facades.Reload`: reconstruye desde el massing actualmente cargado y restablece estilos por seed.
- `Montevideo.Facades.Style -1`: selección por seed. `0` Classic, `1` Modern, `2` Residential, `3` Civic: override para todo el lote, sin modificar fuentes.
- `Montevideo.Facades.Building 106187939`: resalta contorno, bounds y etiqueta del ID durante 30 segundos. Acepta también BuildingId completo.

Si se recarga primero el massing con `Montevideo.Buildings.Reload`, ejecutar después `Montevideo.Facades.Reload`. No hay observador automático ni sincronización continua; esta fase no permite editar alturas en gameplay. La destrucción de la fuente invalida recargas; los actores se retiran con el mundo/PIE.

## Tests y validación

Suite `Montevideo.Facades`: datos reales/determinismo completo, concavidad/patios/multipart/winding, edges cortos/largos/entradas inválidas, mesh/bounds/normales/índices, recarga/override y aislamiento de colisión. Comprueba puertas, ventanas y balcones dentro del edge/planta, ausencia de balcones en planta baja, altura y floors originales, NaN/Inf, colisión intacta y un solo componente.

Se ejecutan además suites M2/M2.5, M3 y M3.2, Python existente y protección SHA-256 de todos los assets y Data. No se añade lógica de fachada a Python.

### Resultado comprobado — 2026-10-15

- Instalación descubierta con EngineAssociation/registro: `C:/Program Files/Epic Games/UE_5.8`; Build.version **5.8.3, CL 58210709**. Target `MontevideoGameEditor Win64 Development`.
- Primera compilación de la implementación: **Succeeded, exit 0, 200,45 s**. Corrección posterior del fixture y límite defensivo compiló, pero el enlace encontró un bloqueo transitorio `LNK1104` de la DLL (exit 6, 78,11 s). Sin procesos Unreal activos al revisar, el reintento final completó enlace/metadatos: **Succeeded, exit 0, 13,84 s**. Este último tiempo es incremental, no una compilación limpia completa.
- Automation final: **26/26 correctas**, cero warnings en tests, cero fallos y cero no ejecutadas; exit 0. Geospatial 9, DataProvenance 1 con centroide real como argumento, M3 6, Buildings 5 y Facades 5. La primera pasada ya pasó 26 tests, pero emitió un warning por destruir un actor de fixture sin contexto de mundo; se corrigió registrando/desregistrando el contexto y se repitió la suite.
- Python existente: **21/21 correctas**, sin omisiones, exit 0, 21,263 s. Incluye reproducibilidad e integridad; no se cambió el pipeline.
- Standalone `-game -nullrhi`: **exit 0**, carga por defecto MVD_Prototype y crea `BP_ThirdPersonCharacter_C_0`, massing y fachadas. Status, Building 106187939, Style 1 y Reload ejecutados; el override Modern produjo 9 edificios Modern y cero balcones, y Reload recuperó exactamente los contadores originales.
- SHA-256: **754 assets y 25 archivos de Data intactos**, cero modificados/eliminados y cero assets nuevos. Incluye raw, processed y MVD_Prototype. Git LFS conserva el mapa con OID `3e4b66253e8e90afc5b4c24c7bd53f19d1bc0730fb1e206c8b290230338578da`.
- Warnings conocidos de build: MSVC 14.51.36260 no preferido (recomendado por UE 14.50.35717); deprecaciones C4996 en headers Engine para GetMaterialRelevance, GetMovementBase, GetRadius y GetMargin. Ningún warning atribuido al código C++ nuevo en la compilación final de fuentes.
- El log Automation conserva 14 `Condition failed` durante el arranque del Engine, anteriores a las suites Montevideo; el reporte de estas es 26/26 limpio. Standalone conserva 21 warnings de widgets `LogEditorDataStorageUI`. Además, el lote de ocho comandos, con dos reconstrucciones síncronas, produjo `TickDeferredCommands took 1740.93ms`: se registra como coste real de debug, no se oculta ni se presenta la recarga como instantánea. No hubo errores standalone.

Evidencias locales (no versionadas): `Saved/M3_3/build.log`, `build-initial.log`, `build-dll-lock.log`, `Automation/index.json`, `tests.log`, `python-tests.log`, `standalone.log`, `before.json`, `integrity.json`.

### Métricas y rendimiento

Contadores del estilo por seed, verificados también después de Reload:

| Métrica | Resultado |
| --- | ---: |
| Edificios / footprints | 9 / 12 |
| Fachadas (segmentos exteriores, no frentes arquitectónicos completos) | 506 |
| Edges sin bay por longitud insuficiente | 443 |
| Bays horizontales (repetidos por planta) | 430 |
| Ventanas / puertas / balcones | 1.762 / 9 / 465 |
| Edificios Classic / Modern / Residential / Civic | 2 / 0 / 5 / 2 |
| Piezas orientadas | 13.362 |
| Vértices / triángulos adicionales de fachada | 106.896 / 160.344 |
| Grupos de identidad de fachada | 506 posibles; las piezas referencian su segmento |
| Payload CPU estimado de mesh | 22.448.160 bytes (~21,41 MiB) |
| Layout inicial standalone | 97,093 ms |
| Generación y validación mesh | 448,822 ms |
| Asignación al componente | 1,014 ms |
| Total inicial de fachadas | **547,048 ms** |

La suma no es exactamente el total: incluye búsqueda de material, sustitución de estado y otros costes. No incluye carga GIS ni generación de massing. Los contadores de vértices/triángulos son **adicionales** a los 1.052/2.076 de M3.2. No son métricas de FPS ni GPU: la ejecución usa NullRHI. Máquina de medición: Ryzen 5 3500U, 13 GiB RAM informada por Automation. Una sola ejecución no es benchmark estadístico. En el mismo proceso, override Modern tardó 727,081 ms y la recarga posterior 990,749 ms: coste síncrono variable, cercano a un segundo en esta máquina. Se mantiene documentado como deuda antes de escalar; no se introduce optimización prematura.

## Inventario de cambios M3.3

Nuevos archivos:

- `Source/MontevideoGame/World/FacadeLayoutGenerator.h`
- `Source/MontevideoGame/World/FacadeLayoutGenerator.cpp`
- `Source/MontevideoGame/World/FacadeMeshGenerator.h`
- `Source/MontevideoGame/World/FacadeMeshGenerator.cpp`
- `Source/MontevideoGame/World/FacadeBlockoutActor.h`
- `Source/MontevideoGame/World/FacadeBlockoutActor.cpp`
- `Source/MontevideoGame/Tests/FacadeTests.cpp`
- `docs/M3_3_PROCEDURAL_FACADES.md`

Archivos existentes modificados:

- `Source/MontevideoGame/World/MontevideoBlockoutGameMode.cpp`: composición del actor de presentación.
- `README.md`, `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`, `docs/GAMEPLAY_SYSTEMS.md`: fase actual, responsabilidades y alcance.
- `docs/M3_1_PROJECT_WORLD.md`, `docs/M3_2_BUILDING_GENERATOR.md`: registrar la aceptación visual comunicada por el usuario.

Sin cambios en Build.cs, Config, scripts Python, loader GIS, generador M3.2 ni assets binarios.

## Limitaciones y deuda técnica

- Fachadas genéricas sobre cobertura oficial parcial; alturas sintéticas, terreno Z=0. No reconstrucción arquitectónica histórica.
- Ventanas/puertas son overlays; interiores inexistentes y puertas no atravesables. Balcones sin colisión.
- Sin clasificación frente/fondo/calle, accesibilidad, medianeras o límites de recorte; solo exterior vs anillos interiores.
- Edges fragmentados de fuente pueden quedar sin módulos aunque visualmente formen una pared larga. Sin simplificación ni agrupación de colineales para preservar trazabilidad.
- Clearance horizontal conservador, sin resolución global de conflictos entre piezas decorativas de distintos edges; las separaciones y el recorrido visual requieren inspección del usuario.
- Recarga síncrona y malla agregada adecuada al lote, sin LOD/instancing/streaming. No escalar a ciudad completa; medir y definir sectores antes.
- Payload de mesh es estimación CPU, excluye objetos, layout, adyacencia, allocator, GPU y massing. NullRHI no mide rendimiento GPU ni valida apariencia.
- Recursos debug y Data aún no validados en distribución empaquetada. No introducir assets binarios para resolverlo en esta fase.

## Inspección visual exacta

1. Reiniciar Unreal tras compilar, abrir MontevideoGame.uproject con UE 5.8.3 y `Content/Montevideo/Maps/MVD_Prototype`.
2. Play → Selected Viewport. El mapa vacío antes de Play es esperado: toda la escena GIS/fachadas es runtime.
3. Consola: `Montevideo.Facades.Status`, luego `Montevideo.Facades.Building 106187939` (Teatro Solís). Usar también `Montevideo.Buildings.Bounds` para localizar el conjunto.
4. F8; seleccionar FacadeBlockoutActor en World Outliner y pulsar F sobre el viewport. Volar con botón derecho + WASD y ajustar velocidad; inspeccionar desde arriba y bajar a nivel de calle. Buscar plantas, puertas oscuras más altas, ventanas con marcos y balcones en edificios Classic/Residential.
5. Probar `Montevideo.Facades.Style 0`, `1`, `2`, `3`; finalizar con `Montevideo.Facades.Reload` para recuperar selección por seed. Comparar recargas: no deben duplicar piezas ni cambiar distribución.
6. Revisar esquinas cóncavas, extremos cortos, patios del Cabildo y Ministerio de Trabajo: no deben aparecer módulos en sus anillos interiores ni piezas atravesando masa. Revisar que remates no suban la cubierta.
7. F8 para recuperar al personaje; WASD, ratón y Espacio. Caminar entre edificios y empujar contra paredes/puertas: el sólido sigue bloqueando. No esperar colisión en balcones decorativos.
8. Confirmar visualmente ausencia de z-fighting, módulos flotantes perceptibles o cruces en esquinas. Detener Play sin guardar cambios de simulación. La aceptación M3.3 corresponde al usuario; no avanzar a M3.4/M4.
