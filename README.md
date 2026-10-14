# MontevideoGame

![MontevideoGame](https://elsaltoweb.es/wp-content/uploads/2026/10/Montevideo_-Atardecer-en-mundo-abierto.webp)

**MontevideoGame** is an open-world prototype built with **Unreal Engine 5.8 and C++**, with the goal of recreating Montevideo, Uruguay, using real-world geospatial data.

The project is being developed progressively from a real geographic foundation rather than a fictional map. Streets, public spaces and urban geometry are processed from official datasets and converted into Unreal Engine coordinates through a custom WGS84 → ECEF → local tangent coordinate system.

The first playable area is centered around **Plaza Independencia**, with the long-term goal of expanding through Ciudad Vieja, Centro, the Rambla and other areas of Montevideo.

## Current status

- Unreal Engine 5.8.3
- C++ gameplay architecture
- Third-person character
- Geospatial coordinate system
- Official Montevideo GIS data pipeline
- Plaza Independencia as geographic origin
- Procedural urban blockout in development

## Long-term goals

- Real Montevideo street network
- Procedural buildings
- Driveable vehicles
- Traffic
- Pedestrians
- Police and wanted system
- Missions
- World Partition
- Progressive expansion of Montevideo

> This is an independent technical prototype and is not affiliated with Rockstar Games or the Grand Theft Auto franchise.# MontevideoGame

MontevideoGame es un videojuego de mundo abierto tipo GTA ambientado en una recreación geográfica de Montevideo, Uruguay. Se desarrollará principalmente con Unreal Engine 5 y C++, con Python para el procesamiento geográfico y VS Code como editor.

## Estado actual
M1 está completado, validado manualmente y guardado en Git: proyecto C++ Third Person de Unreal Engine 5.8.3, movimiento, cámara y salto funcionales. Inventario en [M1_BASELINE](docs/M1_BASELINE.md). M2 añade infraestructura geoespacial WGS84 ↔ coordenadas locales Unreal; API y resultados en [M2_GEOSPATIAL](docs/M2_GEOSPATIAL.md). M2.5 incorpora la capa oficial de Espacios Públicos y el origen horizontal reproducible de Plaza Independencia, con altitud pendiente: [procedencia y comandos](docs/M2_5_DATA_PROVENANCE.md). M3 implementa el blockout C++ sobre plano Z=0 con plaza, ejes viales y manzanas oficiales; **pendiente de validación visual por el usuario**. Abrir el proyecto y pulsar Play en `Entry`; [instrucciones completas](docs/M3_MONTEVIDEO_BLOCKOUT.md). M4 no está autorizado.

## Primer vertical slice
Una zona pequeña y reconocible de Plaza Independencia, el acceso a Ciudad Vieja y el inicio de 18 de Julio. El primer recorrido permitirá caminar y correr con un personaje en tercera persona sobre un blockout basado en datos reales. Después se incorporará un primer vehículo conducible. Los criterios exactos de extensión y rendimiento se fijarán antes del blockout; no se construirá toda la ciudad de una vez.

## Organización
- `AGENTS.md`: instrucciones para futuros agentes Codex.
- `docs/ARCHITECTURE.md`: módulos conceptuales y reglas de integración.
- `docs/ROADMAP.md`: milestones M0–M11 y criterios de salida.
- `docs/MAP_PIPELINE.md`: datos reales, normalización, conversión y futura importación.
- `docs/GAMEPLAY_SYSTEMS.md`: contratos y responsabilidades de clases futuras.
- `Scripts/geospatial/`: descarga oficial, procesamiento, diagnóstico y validación reproducibles de M2.5.
- `Data/raw/`: originales y metadatos de procedencia/licencia.
- `Data/processed/`: derivados reproducibles, manifiestos y GeoJSON.
- `Tools/`: futuras herramientas auxiliares de desarrollo, distintas de los scripts del pipeline.

Los archivos `.gitkeep` conservan directorios vacíos en Git. `Content/`, `Config/` y `Source/` contienen el proyecto real y son versionables; `Binaries/`, `Intermediate/`, `Saved/` y cachés no se versionan.
