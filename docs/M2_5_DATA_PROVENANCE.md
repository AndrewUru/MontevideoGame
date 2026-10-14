# M2.5 — procedencia y origen horizontal de Plaza Independencia

Completado el 14 de octubre de 2026. Se incorporó una única capa oficial y se obtuvo un origen **horizontal** verificable. No se importaron representaciones visuales, no se activó el origen en Config y no se avanzó a M3. Los 753 archivos `.uasset`/`.umap` conservan sus SHA-256 previos.

## Fuente, licencia y snapshot

Organismo: **Intendencia de Montevideo, Servicio de Geomática**. Dataset: **Espacios públicos de Montevideo**, capa `mapstore-base:cb_v_sig_espacios_publicos`.

- Referencia primaria: <https://sig.montevideo.gub.uy/>.
- Catálogo municipal: <https://ckan.montevideo.gub.uy/dataset/espacios-publicos-de-montevideo>.
- Catálogo nacional: <https://catalogodatos.gub.uy/dataset/espacios-publicos-de-montevideo>.
- API conservada: <https://catalogodatos.gub.uy/api/3/action/package_show?id=espacios-publicos-de-montevideo>.
- Metadatos oficiales: <https://geoweb.montevideo.gub.uy/geonetwork/srv/api/records/55a1f334-aada-48df-811a-ddde43ab739a/formatters/xml>.
- Descarga concreta, obtenida de esos metadatos: <https://montevideo.gub.uy/app/geoserver/ows?service=WFS&version=1.1.0&request=GetFeature&typeName=mapstore-base%3Acb_v_sig_espacios_publicos&outputFormat=SHAPE-ZIP&format_options=charset%3AUTF-8>.

El recurso antiguo del catálogo, `https://intgis.montevideo.gub.uy/sit/php/common/datos/generar_zip2.php?nom_tab=v_sig_espacios_publicos&tipo=gis`, respondió HTTP 200 con contenido no ZIP. Se descartó antes de crear el snapshot. El enlace OGC utilizado está publicado por GeoNetwork; no se dedujo un endpoint por similitud de nombres. `source.json` conserva ambos enlaces.

Licencia: **Licencia de Datos Abiertos — Uruguay, versión 0.1**, identificada como `odc-uy` en el catálogo nacional y «Licencia de DAG de Uruguay». [Texto oficial](https://www.gub.uy/agencia-gobierno-electronico-sociedad-informacion-conocimiento/sites/agencia-gobierno-electronico-sociedad-informacion-conocimiento/files/documentos/publicaciones/licencia_de_datos_abiertos_0.pdf), conservado como `license.pdf`. Permite reutilización, adaptación y redistribución comercial y no comercial. Conservar proveedor, referencia al conjunto y licencia, y declarar modificaciones. La atribución y las transformaciones se propagan al JSON derivado. Los metadatos municipales también remiten a la resolución 640/10, de 22/02/2010, sobre libre utilización de información geográfica.

Snapshot: `Data/raw/montevideo/public_spaces/2026-10-14/`. Descarga del ZIP: **2026-10-14T17:55:36.723462+00:00**. Tamaño **10.270.866 bytes**; 3.413 registros. Se conserva esta capa completa y acotada, sin descargar otras capas de la ciudad. El ZIP, catálogo, XML y licencia son cuerpos HTTP sin modificar. `source.json` es el manifiesto generado localmente. Los cinco archivos se tratan como inmutables; nuevas descargas requieren otro directorio. El ZIP contiene SHP, SHX, DBF, PRJ, CST y `wfsrequest.txt`; se leen en memoria, sin extraer sobre originales.

SHA-256 del ZIP:

```text
35fa31f9cc6a683f65dc61358db164dfaee69feb6aca02140784b4fa59dce403
```

Los hashes, tamaños, URLs, UTC y cabeceras disponibles de cada original están en [source.json](../Data/raw/montevideo/public_spaces/2026-10-14/source.json). La fecha de modificación del catálogo, 2024-08-09, no prueba la fecha de levantamiento ni la exactitud posicional de cada geometría. El WFS es dinámico: una descarga futura puede cambiar bytes, orden, timestamps del ZIP o entidades. La reproducción exacta se realiza **desde este snapshot**, no volviendo a consultar el servicio.

## Selección y resultado

Selección exclusivamente por atributos, con `strip().upper()` y coincidencia completa: `nombre_esp == INDEPENDENCIA` y `tipo_espac == PLAZA`. Se exige exactamente una coincidencia, sin escoger por cercanía ni fijar el gid en el algoritmo.

Entidad encontrada: **gid 95385783**, `nro_espaci=1405`, `cod_nom_es=2904`, `desc_tipo_=PLAZA`. Polígono válido, no vacío, 352 puntos incluyendo cierre. Los atributos originales completos están en [plaza_independencia.json](../Data/processed/montevideo/plaza_independencia.json).

CRS original: **EPSG:32721, WGS 84 / UTM zone 21S**, easting/northing en metros. Coinciden `referenceSystemInfo` del XML y el PRJ del ZIP. DBF UTF-8 según CST y la consulta oficial. No se reparan ni simplifican geometrías.

Se calcula el centroide ponderado por área y el área planar **en UTM**, después se reproyectan el centroide y los vértices a EPSG:4326 mediante PROJ, `always_xy=True`, sin transformaciones aproximadas ni descargas de grids. El JSON declara órdenes y unidades; no se presenta la geometría UTM como GeoJSON WGS84. El bbox WGS84 corresponde a los vértices reproyectados y sus segmentos; no es una medición topográfica.

| Magnitud | Resultado |
| --- | --- |
| Centroide UTM (E, N), m | 573107.6227820638, 6137032.721349981 |
| Centroide WGS84 (latitud, longitud), grados | **-34.90650688716759, -56.19975332071957** |
| Bbox UTM (Emin, Nmin, Emax, Nmax), m | 573019.3122828983, 6136980.903037938, 573197.2910940512, 6137084.186629526 |
| Bbox WGS84 (lonmin, latmin, lonmax, latmax), grados | -56.200722052199076, -34.906979287282134, -56.19877009471226, -34.906038112672 |
| Área planar aproximada | **15842.005955819597 m²** |
| Altitud / referencia vertical | **null / null** |

Se elige el centroide porque es una regla geométrica reproducible a partir del polígono oficial, y en esta entidad se comprobó que queda estrictamente dentro. No se afirma que coincida con un monumento, vértice geodésico o punto de relevamiento. Las cifras conservan precisión de cálculo; no expresan precisión de levantamiento. Si una futura geometría tiene su centroide fuera, el pipeline falla y requiere revisar el criterio; no lo reemplaza silenciosamente por otro punto.

El origen no se hardcodea en C++ ni se carga al iniciar el juego. Falta una fuente vertical fiable y su datum. No convertir `null` en altura real cero: la API M2 admite alturas numéricas, por lo que las pruebas usan alturas sintéticas explícitas, sin escribirlas en datos ni Config.

SHA-256 del derivado:

```text
5899e8627994b320c462cb59323ffa7586054ca622f51ffbfa80c7d3ff231bef
```

## Diagnóstico de «Condition failed»

Se realizó **antes de descargar el dataset**. Logs completos locales bajo `Saved/M2_5/`; extractos con números de línea, contexto y hashes en [M2_5_LOG_DIAGNOSTIC.json](M2_5_LOG_DIAGNOSTIC.json). La evidencia incluye todos los `Error:`, `Ensure condition failed` y `Assertion failed` encontrados, además de ventanas completas alrededor de cada mensaje.

| Ejecución | Condition failed | Resultado |
| --- | ---: | --- |
| Proyecto, arranque sin nuestra suite (`startup-no-tests.log`) | 14 | Aparecen durante inicialización, antes de tests del proyecto |
| Suite exacta M2 (`geospatial-tests.log`) | 14 | **9 éxitos, 0 fallos, 0 warnings de tests**, exit 0 |
| Proyecto de control sin módulos propios (`engine-control.log`) | 14 | Mismos tres smoke tests del Engine fallan |
| Mismo control con `-culture=en` (`engine-control-en.log`) | 0 | Los tres smoke tests pasan, exit 0 |

En el arranque inicial y el primer control se utilizó `-ExecCmds=quit`, que no cerró este Editor. Tras completarse la inicialización se terminaron esos procesos de diagnóstico (exit -1); no se presenta ese cierre como ejecución satisfactoria. Los comandos reproducibles siguientes usan `-TestExit="Engine Initialization"`, que sí funcionó en el control en inglés. La suite terminó automáticamente con exit 0.

Emisor: macro `CHECK` en `Engine/Source/Runtime/Core/Public/Misc/LowLevelTestAdapter.h:130`, que registra el texto genérico `Condition failed`. `FAutomationTestFramework::RunSmokeTests` y `DumpAutomationTestExecutionInfo` en `Runtime/Core/Private/Misc/AutomationTest.cpp` (aprox. 531 y 1243) ejecutan/registran las pruebas. `Runtime/Launch/Private/LaunchEngineLoop.cpp` llama a `RunSmokeTests` durante el inicio (líneas 4079 y 4376 en la instalación inspeccionada).

Pruebas identificadas aumentando la visibilidad con `-LogCmds="LogAutomationTest Log"`, sin silenciar errores:

| Prueba Core | Errores | Fuente relativa a Engine/Source/Runtime |
| --- | ---: | --- |
| `FUnifiedErrorTest_CreateErrorMessage` | 7 | `Core/Tests/Experimental/UnifiedError/UnifiedErrorTests.cpp:479`; comparaciones de texto inglés en 485–509 |
| `FUnifiedErrorTest_CreateErrorMessageWithContext` | 4 | Mismo archivo, desde 512; comparaciones en 522, 523, 533 y 534 |
| `FStructuredLogFormatTest` | 3 | `Core/Tests/Logging/StructuredLogFormatTest.cpp:21`; helpers CHECK en 183, 192 y 266, formatos localizados |

El entorno español produce textos localizados (por ejemplo «Error vacío»), mientras las pruebas de UnifiedError comparan literales ingleses. Al cambiar solo la cultura del proceso de control a inglés pasan las tres pruebas. Esto identifica una dependencia de cultura también en StructuredLogFormat; el adaptador no proporciona la expresión individual fallida, así que no se atribuyen sus tres errores a líneas de comparación más precisas sin instrumentar el Engine.

**Conclusión:** fallos de smoke tests internos de **Core/Engine UE 5.8.3**, no del template ni de `GeospatialTests`. Se reproducen sin cargar ningún módulo MontevideoGame; **no fueron introducidos por M2**. No hay Ensure/Assertion adicionales relacionados con ese bloque. El warning `LogDirectoryWatcher` del control se debe a que ese proyecto mínimo no tenía directorio Content, y es independiente. Los mensajes sobre DLL opcionales de profiling/tablet ausentes (`aqProf`, VTune, WinPix, Wintab32) son anteriores al bloque y no explican esos CHECK. No se cambió la cultura del proyecto, el motor ni la visibilidad de warnings.

## Dependencias y validaciones

Python **3.12**, dependencias offline aisladas y fijadas en [requirements-geospatial.txt](../requirements-geospatial.txt): pyshp 2.3.1 (MIT), pyproj 3.7.2 (MIT/PROJ), shapely 2.1.2 (BSD-3-Clause/GEOS LGPL); transitivas numpy 2.5.4 (BSD-3-Clause) y certifi 2026.7.22 (MPL-2.0). Justificación y alternativas en [ARCHITECTURE.md](ARCHITECTURE.md). Versiones nativas PROJ/GEOS efectivas en el derivado. Sin dependencias ni plugins adicionales de Unreal.

- Selección única, polígono válido/no vacío, CRS de XML/PRJ/manifiesto coincidentes, centroide dentro antes y después de reproyectar, coordenadas y bbox finitos dentro de rangos: correctos.
- Dos ejecuciones desde raw produjeron bytes idénticos al JSON versionado; hashes de todos los archivos bajo Data/raw iguales antes/después.
- **6/6 unittest**: ausencia/duplicación, tipo/nombre incorrectos, geometría vacía/autointersectada, centroide en hueco, original alterado y CRS desconocido son rechazados.
- Compilación `MontevideoGameEditor Win64 Development`, **UE 5.8.3 CL 58210709: Result: Succeeded, exit 0, 64.00 s**.
- `Montevideo.DataProvenance.PlazaOrigin`: **1/1 éxito**, exit 0. Python valida la fuente/derivado y pasa el centroide por argumentos al test C++ real. `FGeoReference` devuelve X=0, Y=0, Z=0 cm a alturas **sintéticas** 0 y 123 m; tolerancia 0.001 cm. Prueba explícita mediante el runner: no está incluida en el filtro original `Montevideo.Geospatial` y requiere los argumentos documentados.
- Warnings de compilación: MSVC 14.51.36260 más nuevo que el preferido 14.50.35717; C4996 en `Engine/Classes/GameFramework/Character.h:798`, override de `APawn::GetMovementBase` deprecado. No se ocultaron ni se modificó Engine. SDK 10.0.26100.0.
- El arranque de esta última prueba mantiene los 14 errores internos conocidos; el test propio no registra errores ni warnings. Logs `build.log`, `provenance-tests.log`; reportes `Automation/index.json` y `ProvenanceAutomation/index.json` bajo Saved/M2_5.
- Hashes de los **753 assets** iguales antes/después; ningún `.uasset`/`.umap`, Build.cs ni Config modificado. `git diff --check` correcto (Git advierte normalización LF/CRLF local, sin errores de whitespace).

## Reproducción (PowerShell, raíz del repositorio)

Descubrir el Engine asociado y comprobar versión; no usar una ruta supuesta:

```powershell
$projectFile = (Resolve-Path MontevideoGame.uproject).Path
$association = (Get-Content $projectFile -Raw | ConvertFrom-Json).EngineAssociation
$engineRoot = (Get-ItemProperty 'HKCU:\Software\Epic Games\Unreal Engine\Builds').$association
if (-not $engineRoot) { throw 'EngineAssociation no registrada: descubrir la instalación antes de continuar' }
Get-Content "$engineRoot/Engine/Build/Build.version"
$editorExe = "$engineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
New-Item -ItemType Directory -Force Saved/M2_5 | Out-Null
```

En esta máquina el registro resolvió `C:/Program Files/Epic Games/UE_5.8`, versión 5.8.3. Diagnóstico sin tests propios, después suite M2; conservar los logs originales si se repite en otra sesión:

```powershell
& $editorExe $projectFile -unattended -nullrhi -nosound -nosplash '-TestExit=Engine Initialization' "-abslog=$PWD/Saved/M2_5/startup-no-tests.log"
& $editorExe $projectFile -unattended -nullrhi -nosound -nosplash '-ExecCmds=Automation RunTests Montevideo.Geospatial' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD/Saved/M2_5/Automation" "-abslog=$PWD/Saved/M2_5/geospatial-tests.log"
```

Control aislado (uproject temporal sin módulos, bajo Saved ignorado; no toca el proyecto real):

```powershell
$controlDir = Join-Path $PWD 'Saved/M2_5/EngineControl'
New-Item -ItemType Directory -Force $controlDir | Out-Null
@{FileVersion=3; EngineAssociation=$association} | ConvertTo-Json | Set-Content "$controlDir/EngineControl.uproject" -Encoding UTF8
& $editorExe "$controlDir/EngineControl.uproject" -unattended -nullrhi -nosound -nosplash '-LogCmds=LogAutomationTest Log' '-TestExit=Engine Initialization' "-abslog=$PWD/Saved/M2_5/engine-control.log"
& $editorExe "$controlDir/EngineControl.uproject" -unattended -nullrhi -nosound -nosplash -culture=en '-LogCmds=LogAutomationTest Log' '-TestExit=Engine Initialization' "-abslog=$PWD/Saved/M2_5/engine-control-en.log"
python Scripts/geospatial/diagnose_logs.py
# Resultado nuevo: Saved/M2_5/diagnostic.json; comparar con la evidencia versionada.
```

Preparar herramientas y reproducir desde el snapshot conservado (no requiere red tras instalar):

```powershell
python -m venv .venv-geospatial
& ./.venv-geospatial/Scripts/python.exe -m pip install -r requirements-geospatial.txt
& ./.venv-geospatial/Scripts/python.exe Scripts/geospatial/process_public_spaces.py
& ./.venv-geospatial/Scripts/python.exe -m unittest discover -s Scripts/geospatial -p test_public_spaces.py -v
& ./.venv-geospatial/Scripts/python.exe Scripts/geospatial/validate_public_spaces.py
```

Para una **nueva** captura, después de revisar licencia/metadatos actuales; nunca sobrescribir el snapshot entregado:

```powershell
$snapshot = 'Data/raw/montevideo/public_spaces/' + (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHHmmssZ')
python Scripts/geospatial/fetch_public_spaces.py --output $snapshot
& ./.venv-geospatial/Scripts/python.exe Scripts/geospatial/process_public_spaces.py --source "$snapshot/source.json" --output Data/processed/montevideo/plaza_independencia_candidate.json
& ./.venv-geospatial/Scripts/python.exe Scripts/geospatial/validate_public_spaces.py --source "$snapshot/source.json" --processed Data/processed/montevideo/plaza_independencia_candidate.json
```

Compilar y validar la conversión real, con el Editor del proyecto cerrado:

```powershell
& "$engineRoot/Engine/Build/BatchFiles/Build.bat" MontevideoGameEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE *> Saved/M2_5/build.log
if ($LASTEXITCODE -ne 0) { throw 'Falló la compilación' }
& ./.venv-geospatial/Scripts/python.exe Scripts/geospatial/validate_public_spaces.py --unreal-editor $editorExe
if ($LASTEXITCODE -ne 0) { throw 'Falló la validación' }
git diff --check
git diff --name-only -- '*.uasset' '*.umap'
```

El runner verifica dos resultados byte a byte, compara el derivado guardado, verifica hashes raw antes/después, ejecuta el test con el centroide recién verificado y exige un reporte nuevo con exactamente un éxito y exit 0. Los argumentos `GeoTestLatitude`/`GeoTestLongitude` solo existen para pruebas de desarrollo; no configuran gameplay. Para validar además altas/bajas de assets, comparar inventarios SHA-256 de Content antes/después, como se hizo en esta entrega.

## Pendientes

Fuente vertical y datum; exactitud/actualidad cartográfica; eventual tratamiento de cambios de gid en nuevas publicaciones; integración futura del JSON como configuración de editor. La observación de cultura de los smoke tests puede reportarse a Epic con el proyecto mínimo. No corregirla escondiendo logs o modificando el Engine localmente. Antes de un blockout, acordar estos puntos y autorizar M3 por separado.
