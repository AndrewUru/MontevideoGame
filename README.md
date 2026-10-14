# MontevideoGame

MontevideoGame es un videojuego de mundo abierto tipo GTA ambientado en una recreación geográfica de Montevideo, Uruguay. Se desarrollará principalmente con Unreal Engine 5 y C++, con Python para el procesamiento geográfico y VS Code como editor.

## Estado actual
M1 está completado, validado manualmente y guardado en Git: proyecto C++ Third Person de Unreal Engine 5.8.3, movimiento, cámara y salto funcionales. Inventario en [M1_BASELINE](docs/M1_BASELINE.md). M2 añade infraestructura geoespacial WGS84 ↔ coordenadas locales Unreal; API y resultados en [M2_GEOSPATIAL](docs/M2_GEOSPATIAL.md). M2.5 incorpora la capa oficial de Espacios Públicos y el origen horizontal reproducible de Plaza Independencia, con altitud pendiente: [procedencia, diagnóstico y comandos](docs/M2_5_DATA_PROVENANCE.md). M3 no está autorizado.

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
