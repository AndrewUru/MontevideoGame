# M3 — blockout horizontal de Montevideo

**Estado: implementación C++ y validación automática; aceptación visual pendiente del usuario. M3 no está completado. M4 no está autorizado.**

La escena se genera al iniciar Play sobre el mapa `Entry` ya incluido en Unreal. No se crearon ni modificaron mapas, Blueprints, materiales ni otros assets binarios. Se conserva el Third Person de M1, incluyendo su mapa, sus Blueprints, cámara, movimiento, salto y Enhanced Input. El único cambio de selección de escena está en `Config/DefaultEngine.ini`.

## Fuentes y alcance

Fuentes primarias: [SIG de la Intendencia](https://sig.montevideo.gub.uy/), su [WFS GetCapabilities](https://montevideo.gub.uy/app/geoserver/ows?service=WFS&version=1.1.0&request=GetCapabilities) y los enlaces de metadatos anunciados en él. `fetch_m3.py` busca títulos exactos en GetCapabilities, toma los nombres de capa anunciados, sigue MetadataURL y verifica que el enlace SHAPE-ZIP del XML corresponda a esa misma capa. No supone nombres de layers.

| Elemento | Fuente/capa | Entidades utilizadas |
| --- | --- | ---: |
| Plaza | M2.5, `mapstore-base:cb_v_sig_espacios_publicos`, `gid=95385783` | 1 polígono |
| Ejes viales | `mapstore-base:cb_v_sig_vias` — Ejes de Calles | 160 líneas |
| Manzanas | `mapstore-base:cb_v_sig_manzanas_materializadas` — Manzanas materializadas | 66 polígonos |

Metadatos oficiales preservados:

- [Ejes de Calles](https://geoweb.montevideo.gub.uy/geonetwork/srv/api/records/14264354-5ed5-4ecf-86e8-7d2188af59bc/formatters/xml): ejes ajustados con imágenes de vuelos 2021/2024 y actualización continua; no equivalen a anchuras de calzada.
- [Manzanas materializadas](https://geoweb.montevideo.gub.uy/geonetwork/srv/api/records/d652902a-4878-449a-9aab-ad5fa1865e45/formatters/xml): límites de propiedad aparentes; no son huellas ni alturas de edificios. No se sustituyeron por la capa distinta de manzanas catastrales.

Organismo: **Intendencia de Montevideo, Servicio de Geomática**. Los XML de ambas nuevas capas declaran **uso libre según resolución IM 640/10, 22/02/2010**; se conserva el texto legal exacto, atribución y URL en el manifiesto. Véase también [información del Geoportal](https://sig.montevideo.gub.uy/acerca.html). La plaza conserva su licencia Datos Abiertos Uruguay 0.1 y su atribución de M2.5. No se asigna silenciosamente la licencia de otra capa a las manzanas materializadas.

Snapshot nuevo: `Data/raw/montevideo/m3/2026-10-14/`. Contiene `capabilities.xml`, `roads-metadata.xml`, `blocks-metadata.xml`, `roads.zip`, `blocks.zip` y `source.json`. Las URLs concretas, BBOX, CRS, fechas UTC, tamaños y hashes están en [source.json](../Data/raw/montevideo/m3/2026-10-14/source.json). Los archivos descargados se conservan byte a byte. Los snapshots M2.5 permanecen intactos.

| Original nuevo | Tamaño | SHA-256 |
| --- | ---: | --- |
| `roads.zip` | 20.266 bytes | `9411258be940b4e66a192559ce64a64773853d764f50cec185b365b6c0367f81` |
| `blocks.zip` | 103.145 bytes | `397460e1adb01006d8b72211323832c8f451d8ea536cdbe0af71ead73b8f5cf7` |

Las dos consultas llevan BBOX en **EPSG:32721** y límite de seguridad de 2.000 entidades; el pipeline rechaza una respuesta que alcance ese límite. Solo se solicitaron entidades intersectantes de un cuadrado UTM de **700 × 700 m, 0,49 km²**, centrado en el origen oficial (esquinas a ~495 m). Después se recortan exactamente al cuadrado. No se descargó la ciudad completa, ni edificios, terreno u otras capas.

Extensión de consulta/recorte UTM (Emin, Nmin, Emax, Nmax), metros:

```text
572757.6227820638, 6136682.721349981, 573457.6227820638, 6137382.721349981
```

## Origen, ejes y alturas

La única fuente numérica del origen es [plaza_independencia.json de M2.5](../Data/processed/montevideo/plaza_independencia.json): latitud **-34.90650688716759**, longitud **-56.19975332071957**. El manifiesto M3 guarda ruta y hash; no duplica el centroide ni lo incrusta en C++. El renderer no conoce el nombre geográfico de la plaza.

Contrato: WGS84 (longitud, latitud en los arrays JSON) → `FGeoReference` → X norte, Y este, Z arriba, **100 cm/m**. Se utiliza h=0 exclusivamente como referencia matemática; después se aplana Z local a cero. El dato de altitud y su referencia siguen **null**. El subsistema mantiene la referencia matemática de sesión; su comando de estado puede mostrar h=0, que aquí no debe interpretarse como elevación real.

| Medida | Resultado |
| --- | --- |
| Bbox local total (cm), mín. | X=-35275.56214009199, Y=-35286.77515684422, Z=0 |
| Bbox local total (cm), máx. | X=35274.89479707801, Y=35290.41774079281, Z=0 |
| Envolvente local total | **705,505 × 705,772 m** |
| Bbox plaza (cm), mín. | X=-5240.781229298716, Y=-8853.442639023588, Z=0 |
| Bbox plaza (cm), máx. | X=5200.507447208351, Y=8985.868129837268, Z=0 |
| Dimensiones plaza norte-sur × este-oeste | **104,413 × 178,393 m** |
| Área plaza en UTM | **15842,005955819597 m²** |
| Área plaza en plano tangente local | **15852,597786 m²** |

El bbox local del cuadrado es mayor que 700 m por la rotación entre norte de cuadrícula UTM y norte geodésico, y por el factor de proyección. La diferencia de área (~0,067 %) también corresponde a esa conversión; no se aplica una escala arbitraria. Los tests comparan el resultado C++ con PROJ independiente a 0,1 cm y comprueban el área con tolerancia de 0,2 % para la plaza.

## Pipeline y contrato

`fetch_m3.py` → snapshot raw → `process_m3.py` → `Data/processed/montevideo/m3/`:

- `plaza_independencia.json`: una plaza, **351 vértices**, 349 triángulos; se elimina únicamente el cierre duplicado del anillo para el índice de malla.
- `roads.json`: **160 líneas, 1.394 vértices**.
- `blocks.json`: **66 polígonos, 2.376 vértices**.
- `manifest.json`: referencias al origen y fuentes, hashes, fecha reproducible, versiones, CRS, área, límites esperados y conteos.

Total cartográfico: **67 polígonos, 160 líneas, 4.121 vértices, 2.579 triángulos de superficie**. Los trazos y flechas de debug agregan geometría de presentación y no cuentan como entidades cartográficas.

Se validan hashes originales/PRJ, se recorta en UTM y se triangula con `shapely.constrained_delaunay_triangles`, manteniendo huecos y comprobando que la cobertura coincide con el polígono. No se reparan ni simplifican polígonos. La conversión local de referencia en Python se calcula con PROJ cart/topocentric, independiente del código C++.

Algunos triángulos de manzanas entre vértices casi coincidentes colapsan al reproyectar. Se excluyen **14 triángulos numéricos**, cada uno de área ≤1 mm² tanto en UTM como en local; área total excluida UTM **9,83950374369995e-7 m²**. Todos los vértices y anillos se conservan. Cantidad y área excluida se registran por feature. Una inversión o colapso de área mayor detiene el pipeline. No se redujo la comprobación C++ de winding para tolerar errores.

Los arrays de vértices usan WGS84; índices `triangles` y `rings` referencian esos arrays. Triángulos del contrato tienen cross product +Z al convertir a X norte/Y este. El renderer adapta su orden a la convención de render/colisión de Unreal (horario visto desde arriba); `UDynamicMeshComponent` invierte normales de colisión internamente. La prueba de raycast verifica una normal hacia arriba y el impacto en el polígono real a Z=0, no solo en el soporte.

`.gitattributes` conserva las reglas LFS de Unreal y fija `Data/raw/** -text`, `Data/processed/** text eol=lf`. Esto evita que autocrlf invalide hashes al hacer checkout. Se detectó normalización previa en el almacenamiento Git de los XML nuevos, `source.json` M3 y `metadata.xml` M2.5. Se prepararon esos cuatro archivos en el índice con sus bytes originales mediante `git add --renormalize -- Data/raw`, junto con los atributos; **no se modificó ningún byte de los archivos raw de trabajo ni se creó un commit**. Se comparó cada blob raw preparado con su original byte a byte. Los diffs de finales de línea de esos cuatro archivos restauran fidelidad de almacenamiento, no son transformaciones de los datos oficiales.

La fecha `processed_at_utc` es un parámetro UTC explícito, capturado al ejecutar el lote de procesamiento y registrado en el manifiesto. Las reconstrucciones conservan ese parámetro; no lo reemplazan por el reloj ni por la fecha de descarga. Si no existe un manifiesto previo, se exige `--processed-at-utc`. La reproducción requiere los mismos originales, versiones y parámetro de fecha. Los SHA-256 completos están en manifest/source. C++ usa adicionalmente SHA-1 portable de Core para detectar archivos desactualizados antes de cargar; la comprobación de procedencia SHA-256 vive en Python. Estos checksums no son firmas de confianza.

## Arquitectura y representación Unreal

- `FGeospatialBlockoutLoader`, en `Geospatial/`: lectura JSON acotada, rutas dentro del proyecto, checksums, origen, CRS, índices, radio, winding, área, counts y bbox; convierte con `FGeoReference`. No crea actores. El resultado anterior no se reemplaza en caso de error.
- `AGeospatialBlockoutActor`, en `World/`: dos `UDynamicMeshComponent` transitorios. Superficies poligonales con colisión compleja como simple y cocción síncrona; trazos sin colisión. Render basado en roles de datos, sin coordenadas/nombres de lugares hardcodeados.
- `AMontevideoBlockoutGameMode : AMontevideoGameGameMode`: compone escena, luces y PlayerStart al iniciar el juego; reutiliza `BP_ThirdPersonCharacter` y `BP_ThirdPersonPlayerController`. No modifica sus assets ni añade gameplay.
- Se reutilizan `/Engine/Maps/Entry`, `/Engine/BasicShapes/Cube` y `/Engine/EngineDebugMaterials/VertexColorMaterial`, todos preexistentes. `Entry` está vacío antes de Play porque la escena se construye durante InitGame.
- Config selecciona `Entry` para editor/juego y `/Script/MontevideoGame.MontevideoBlockoutGameMode` como GameMode global. `Lvl_ThirdPerson` conserva su override Blueprint y sirve para volver a comprobar el template.

Dependencias nuevas privadas en el único módulo existente: **Json, GeometryCore, GeometryFramework**, incluidas con UE 5.8.3. Se verificaron headers/APIs en la instalación real; no se habilitó ProceduralMeshComponent ni ningún plugin. Justificación previa en [ARCHITECTURE.md](ARCHITECTURE.md). No se añadieron dependencias Python respecto a M2.5.

Presentación:

- Plaza verde; manzanas azul/gris planas; perímetros claros; ejes viales amarillos.
- Trazo de calles de 30 cm y perímetro de 15 cm: símbolos gráficos, **no anchos medidos**. Z=1 cm evita z-fighting y no representa elevación.
- Un cubo del Engine escalado proporciona soporte neutral bajo el área, con cara superior Z=-0,5 cm. Es un plano de ingeniería ficticio, no terreno ni inferencia de cobertura urbana. Las superficies oficiales están en Z=0.
- Flechas de referencia en el origen: roja +X norte, verde +Y este, ambas de **20 m**. Son ayudas métricas, no cartografía.
- PlayerStart en (0,0,120) cm, mirando al este. El centroide está dentro de la plaza; los 120 cm colocan la cápsula sobre el plano, no indican altitud. Movimiento original a 500 cm/s, cámara y salto intactos.

## Comandos de desarrollo

Ejecutar durante Play/Standalone desde consola o Output Log:

| Comando | Resultado |
| --- | --- |
| `Montevideo.Geo.Status` | Referencia matemática de la GameInstance, ejes y unidades |
| `Montevideo.M3.Status` | Origen/fuente, datasets activos, polígonos/líneas/vértices, bbox y dimensiones |
| `Montevideo.M3.Bounds` | Estado más bbox y etiquetas norte/este durante 30 segundos |
| `Montevideo.M3.Reload` | Revalida/reconstruye la geometría; rechaza datos erróneos conservando la escena anterior |

Reload no crea assets ni acumula actores. Si se cambia una fuente, regenerar primero los derivados y manifiesto; no modificar checksums manualmente. No se implementa cambio de región ni viaje del jugador.

## Reproducir y validar automáticamente

PowerShell desde la raíz del repositorio, Editor cerrado para compilar:

```powershell
python -m venv .venv-geospatial
& ./.venv-geospatial/Scripts/python.exe -m pip install -r requirements-geospatial.txt
& ./.venv-geospatial/Scripts/python.exe Scripts/geospatial/process_m3.py
& ./.venv-geospatial/Scripts/python.exe -m unittest discover -s Scripts/geospatial -p test_m3.py -v
if ($LASTEXITCODE -ne 0) { throw 'Falló el pipeline M3' }
```

Las cinco pruebas Python comprueban: dos ejecuciones byte-idénticas y coherentes con los archivos guardados; raw/Content intactos; origen único y altitudes null; plaza/área/winding/bbox; extensión y conteos. No requieren red tras preparar el entorno. El snapshot se valida por SHA-256.

Nueva captura, solo cuando se quiera actualizar el lote; nunca sobrescribir el original:

```powershell
$snapshot = 'Data/raw/montevideo/m3/' + (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHHmmssZ')
& ./.venv-geospatial/Scripts/python.exe Scripts/geospatial/fetch_m3.py --output $snapshot
$processingUtc = (Get-Date).ToUniversalTime().ToString('o')
& ./.venv-geospatial/Scripts/python.exe Scripts/geospatial/process_m3.py --source "$snapshot/source.json" --processed-at-utc $processingUtc
```

Descubrir instalación y compilar:

```powershell
$projectFile = (Resolve-Path MontevideoGame.uproject).Path
$association = (Get-Content $projectFile -Raw | ConvertFrom-Json).EngineAssociation
$engineRoot = (Get-ItemProperty 'HKCU:\Software\Epic Games\Unreal Engine\Builds').$association
if (-not $engineRoot) { throw 'EngineAssociation no registrada; localizar instalación antes de continuar' }
Get-Content "$engineRoot/Engine/Build/Build.version"
New-Item -ItemType Directory -Force Saved/M3 | Out-Null
& "$engineRoot/Engine/Build/BatchFiles/Build.bat" MontevideoGameEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE *> Saved/M3/build.log
if ($LASTEXITCODE -ne 0) { throw 'Falló la compilación' }
& "$engineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $projectFile -unattended -nullrhi -nosound -nosplash '-ExecCmds=Automation RunTests Montevideo.Geospatial+Montevideo.M3' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD/Saved/M3/Automation" "-abslog=$PWD/Saved/M3/tests.log"
$report = Get-Content Saved/M3/Automation/index.json -Raw | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or $report.failed -ne 0 -or $report.succeeded -ne 14 -or $report.succeededWithWarnings -ne 0 -or $report.notRun -ne 0) { throw 'Fallaron tests M2/M3: revisar reporte, no solo exit code' }
```

**Importante:** esta versión del motor puede cerrar con código 0 aun si falla una prueba. Siempre se inspecciona `index.json`, no solo el código de salida. Las suites contienen las nueve pruebas M2 y cinco M3: geometría oficial, origen/ejes/escala, rechazo de corrupción/inversión, radio/ejes incorrectos, colisión y recarga. La prueba M2.5 con argumentos externos se mantiene separada.

## Resultado técnico de esta entrega

- Engine descubierto por registro/EngineAssociation: **UE 5.8.3 CL58210709**, `C:/Program Files/Epic Games/UE_5.8`.
- Compilación final **MontevideoGameEditor Win64 Development: Result: Succeeded, exit 0, 60,38 s**. Los errores de integración detectados durante el desarrollo se corrigieron antes de esta compilación.
- Reporte de automatización: **14/14 éxitos (M2 9/9, M3 5/5), 0 fallos, 0 warnings de tests**. El test físico impacta `OfficialPolygons`, posición (0,0,0), normal (0,0,1); la recarga válida conserva conteos y una inválida conserva la escena previa.
- Python **5/5**, dos reconstrucciones byte-idénticas; hashes raw/Content sin cambios.
- Arranque standalone headless con Config por defecto: **exit 0**, mapa Entry, GameMode M3 y 67 polígonos/160 líneas cargados. Una segunda comprobación con `obj list class=BP_ThirdPersonCharacter_C` confirmó la instancia `Entry:PersistentLevel.BP_ThirdPersonCharacter_C_0`. No sustituye una comprobación visual.
- Los **753 `.uasset`/`.umap` existentes** conservan exactamente sus SHA-256 e inventario. Los originales anteriores M2.5 y los nuevos snapshots permanecen intactos. No se creó ningún asset binario Unreal.
- Warnings de compilación: MSVC 14.51.36260 no preferido (UE prefiere 14.50.35717); C4996 internos en GeometryFramework `BaseDynamicMeshComponent.h:678` (`GetMaterialRelevance`) y Chaos `Sphere.h:74`, `Convex.h:207/212`, `Plane.h:42`, `Box.h:104` (`GetRadius`/`GetMargin`). En la compilación inicial del módulo también apareció el C4996 conocido de `Character.h:798`. No se suprimieron warnings ni se modificó el Engine.
- Los **14 `Condition failed` de Core** aparecen antes de la suite Editor, como en M2.5; no son fallos de estas 14 pruebas. El arranque standalone registra warnings de `LogEditorDataStorageUI` sobre propósitos de widget no registrados; no hay errores del blockout en ese arranque.
- Evidencia compacta y hashes de logs en [M3_VALIDATION.json](M3_VALIDATION.json). Logs completos locales: `Saved/M3/build.log`, `tests.log`, `standalone.log`, `spawn.log`; reporte `Saved/M3/Automation/index.json`. La comprobación de whitespace de código/documentación es correcta. `git diff --cached --check` señala CRLF/espacios originales en los cuatro raw cuya representación Git se restauró; se conservan deliberadamente para no alterar los cuerpos originales ni sus hashes. No se ocultaron esos avisos ni se aplicó formateo a raw.

## Archivos de M3

| Ubicación | Archivos creados/modificados |
| --- | --- |
| Raíz | `.gitattributes` (conserva reglas LFS existentes y añade preservación de bytes de datos) |
| Config | `DefaultEngine.ini` |
| Source/MontevideoGame | `MontevideoGame.Build.cs` |
| Source/MontevideoGame/Geospatial | `GeospatialBlockoutData.h`, `.cpp` |
| Source/MontevideoGame/World | `GeospatialBlockoutActor.h`, `.cpp`; `MontevideoBlockoutGameMode.h`, `.cpp` |
| Source/MontevideoGame/Tests | `GeospatialBlockoutTests.cpp` |
| Scripts/geospatial | `fetch_m3.py`, `process_m3.py`, `test_m3.py` |
| Data/raw/montevideo/m3/2026-10-14 | `capabilities.xml`, `roads-metadata.xml`, `blocks-metadata.xml`, `roads.zip`, `blocks.zip`, `source.json` |
| Data/raw/montevideo/public_spaces/2026-10-14 | `metadata.xml`: solo representación en el índice Git; bytes originales de trabajo sin cambios |
| Data/processed/montevideo/m3 | `plaza_independencia.json`, `roads.json`, `blocks.json`, `manifest.json` |
| Documentación | `AGENTS.md`, `README.md`, `docs/ARCHITECTURE.md`, `docs/GAMEPLAY_SYSTEMS.md`, `docs/MAP_PIPELINE.md`, `docs/ROADMAP.md`, este documento y `docs/M3_VALIDATION.json` |

La actualización de README se limita al estado M3 en la sección española; se preservaron los cambios introductorios concurrentes del usuario. Los archivos M2.5 de datos/origen no se reescribieron.

## Validación visual — pendiente del usuario

1. Abrir `MontevideoGame.uproject` con Unreal **5.8.3**, después de la compilación. El mapa de inicio es `Entry`; antes de Play puede verse vacío. No guardar ese mapa del Engine.
2. Pulsar **Play → Selected Viewport**. Debe aparecer el personaje Third Person dentro de la plaza verde, con manzanas azul/gris y ejes amarillos alrededor. Hacer clic en el viewport para capturar entrada. WASD, ratón y Espacio deben conservar sus funciones.
3. Si Unreal restauró `Lvl_ThirdPerson` de una sesión anterior, abrir el mapa existente `Engine/Maps/Entry` (activar **Show Engine Content** en Content Browser, buscar `Entry`, abrirlo) y pulsar Play. No crear ni guardar un nivel nuevo.
4. Ejecutar `Montevideo.M3.Status` y `Montevideo.Geo.Status`. Confirmar 67 polígonos/160 líneas/4.121 vértices, dimensiones ~705,5 × 705,8 m, origen oficial y aviso de plano ficticio.
5. Ejecutar `Montevideo.M3.Bounds`. La flecha roja es norte (+X), la verde este (+Y), ambas 20 m. Con el personaje original a máxima marcha, recorrer 20 m tarda aproximadamente cuatro segundos, sin contar aceleración.
6. Caminar y saltar sobre la plaza, cruzar su perímetro y recorrer espacios entre manzanas. No debe caer, atravesar la plaza ni quedar bloqueado por las líneas. El entorno es plano: no esperar veredas, fachadas o relieve real.
7. Para inspección aérea: durante PIE pulsar **F8** (eject), seleccionar `GeospatialBlockoutActor` en World Outliner y **F** para encuadrar; alejarse/subir hasta ver la zona completa. No guardar actores del PIE. Comparar el contorno y la orientación de calles/manzanas con el SIG oficial o los derivados. La plaza mide aproximadamente 104,4 m norte-sur por 178,4 m este-oeste; el personaje debe verse a escala humana frente a esa extensión.
8. Ejecutar `Montevideo.M3.Reload`; no debe duplicar geometría ni cambiar escala. Volver a Play/posesión y comprobar que se sigue caminando. Detener Play debe retirar los actores transitorios.
9. Confirmar explícitamente apariencia, escala, ejes y recorrido. **Hasta esa confirmación, M3 sigue pendiente.**

Alternativa de arranque directo sin depender del mapa restaurado del Editor (variables de descubrimiento anteriores):

```powershell
& "$engineRoot/Engine/Binaries/Win64/UnrealEditor.exe" $projectFile '/Engine/Maps/Entry?game=/Script/MontevideoGame.MontevideoBlockoutGameMode' -game -windowed -ResX=1280 -ResY=720
```

Para revisar el template original: abrir `Content/ThirdPerson/Lvl_ThirdPerson` y Play. Su GameMode override y assets no se han modificado. No hay que borrar el blockout ni cambiar Blueprints.

## Limitaciones y deuda técnica

- Referencia vertical, alturas y terreno real pendientes. El soporte no representa el suelo real fuera de los polígonos oficiales.
- Ejes viales no proporcionan calzadas, sentidos, anchos o aceras en esta representación. Manzanas no equivalen a edificios; no se generan fachadas.
- Datos reales con distintas fechas/precisiones; el área proyectada no es una medición de superficie topográfica.
- Escena de desarrollo de escritorio/Editor: lectura síncrona y colisión síncrona para un lote pequeño; sin streaming, LOD, empaquetado/staging automático de Data, navegación ni multijugador. No se afirma compatibilidad con un build empaquetado sin preparar staging de los JSON.
- Reload valida antes de reemplazar, pero no es un sistema de migración de origen con jugadores/actores persistentes. Mantener esta escena acotada.
- Los warnings internos de Engine/toolchain se registran sin ocultarlos; los 14 smoke-test `Condition failed` anteriores a la suite tienen el diagnóstico de M2.5, independiente del blockout.
- Validación visual y rendimiento con render real pendientes; los tests headless no los sustituyen.
