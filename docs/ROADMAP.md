# Roadmap

M1 completado en el alcance autorizado de consolidación C++ el 14 de octubre de 2026: MontevideoGameEditor Win64 Development compiló con UE 5.8.3 (salida 0); las ejecuciones headless del mapa y del GameMode nativo confirmaron la composición. Véase [M1_BASELINE.md](M1_BASELINE.md) para evidencias y límites. Se recomienda repetir la comprobación visual en PIE tras estos cambios. M2 y siguientes no están autorizados.

| Hito | Alcance | Criterio de salida |
| --- | --- | --- |
| M0 Repository | Estructura, instrucciones, arquitectura y pipeline | Archivos revisados; directorios preservados en Git; sin archivos internos de Unreal ni descargas. |
| M1 Unreal third-person project | Verificar instalación y toolchain; consolidar las tres clases C++ del proyecto Third Person existente sin modificar assets | MontevideoGameEditor compila con UE 5.8.3; conservar movimiento, cámara y salto de la baseline validada; registrar jerarquía, referencias, comandos y límites de las comprobaciones. |
| M2 Georeferenced prototype | Elegir área, fuente y licencia; definir CRS/origen; implementar conversión mínima | Puntos de control y distancias verificados; ida/vuelta dentro de tolerancia documentada; trazabilidad del dataset. |
| M3 Plaza Independencia blockout | Plaza Independencia y acceso acotado a Ciudad Vieja e inicio de 18 de Julio; blockout geográfico y World Partition | Recorrido a pie continuo con escala y colisión verificadas; carga/descarga comprobada; fijar presupuestos de rendimiento. |
| M4 First driveable vehicle | Un vehículo C++, entrada/salida y posesión | Se puede entrar, conducir y salir; validar ocupación, colisión y devolución del control a pie. |
| M5 Road generation | Pipeline vial reproducible e importador de editor acotado | Generación/reimportación sin duplicados; conectividad, cruces y bordes de sectores verificados. |
| M6 Procedural buildings | Huellas y alturas documentadas o estimadas; generación por sectores | Edificios con colisión y escala correctas; procedencia conservada; rendimiento medido. |
| M7 Traffic | Rutas y agentes de tráfico desacoplados | Circulación básica en zona cargada; límites de población y continuidad al descargar/recargar sectores. |
| M8 Pedestrians | Población peatonal y navegación | Peatones recorren zonas válidas con cupos; coexistencia básica con tráfico y streaming sin referencias inválidas. |
| M9 Police/Wanted System | Incidentes, búsqueda y respuesta policial | Nivel de búsqueda aumenta/disminuye con reglas C++; policía responde sin acoplar misiones o UI. |
| M10 Missions | Objetivos y progreso por eventos; persistencia versionada | Una misión completa conserva progreso tras guardar/cargar y descargar sectores. |
| M11 Expanded Montevideo | Incorporación progresiva de nuevas áreas | Nuevos sectores reproducibles, licencias completas y continuidad espacial; presupuestos de memoria/streaming respetados. |

El recorrido a pie de M1–M3 establece el primer slice; M4 lo amplía con conducción. M5–M6 automatizan lo que el blockout inicial puede resolver de manera acotada. SaveGame e Interaction se desarrollarán conforme los hitos los necesiten, sin esperar a M10 para definir IDs persistentes. No se comprometen fechas ni se incorpora toda Montevideo en una sola carga.
