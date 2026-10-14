# Roadmap

M1 está completado, validado manualmente por el usuario y guardado en Git. Véase [M1_BASELINE.md](M1_BASELINE.md). M2 y M2.5 completados: conversión y origen horizontal oficial verificables; la altitud permanece pendiente. Evidencia en [M2_GEOSPATIAL.md](M2_GEOSPATIAL.md) y [M2_5_DATA_PROVENANCE.md](M2_5_DATA_PROVENANCE.md). M3 está autorizado e implementado como blockout de desarrollo acotado, **pendiente de aceptación visual del usuario**; ver [M3_MONTEVIDEO_BLOCKOUT.md](M3_MONTEVIDEO_BLOCKOUT.md). M4 y siguientes no están autorizados.

| Hito | Alcance | Criterio de salida |
| --- | --- | --- |
| M0 Repository | Estructura, instrucciones, arquitectura y pipeline | Archivos revisados; directorios preservados en Git; sin archivos internos de Unreal ni descargas. |
| M1 Unreal third-person project | Verificar instalación y toolchain; consolidar las tres clases C++ del proyecto Third Person existente sin modificar assets | MontevideoGameEditor compila con UE 5.8.3; conservar movimiento, cámara y salto de la baseline validada; registrar jerarquía, referencias, comandos y límites de las comprobaciones. |
| M2 Geospatial infrastructure | WGS84 ↔ marco local Unreal; origen configurable, subsistema de GameInstance y pruebas sintéticas; sin importación visual | Compilación UE 5.8.3 correcta y pruebas geoespaciales exitosas; ejes, unidades y tolerancias documentados. Origen real y fuente/licencia pendientes si no hay datos verificados. |
| M2.5 Official source and provenance | Diagnóstico de logs, Espacios Públicos oficial y centroide de Plaza Independencia; sin importación visual | Hashes/licencia/CRS verificados; selección única; pipeline determinista; raw y assets intactos; conversión del centroide a cero comprobada. Altitud null. |
| M3 Plaza Independencia blockout | Plaza y entorno oficial de 700 × 700 m; geometría horizontal 1:1 en plano de desarrollo, sin terreno ni World Partition personalizado | Compilación, suites M2/M3, integridad y reproducibilidad; usuario confirma apariencia, orientación, escala humana y recorrido caminable en Unreal. Validación visual pendiente. |
| M4 First driveable vehicle | Un vehículo C++, entrada/salida y posesión | Se puede entrar, conducir y salir; validar ocupación, colisión y devolución del control a pie. |
| M5 Road generation | Pipeline vial reproducible e importador de editor acotado | Generación/reimportación sin duplicados; conectividad, cruces y bordes de sectores verificados. |
| M6 Procedural buildings | Huellas y alturas documentadas o estimadas; generación por sectores | Edificios con colisión y escala correctas; procedencia conservada; rendimiento medido. |
| M7 Traffic | Rutas y agentes de tráfico desacoplados | Circulación básica en zona cargada; límites de población y continuidad al descargar/recargar sectores. |
| M8 Pedestrians | Población peatonal y navegación | Peatones recorren zonas válidas con cupos; coexistencia básica con tráfico y streaming sin referencias inválidas. |
| M9 Police/Wanted System | Incidentes, búsqueda y respuesta policial | Nivel de búsqueda aumenta/disminuye con reglas C++; policía responde sin acoplar misiones o UI. |
| M10 Missions | Objetivos y progreso por eventos; persistencia versionada | Una misión completa conserva progreso tras guardar/cargar y descargar sectores. |
| M11 Expanded Montevideo | Incorporación progresiva de nuevas áreas | Nuevos sectores reproducibles, licencias completas y continuidad espacial; presupuestos de memoria/streaming respetados. |

El recorrido a pie de M1–M3 establece el primer slice; M4 lo amplía con conducción. M5–M6 automatizan lo que el blockout inicial puede resolver de manera acotada. SaveGame e Interaction se desarrollarán conforme los hitos los necesiten, sin esperar a M10 para definir IDs persistentes. No se comprometen fechas ni se incorpora toda Montevideo en una sola carga.
