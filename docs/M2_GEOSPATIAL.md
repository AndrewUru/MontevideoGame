# M2 — infraestructura geoespacial

## Alcance y estado
M2 completado el 14 de octubre de 2026: compilación correcta y nueve pruebas automatizadas exitosas. Incorpora únicamente coordenadas, conversión, origen de sesión, diagnóstico C++ y pruebas. No importa Montevideo visualmente ni añade datasets, servicios web, Cesium, OSM, parsers, plugins, assets, mapas o sistemas de gameplay. M3 no está autorizado.

Antes de editar se revisaron AGENTS, README, arquitectura, pipeline, gameplay, roadmap, M1_BASELINE, las fuentes del módulo, Build.cs y Config. Git estaba limpio. La instalación se resolvió por el GUID de `.uproject` en el registro: `C:/Program Files/Epic Games/UE_5.8`; Build.version confirma UE 5.8.3, CL 58210709. No se modifica Build.cs: Core, CoreUObject y Engine existentes cubren todo M2, incluyendo automatización; no hay dependencias nuevas.

## Arquitectura y ciclo de vida
- `FGeoCoordinate`: latitud/longitud geodésicas en grados y altura elipsoidal en metros, todo `double`. La altura es opcional en el constructor C++ y vale 0 si se omite.
- `FLocalGeoCoordinate`: `FVector Centimeters` (doble precisión en UE5), con semántica norte/este/arriba relativa al origen.
- `FGeoReference`: tipo C++ por valor con origen y base ortonormal precalculada. No depende de UObject, GameInstance, actores ni mundo para ejecutar sus operaciones; usa tipos matemáticos/estructuras reflejadas del módulo. Puede reutilizarse en herramientas futuras sin iniciar gameplay.
- `UGeospatialSubsystem : UGameInstanceSubsystem`: delega el cálculo, carga configuración al inicializarse y libera el marco al desinicializarse. Cada GameInstance tiene su propio origen; sobrevive a viajes de mapa de esa instancia. No replica, persiste a disco ni aplica rebasing.

Se mantiene un único módulo `MontevideoGame`; no se altera Character, Controller, GameMode ni su configuración de mapa. La decisión de alcance GameInstance quedó registrada antes de implementar en ARCHITECTURE. La API del subsistema se usa desde el game thread. Para procesamiento paralelo futuro, copiar un `FGeoReference` configurado por tarea y no mutarlo concurrentemente.

## Referencias espaciales y fórmulas
Entrada: WGS84 geodésico 3D, semántica EPSG:4979 (latitud, longitud, altura elipsoidal). ECEF intermedio en metros, semántica EPSG:4978. No se implementa una biblioteca de transformación de datums ni selección arbitraria de CRS; esos identificadores describen el contrato. No se modelan época/realización WGS84, deriva tectónica ni geoide.

Parámetros del elipsoide WGS84: `a = 6378137 m`, `f = 1 / 298.257223563`, `e² = f(2-f)`, `b = a(1-f)`. Son parámetros matemáticos del modelo, no coordenadas obtenidas de un dataset. El código es una implementación local de las fórmulas, sin código externo copiado ni nuevas licencias de bibliotecas. No se realizó búsqueda web ni descarga de datos geográficos.

Con latitud φ, longitud λ (convertidas de grados a radianes) y altura h:

```text
N = a / sqrt(1 - e² sin²φ)
ECEF = ((N+h) cosφ cosλ, (N+h) cosφ sinλ, (N(1-e²)+h) sinφ)
Δ = ECEF(punto) - ECEF(origen)

norte  = (-sinφ₀ cosλ₀, -sinφ₀ sinλ₀, cosφ₀)
este   = (-sinλ₀,        cosλ₀,        0)
arriba = ( cosφ₀ cosλ₀,  cosφ₀ sinλ₀, sinφ₀)

Unreal_cm = 100 * (dot(norte, Δ), dot(este, Δ), dot(arriba, Δ))
ECEF_inverso = ECEF(origen) + norte*X_cm/100 + este*Y_cm/100 + arriba*Z_cm/100
```

Orientación Unreal: **X norte, Y este, Z arriba**, consistente con la convención de mano izquierda del motor. Se permutan las componentes del marco ENU habitual; no se mezclan grados con vectores de juego. El mapa Third Person no adquiere orientación geográfica automáticamente.

La inversa ECEF → geodésicas usa `p = hypot(x,y)`, `λ = atan2(y,x)` y una iteración de latitud:

```text
φ inicial = atan2(z, p(1-e²))
φ siguiente = atan2(z + e² N(φ) sinφ, p)
h = p cosφ + z sinφ - a sqrt(1-e² sin²φ)
```

Se exige convergencia menor que `1e-14 rad` en un máximo de 16 pasos; si falla, se devuelve NumericalFailure. La fórmula de h evita dividir por cosφ cerca de polos. Para `p < 1e-8 m`, se usa latitud ±90°, altura `abs(z)-b` y longitud del origen: en el polo la longitud es indeterminada. La inversa normaliza longitud al intervalo [-180°,180°] de atan2. Las pruebas incluyen cruce del antimeridiano y ambos polos.

## API pública
Obtener la instancia mediante `GameInstance->GetSubsystem<UGeospatialSubsystem>()`.

```cpp
EGeoConversionResult SetOrigin(const FGeoCoordinate& Origin);
void ClearOrigin();
bool HasOrigin() const;
bool TryGetOrigin(FGeoCoordinate& OutOrigin) const;
EGeoConversionResult GeographicToLocal(const FGeoCoordinate& Geographic,
                                      FLocalGeoCoordinate& OutLocal) const;
EGeoConversionResult LocalToGeographic(const FLocalGeoCoordinate& Local,
                                      FGeoCoordinate& OutGeographic) const;
void LogStatus() const;
```

Las operaciones salvo LogStatus son accesibles también desde Blueprint, con implementación C++. `Initialize`/`Deinitialize` son hooks del motor. Las conversiones retornan Success, OriginNotConfigured, InvalidCoordinate, OutsideLocalDomain o NumericalFailure. Ante fallo dejan la salida intacta; SetOrigin inválido conserva el marco anterior. Primero se comprueba que exista origen. La API no devuelve un cero silencioso como sustituto de un error.

Ejemplo **sintético**, no coordenadas de Plaza Independencia:

```cpp
if (Geo->SetOrigin(FGeoCoordinate(-35.0, -56.0, 50.0)) == EGeoConversionResult::Success)
{
    FLocalGeoCoordinate Local;
    const auto Result = Geo->GeographicToLocal(FGeoCoordinate(-35.0, -56.0, 150.0), Local);
    // Usar Local solo si Result == Success: aproximadamente (0, 0, 10000) cm.
}
```

## Configuración y Plaza Independencia
El preset de desarrollo previsto se denomina **Plaza Independencia, Montevideo, Uruguay**, pero sus coordenadas y referencia vertical están pendientes. El repositorio no contiene una fuente verificable. No se inventa ni activa un origen real; los ceros por defecto del struct no son un preset.

`Config/DefaultGame.ini` contiene únicamente:

```ini
[/Script/MontevideoGame.GeospatialSubsystem]
bConfigureOriginFromConfig=False
```

Cuando exista fuente autorizada/verificada, registrar proveedor, URL, punto físico elegido, fecha/versión, licencia/atribución, CRS y referencia vertical. Entonces se podrá configurar `ConfigOrigin=(LatitudeDegrees=...,LongitudeDegrees=...,AltitudeMeters=...)` y activar `bConfigureOriginFromConfig=True`, o llamar a SetOrigin. Los puntos suspensivos son documentación, no una línea válida para copiar al ini. Un origen configurado inválido produce un warning y deja el subsistema sin origen. SetOrigin no guarda configuración a disco.

Diagnóstico en consola de PIE/juego: **`Montevideo.Geo.Status`**. Imprime origen, unidades, ejes y radio, o el estado sin configurar. No cambia datos ni necesita UI. La inicialización también registra el estado. El comando no se registra en Shipping y LogStatus no emite allí.

## Dominio y limitaciones
- Latitud finita [-90,90], longitud finita [-180,180], altura finita [-1000,10000] metros elipsoidales. No se normalizan silenciosamente entradas inválidas.
- Radio tridimensional máximo **20000 m** desde el origen (distancia ECEF, equivalente a norma local). Es una decisión conservadora de alcance urbano M2, no un límite intrínseco de ECEF. Las alturas/radio fuera del dominio se rechazan. Se admite solamente `1e-7 m` de redondeo en las fronteras inversas y se ajusta altura a su frontera cuando ese es todo el exceso.
- Las transformaciones de traslación/rotación conservan distancias de cuerda 3D; no calculan distancia geodésica sobre la superficie ni eliminan su curvatura. A 20 km, la caída respecto al plano tangente es del orden de 31 m y la diferencia de proyección horizontal respecto al arco del orden de centímetros. Un punto de altura constante generalmente tiene Z negativo lejos del origen.
- Altura omitida = 0 m sobre el elipsoide. No es dato de elevación medido, altura ortométrica ni nivel del mar. La diferencia de alturas coincide con Z solo sobre la vertical del origen. Adaptadores futuros deberán resolver altura desconocida/geoide antes de llamar a esta capa.
- Un cambio de origen invalida todas las posiciones locales anteriores; corresponde al consumidor conservar el mismo marco durante el procesamiento. No mueve actores, no corrige world-origin shifts, no crea streaming ni reinterpreta FVector de otros sistemas.
- Las tolerancias de las pruebas expresan precisión numérica, no exactitud de levantamiento topográfico. Se requieren puntos de control reales antes de importar datos. No se certifica uso global, aeronáutico ni geodésico de alta precisión.

## Pruebas automatizadas
Archivo: `Source/MontevideoGame/Tests/GeospatialTests.cpp`, compilado bajo WITH_DEV_AUTOMATION_TESTS. Suite `Montevideo.Geospatial`, nueve pruebas de automatización Unreal sin assets de prueba.

| Test | Comprobación | Tolerancia |
| --- | --- | --- |
| 01Origin | Origen ↔ cero local, incluida altura no nula | 0.001 cm; 1e-9°; 1e-5 m |
| 02RoundTrip | 15 puntos geográficos en tres marcos sintéticos | 1e-9° lat/lon; 1e-5 m altura |
| 03KnownNorth | Punto independiente de elipse meridiana desde el ecuador | Error vectorial ≤0.001 cm |
| 04KnownEast | Arco ecuatorial de 0.01°; ejes, escala y curvatura | Error vectorial ≤0.001 cm |
| 05Altitude | -1000 a 10000 m, origen a 50 m; omisión de altura | 0.001 cm; 1e-5 m |
| 06UrbanStability | 605 posiciones locales, hasta ~19.8 km, fracciones de cm y altura con signo | Máximo error 3D ≤0.001 cm |
| 07InvalidCoordinates | Sin origen, NaN, infinitos, lat/lon/altura fuera de rango, local enorme; salidas y origen intactos | Estados exactos; comparación de salidas |
| 08DomainPolesDateline | Radio 20 km, rechazo fuera de dominio, polos y cruce de ±180° | 0.001 cm; 1e-9°; 1e-5 m |
| 09SubsystemLifecycle | Dos GameInstances reales; creación automática, aislamiento, API, ClearOrigin y Shutdown | Estados exactos y mismas tolerancias numéricas |

La prueba norte usa un punto de la elipse parametrizada: `x=a*cos(t)`, `z=b*sin(t)`, `t=0.001 rad`, latitud `atan((a/b)*tan(t)) = 0.057488527532579305°`. Esperado norte `6356.7512547865135 m`, arriba `-3.1890682341943504 m`. La prueba este usa la circunferencia ecuatorial: este `1113.1949022810932 m`, arriba `-0.09714458121567937 m`. Las constantes se calcularon independientemente con Python estándar a partir de esas geometrías analíticas, no mediante la conversión implementada ni mediante una ida/vuelta. No son mediciones de Montevideo.

## Comandos y evidencia
Desde la raíz, PowerShell; no asumir una ruta del motor:

```powershell
$projectPath = (Resolve-Path ./MontevideoGame.uproject).Path
$association = (Get-Content $projectPath -Raw | ConvertFrom-Json).EngineAssociation
$engineRoot = (Get-ItemProperty 'HKCU:\Software\Epic Games\Unreal Engine\Builds').$association
if (-not $engineRoot) { throw 'Instalación asociada no encontrada' }
Get-Content (Join-Path $engineRoot 'Engine/Build/Build.version')
& (Join-Path $engineRoot 'Engine/Build/BatchFiles/Build.bat') MontevideoGameEditor Win64 Development $projectPath -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw 'Compilación fallida' }
& (Join-Path $engineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $projectPath -unattended -nullrhi -nosound -nosplash '-ExecCmds=Automation RunTests Montevideo.Geospatial' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD/Saved/M2/Automation" "-abslog=$PWD/Saved/Logs/M2-GeospatialTests.log"
if ($LASTEXITCODE -ne 0) { throw 'Automatización fallida' }
```

Comprobar también que el informe contiene los nueve tests y cero fallos: el código de salida por sí solo no demuestra que se haya encontrado la suite. Logs, informe y manifiestos SHA-256 permanecen en Saved, fuera de Git.

### Resultado verificado
- Compilación final: `MontevideoGameEditor Win64 Development`, UE **5.8.3**, **Result: Succeeded**, salida **0**, **15.07 s**. Log `Saved/M2/build-final.log`. MSVC 14.51.36260, Windows SDK 10.0.26100.0, .NET 10.0 incluido en el motor.
- Automatización final: salida **0**, **9 Success**, **0 failed**, **0 notRun**, **0 succeededWithWarnings**. Las nueve filas de la tabla anterior pasaron individualmente con cero errores y warnings en sus entradas del informe. Duración de ejecución de tests 0.102326 s (no incluye inicio del editor). Informe `Saved/M2/Automation/index.json`, log `Saved/Logs/M2-GeospatialTests.log`.
- UrbanStability: **605 muestras**, error 3D máximo **3.1181025294e-7 cm**, frente a tolerancia **0.001 cm**.
- SHA-256 de **753 paquetes** `.uasset`/`.umap`: **0 modificados, 0 eliminados, 0 añadidos**. Manifiestos `Saved/M2/asset-hashes-before.json` y `asset-hashes-after.json`.
- Build.cs y las clases de gameplay de M1 permanecen sin cambios. Revisión de diff y whitespace correcta.

### Warnings, incidencias corregidas y límites
- El motor advierte que MSVC 14.51.36260 es posterior a la versión preferida 14.50.35717. No se cambió el compilador.
- La primera compilación también mostró el C4996 preexistente del header del motor `Character.h:798` por `APawn::GetMovementBase`. El último build incremental no recompiló ese header; no se afirma haber eliminado el warning.
- Se corrigieron dos defectos del montaje de pruebas: flags de automatización declarados con el tipo incorrecto y GameInstances creadas sin `GEngine` como Outer. El primer recorrido pasó los ocho tests matemáticos y abortó en el noveno; el informe final corresponde a una ejecución completa posterior con el montaje corregido y limpieza de mundos/contextos temporales.
- El log global del editor emite **14 líneas `LogAutomationTest: Error: Condition failed` durante inicialización, antes de ejecutar esta suite**. No aparecen como errores de los nueve tests en el informe final, que es exitoso. Su causa concreta dentro de la inicialización del motor no quedó determinada; se registra como deuda del entorno, sin ocultar mensajes ni modificar el motor. No se presenta el log global como libre de errores.
- No se probaron datos geográficos reales, precisión topográfica, importación, packaging ni nuevos mapas. No se repite la prueba visual de M1: el cambio no conecta coordenadas al personaje ni modifica su comportamiento.

## Deuda y siguiente paso
Origen real de Plaza Independencia, fuente/licencia, datum vertical/alturas, puntos de control reales y área exacta quedan pendientes. No hay adaptación de otros CRS ni importadores. Antes de M3 deben resolverse esos datos y acordarse la representación de curvatura/terreno y la integración de coordenadas con los sectores del mundo. Este trabajo no autoriza empezar M3.
