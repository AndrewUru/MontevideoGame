# M3.2 — Procedural Building Generator proof of concept

**Estado: implementación y validación técnica; aceptación visual pendiente del usuario. No autoriza M3.3 ni M4.**

> **LIMITATION:**
> "Construcciones notables" is not a complete building footprint dataset.
> The current M3.2 implementation validates the procedural building pipeline
> using a limited official dataset and must not be interpreted as complete
> urban coverage.

## Fuente, selección y procedencia

Organismo: Intendencia de Montevideo, Servicio de Geomática. Layer descubierto en GetCapabilities: **`mapstore-base:cb_v_sig_construcciones_notables`**. La metadata describe siluetas de edificaciones destacadas digitalizadas sobre ortomosaico de diciembre de 2021, resolución 10 cm/píxel. No se buscó ni incorporó otra fuente después de confirmar este proof of concept.

- [WFS GetCapabilities oficial](https://montevideo.gub.uy/app/geoserver/ows?service=WFS&version=1.1.0&request=GetCapabilities).
- [Metadata y licencia de Construcciones notables](https://geoweb.montevideo.gub.uy/geonetwork/srv/api/records/939d2fd2-3b25-4b08-b2e1-753b9e7cd9ee/formatters/xml).
- Licencia declarada: **uso libre según resolución IM 640/10, 22/02/2010**. Declaración íntegra, atribución, URLs exactas, fecha UTC, SHA-256 y tamaños en `Data/raw/montevideo/buildings/2026-10-15/source.json` y `metadata.xml`.
- CRS: **EPSG:32721, WGS84 / UTM 21S**, comprobado en metadata y PRJ. Intercambio: EPSG:4326, arrays longitud/latitud; runtime: FGeoReference, X norte/Y este/Z arriba, cm.

Comparación realizada antes de elegir: “Edificación Inapropiada” representa zonas de aplicación tributaria, no siluetas de edificios; “Permisos de Construcción” representa puntos de trámites asociados a padrones, no footprints. “Construcciones notables” sí declara geometría poligonal real y atributos `gid`, `estado`, `nombre`; su esquema WFS declara MultiPolygon. No hay altura ni número de plantas oficial.

La consulta WFS usa exactamente la ventana UTM M3 de 700 × 700 m, con máximo 2.000 entidades y rechazo de respuestas truncadas. Devuelve 10 entidades originales; la envolvente de un multipolígono de Puerto de Montevideo intersecta la consulta, pero sus superficies no intersectan el recorte exacto. Se excluye esa entidad por ausencia de superficie dentro del AOI, no por error de triangulación. Resultado: **9 edificios / 12 footprints**. El WFS puede devolver partes del mismo multipolígono fuera del BBOX; se conservan en raw y se recortan offline, sin cargarlas en Unreal. No se solicitó toda la ciudad.

## Pipeline y contrato

`fetch_buildings.py` → snapshot inmutable → `process_buildings.py` → `buildings.json` + `buildings_manifest.json` → `FBuildingFootprintLoader` → `FBuildingMeshGenerator` → `ABuildingBlockoutActor`.

Comandos desde la raíz, con el entorno geoespacial existente:

```powershell
& ./.venv-geospatial/Scripts/python.exe Scripts/geospatial/process_buildings.py
& ./.venv-geospatial/Scripts/python.exe -m unittest discover -s Scripts/geospatial -p 'test_*.py' -v
& ./.venv-geospatial/Scripts/python.exe Scripts/geospatial/validate_buildings.py --report Saved/M3_2/geometry-audit.json
```

La reconstrucción reutiliza la fecha UTC registrada. Una captura nueva requiere otro directorio raw y `--source`; nunca se sobrescriben originales. `--floor-height-m` configura el valor de desarrollo, rango 2–5 m, predeterminado 3 m. Las métricas se escriben fuera de Data/processed para mantener el JSON byte-idéntico.

`buildings.json` extiende el contrato poligonal M3: `schema_version`, `building_schema_version`, `crs`, `features`. Cada feature es un componente poligonal con `id` único, `building_id` común a sus partes, `source_feature_id`, atributos originales, `vertices`, `rings` (exterior seguido de huecos), `triangles`, centroide de área derivado en UTM, altura, plantas, procedencias y seed. Conserva áreas UTM/local, bounds, recorte y ajustes numéricos explícitos. El centroide puede estar fuera de un polígono cóncavo: es metadata, no un punto de spawn ni garantía de interior.

El manifiesto referencia el origen M2.5 y sus hashes, snapshot, licencia, ventana, unidades/ejes, versiones, fecha y regla de altura; verifica el JSON por SHA-256 en Python y SHA-1 portable en C++. Las alturas de terreno/datum siguen null. No se cambia ningún dato M2/M3 preexistente.

## Alturas y determinismo

Para cada edificio: `building_id = layer + ':' + gid`. `seed` es el entero uint32 big-endian de los primeros cuatro bytes de SHA-1 de ese identificador UTF-8. `floors = 3 + seed % 3`; `height_m = floors × 3.0`. Todas las partes del edificio comparten seed y altura. **Tanto `height_source` como `floors_source` son `synthetic_blockout`**. No se infiere altura de `estado`, nombre, área ni relevancia del edificio.

| source_feature_id | seed | plantas sintéticas | altura m |
| --- | ---: | ---: | ---: |
| 106187939 | 656600148 | 3 | 9 |
| 106187941 | 1325144247 | 3 | 9 |
| 106187942 | 2006907026 | 5 | 15 |
| 106187965 | 3164024926 | 4 | 12 |
| 106188003 | 132923978 | 5 | 15 |
| 106188004 | 552097334 | 5 | 15 |
| 106188012 | 344460884 | 5 | 15 |
| 106188013 | 2295373874 | 5 | 15 |
| 106188055 | 2780362995 | 3 | 9 |

Alturas oficiales: **0**. Derivadas de plantas oficiales: **0**. Sintéticas: **9**. Mínima/media/máxima por edificio: **9 / 12,666667 / 15 m**. No representan las alturas reales de Teatro Solís, Torre Ejecutiva ni otros edificios.

## Geometría, triangulación y tolerancias

Se valida la geometría original, se recorta en UTM y se normaliza el orden de partes y anillos. No se reparan polígonos inválidos ni se simplifican siluetas. GEOS/Shapely triangula con constrained Delaunay en el plano local calculado por PROJ, preservando concavidad y huecos. C++ convierte los vértices WGS84 con FGeoReference y comprueba winding, áreas y bounds de forma independiente. No hay triangulador casero ni triangulación dentro del GameMode.

Se detectó en Palacio Estevez un par de vértices originales que colapsa a una coordenada WGS84/local exactamente igual en doble precisión. Se fusiona únicamente esa duplicación tras exigir distancia original UTM ≤ **1 micrómetro**; no se desplaza ninguna coordenada restante. Registro `merged_reprojection_vertices=1`. La versión anterior generaba una unión incompleta entre cubierta y pared: no se aceptó como válida.

Si GEOS omite un vértice verdaderamente colineal, se restaura subdividiendo el borde del triángulo contenedor, con tolerancia local ≤1 micrómetro, sin borrar el vértice ni inventar coordenadas. Después se vuelve a comprobar cobertura. Ningún footprint se descartó por geometría inválida. La prueba de bordes exige que el contorno de la triangulación coincida exactamente con todos los segmentos de los anillos.

Tolerancias: área de cubierta frente al polígono local **≤1e-6 m²**; área local frente al área UTM **<0,2 %**, por diferencias de proyección, no por reescalado. Los bounds independientes se comprueban a 0,1 cm. No se omiten triángulos degenerados para esconder agujeros: una degeneración no resuelta rechaza el lote.

La extrusión crea cubierta, base y paredes para todos los anillos. Normaliza exterior CCW/huecos CW en el plano local y adapta los triángulos a la convención clockwise de Unreal. Las normales de overlays apuntan al exterior; cubierta +Z, base -Z y paredes horizontales. Comprueba validez de DynamicMesh y ausencia de aristas abiertas antes de reemplazar la escena.

## Malla agregada, identidad y representación

Un **ABuildingBlockoutActor**, un **UDynamicMeshComponent**. No se crean actores por edificio ni por pared. `TriangleGroup` corresponde al índice del footprint en `GetData().Footprints`; desde ahí se recuperan `id`, `building_id`, `source_feature_id`, seed y procedencia. Varias partes de un mismo edificio conservan la misma identidad de origen.

Malla agregada: **1.052 vértices de posición, 2.076 triángulos**, distribuidos en **1.052 de paredes, 512 de cubiertas y 512 de bases**, con **12 grupos**. La planta tiene 526 vértices; los overlays de normales y colores tienen elementos adicionales por esquina y se incluyen en la estimación de payload. La malla mantiene los cinco huecos del lote (cuatro en Cabildo y uno en Ministerio de Trabajo).

Se reutiliza `/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly`, unlit. Colores RGBA llegan por PrimaryColors; cubierta beige lineal (0,55; 0,49; 0,38), paredes con un factor según orientación para lectura volumétrica. Es sombreado esquemático, no materiales ni iluminación artísticos. No hay assets de materiales nuevos.

### Auditoría por footprint

La identidad completa de cada grupo es `layer:gid:parte`; una misma identidad `layer:gid` reúne las partes del edificio. Los índices se reconstruyen determinísticamente para este lote; no son IDs persistentes entre futuras versiones del dataset. Área local y cubierta coinciden dentro de 1e-6 m²; la columna redondea a seis decimales. Exterior y huecos se orientan para normales exteriores al extruir.

| Grupo | gid / parte | Vértices planta | Huecos | Cóncavo | Área UTM m² | Área local/cubierta m² | Techo/base tri. | Pared tri. |
| ---: | --- | ---: | ---: | --- | ---: | ---: | ---: | ---: |
| 0 | 106187939 / 0 | 243 | 0 | sí | 4528.427762 | 4531.457333 | 241 | 486 |
| 1 | 106187941 / 0 | 20 | 4 | sí | 1456.267080 | 1457.242138 | 26 | 40 |
| 2 | 106187942 / 0 | 18 | 0 | sí | 445.193203 | 445.491391 | 16 | 36 |
| 3 | 106187965 / 0 | 6 | 0 | no | 1212.116266 | 1212.926783 | 4 | 12 |
| 4 | 106187965 / 1 | 18 | 0 | sí | 2049.510265 | 2050.880818 | 16 | 36 |
| 5 | 106188003 / 0 | 8 | 0 | sí | 577.788695 | 578.175279 | 6 | 16 |
| 6 | 106188004 / 0 | 3 | 0 | no | 8.301641 | 8.307201 | 1 | 6 |
| 7 | 106188004 / 1 | 5 | 0 | sí | 197.330877 | 197.463053 | 3 | 10 |
| 8 | 106188012 / 0 | 4 | 0 | no | 678.866984 | 679.321701 | 2 | 8 |
| 9 | 106188013 / 0 | 7 | 0 | sí | 794.586913 | 795.118799 | 5 | 14 |
| 10 | 106188013 / 1 | 10 | 1 | sí | 720.041476 | 720.523199 | 10 | 20 |
| 11 | 106188055 / 0 | 184 | 0 | sí | 1051.056209 | 1051.758826 | 182 | 368 |

## Colisión y herramientas

Colisión compleja como simple, síncrona y estática, sobre la única malla agregada. La generación se valida antes de reemplazarla; una recarga con datos inválidos conserva la escena previa. La cocción se separa de SetMesh mediante defer temporal para medirla. No se implementan interiores; los huecos existentes permanecen abiertos y sus paredes interiores tienen colisión.

- `Montevideo.Buildings.Status`: dataset, edificios/footprints, procedencia de alturas, min/media/max, vértices/triángulos, bounds y tiempos.
- `Montevideo.Buildings.Bounds`: envolvente y etiquetas de IDs oficiales durante 30 s.
- `Montevideo.Buildings.Reload`: recarga validada sin acumular actores ni geometría.

## Tests y validación

Python cubre inmutabilidad raw, PRJ/CRS, IDs, geometrías no vacías/válidas, AOI, byte reproducibilidad, seed, política de altura, borde de cubierta exacto, área, concavidad, huecos y winding independiente. C++ cubre el dataset real, conversión, determinismo de vértices/índices, grupos, malla cerrada y normales; fixtures explícitos de concavidad/huecos/multiparte; raycasts y cápsula de personaje contra pared; recarga inválida y límites de extrusión.

La inspección visual final sigue pendiente. Los tests físicos no sustituyen una comprobación de movimiento y render en Play.

### Resultado final comprobado — 2026-10-15

- UE **5.8.3**, instalación descubierta mediante EngineAssociation y registro: `C:/Program Files/Epic Games/UE_5.8`. Target `MontevideoGameEditor Win64 Development`: **Result: Succeeded; exit code 0; 69,76 s**.
- Python: **21/21 correctas**, 14,146 s, sin omisiones. Incluye suites anteriores y edificios. Auditoría adicional: dos reconstrucciones byte-idénticas a los derivados existentes, sin modificar Data.
- Unreal: **21/21 correctas**, 0 con warnings, 0 fallidas, 0 no ejecutadas; proceso exit 0. Desglose: Geospatial 9, DataProvenance 1 (centroide real pasado por argumentos), M3 6 y Buildings 5. Se comprobó el reporte exportado, no solamente el código de salida.
- Raycast de cubierta con normal +Z, raycast lateral, huecos/espacios vacíos, bloqueo por pared de una cápsula ACharacter, bases, normales de render, grupos y recargas: correctos. La auditoría detectó y corrigió una doble inversión: `VectorUtil::Normal` ya usa la convención de Unreal. El test comprueba las normales por esquina de los 2.076 triángulos.
- Standalone `-game -nullrhi`: **exit 0**, carga MVD_Prototype por defecto, instancia `BP_ThirdPersonCharacter_C_0` y genera 9 edificios/12 footprints. Esto comprueba arranque y geometría runtime, no apariencia renderizada.
- Cero geometrías inválidas descartadas. La única entidad excluida es Puerto de Montevideo (`106188057`), sin intersección superficial con el AOI exacto. La tabla de footprints y las tolerancias anteriores documentan el resto.
- Warnings de compilación conocidos: MSVC 14.51.36260 no preferido (UE recomienda 14.50.35717); C4996 en headers del Engine: GetMovementBase, GetMaterialRelevance, GetRadius y GetMargin. No quedan warnings atribuidos al código nuevo ni se suprimieron diagnósticos.
- El arranque de Automation registra 14 mensajes `Condition failed` anteriores a la suite Montevideo; se conservan como incidencia del arranque del Engine, separada del reporte 21/21. El standalone registra 21 warnings de fábricas de widgets `LogEditorDataStorageUI`, sin errores. No se afirma que todos los logs del Engine estén limpios.

Evidencia local no versionada: `Saved/M3_2/build.log`, `python-tests.log`, `Automation/index.json`, `tests.log`, `standalone.log`, `geometry-audit.json`, `world-audit.json` y `asset-integrity.json`.

## Performance

La triangulación ocurre offline; el runtime no la repite. Los contadores separan lectura de archivos, checksum, decodificación/parseo JSON, conversión geográfica, extrusión de FDynamicMesh3, asignación al componente y cocción de colisión. El tiempo total incluye validación, material y otros costes; no es solo la suma de esos contadores. El loader de edificios reutiliza el loader geográfico y vuelve a leer metadata: ese coste real se incluye, no se oculta.

La memoria informada es una **estimación de payload CPU** de posiciones, índices, grupos y overlays; excluye capacidad reservada, adyacencia, objetos, colisión y GPU. No se presenta como memoria total del proceso ni VRAM. No se ha optimizado prematuramente.

Medición final de una carga standalone headless en esta máquina (no benchmark estadístico ni medición de GPU):

| Etapa | Tiempo ms |
| --- | ---: |
| Lectura JSON | 3,363 |
| Decodificación/parseo JSON | 36,199 |
| Checksum | 0,359 |
| Conversión geográfica | 0,148 |
| Extrusión | 18,870 |
| Asignación/creación mesh del componente | 0,172 |
| Creación de colisión | 8,422 |
| Total runtime de edificios | **71,410** |
| Triangulación offline (incluye comprobación de cobertura y bordes) | 128,794 |
| Pipeline offline completo | 346,673 |

La triangulación offline no se suma al tiempo runtime. Payload estimado: **282.672 bytes (~276 KiB)** para 1.052 vértices/2.076 triángulos. Bounds cm: `(-33897.772, -35086.237, 0)` a `(34837.557, 4752.758, 1500)`.

## Limitaciones y deuda

- Cobertura parcial de construcciones notables: no completa manzanas ni implica calles flanqueadas por edificios en toda la ventana.
- Alturas exclusivamente sintéticas; terreno/datum desconocidos, base plana Z=0. Sin interiores, fachadas, ventanas ni puertas.
- Los edificios que cruzan el AOI se recortan; las paredes que cierran ese corte son límites del blockout, no fachadas medidas. No se completa la parte exterior a la ventana.
- Una malla agregada adecuada para este lote, pero sin streaming/LOD ni edición incremental por edificio. Antes de escalar deben definirse sectores, presupuestos y actualización por grupos; no cargar toda Montevideo.
- Recarga síncrona y cocción de colisión pueden causar pausas. Las mediciones dependen de esta máquina y ejecución, no son un presupuesto de rendimiento de producción.
- Falta staging/empaquetado de Data y validación de recursos debug en builds distribuidos. No se promete soporte de juego empaquetado.

## Future building coverage

Antes de escalar habrá que localizar o construir una fuente de footprints con cobertura completa, potencialmente mediante otra fuente oficial, LiDAR, catastro compatible o combinación de datasets. Cada alternativa requiere evaluar licencia, precisión, fecha, semántica y compatibilidad espacial. **No se implementa ni se adquiere ninguna de esas alternativas en M3.2.** No se cambia de capa ni se incorpora OSM sin aprobación.

## Validación visual en Unreal

1. Abrir `Content/Montevideo/Maps/MVD_Prototype` con UE 5.8.3 y Play → Selected Viewport.
2. Comprobar plaza verde, vías amarillas, manzanas azul/gris y edificios beige. Ejecutar `Montevideo.Buildings.Status`: 9 edificios, 12 footprints, todas las alturas sintéticas.
3. Ejecutar `Montevideo.Buildings.Bounds` para ver IDs. En PIE, F8, seleccionar `BuildingBlockoutActor` en World Outliner y F sobre el viewport. Subir/alejar la cámara y mirar desde arriba; inspeccionar cubiertas, huecos y separación de volúmenes. Las etiquetas permiten relacionarlos con la tabla anterior.
4. Volver a poseer el personaje con F8. WASD, ratón y Espacio; caminar desde la plaza hacia los edificios de su entorno. No esperar cobertura completa ni alturas reales.
5. Empujar al personaje contra varias paredes y esquinas: no debe atravesarlas. Caminar por espacios libres y rodear los volúmenes. Inspeccionar especialmente los patios/huecos del Cabildo y del Ministerio de Trabajo.
6. Ejecutar Reload y repetir: no debe duplicar edificios, cambiar alturas ni perder colisión. Detener Play retira todo el blockout runtime.
7. Confirmar apariencia y recorrido manualmente. **M3.2 no se declara completamente aceptado antes de esa confirmación.**
