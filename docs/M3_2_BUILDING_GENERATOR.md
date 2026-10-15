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

Se reutiliza `/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly`, unlit. Colores RGBA llegan por PrimaryColors; cubierta beige lineal (0,55; 0,49; 0,38), paredes con un factor según orientación para lectura volumétrica. Es sombreado esquemático, no materiales ni iluminación artísticos. No hay assets de materiales nuevos.

## Colisión y herramientas

Colisión compleja como simple, síncrona y estática, sobre la única malla agregada. La generación se valida antes de reemplazarla; una recarga con datos inválidos conserva la escena previa. La cocción se separa de SetMesh mediante defer temporal para medirla. No se implementan interiores; los huecos existentes permanecen abiertos y sus paredes interiores tienen colisión.

- `Montevideo.Buildings.Status`: dataset, edificios/footprints, procedencia de alturas, min/media/max, vértices/triángulos, bounds y tiempos.
- `Montevideo.Buildings.Bounds`: envolvente y etiquetas de IDs oficiales durante 30 s.
- `Montevideo.Buildings.Reload`: recarga validada sin acumular actores ni geometría.

## Tests y validación

Python cubre inmutabilidad raw, PRJ/CRS, IDs, geometrías no vacías/válidas, AOI, byte reproducibilidad, seed, política de altura, borde de cubierta exacto, área, concavidad, huecos y winding independiente. C++ cubre el dataset real, conversión, determinismo de vértices/índices, grupos, malla cerrada y normales; fixtures explícitos de concavidad/huecos/multiparte; raycasts y cápsula de personaje contra pared; recarga inválida y límites de extrusión.

La inspección visual final sigue pendiente. Los tests físicos no sustituyen una comprobación de movimiento y render en Play.

## Performance

La triangulación ocurre offline; el runtime no la repite. Los contadores separan lectura de archivos, checksum, decodificación/parseo JSON, conversión geográfica, extrusión de FDynamicMesh3, asignación al componente y cocción de colisión. El tiempo total incluye validación, material y otros costes; no es solo la suma de esos contadores. El loader de edificios reutiliza el loader geográfico y vuelve a leer metadata: ese coste real se incluye, no se oculta.

La memoria informada es una **estimación de payload CPU** de posiciones, índices, grupos y overlays; excluye capacidad reservada, adyacencia, objetos, colisión y GPU. No se presenta como memoria total del proceso ni VRAM. No se ha optimizado prematuramente.

## Limitaciones y deuda

- Cobertura parcial de construcciones notables: no completa manzanas ni implica calles flanqueadas por edificios en toda la ventana.
- Alturas exclusivamente sintéticas; terreno/datum desconocidos, base plana Z=0. Sin interiores, fachadas, ventanas ni puertas.
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
