# MontevideoGame

MontevideoGame es un videojuego de mundo abierto tipo GTA ambientado en una recreación geográfica de Montevideo, Uruguay. Se desarrollará principalmente con Unreal Engine 5 y C++, con Python para el procesamiento geográfico y VS Code como editor.

## Estado actual
M1: consolidación del proyecto C++ real creado con el template Third Person de Unreal Engine 5.8.3. El usuario validó manualmente la baseline: compilación, apertura, personaje, movimiento, cámara y salto. El inventario, comandos y resultados de verificación están en [M1_BASELINE](docs/M1_BASELINE.md). M2 no está autorizado.

## Primer vertical slice
Una zona pequeña y reconocible de Plaza Independencia, el acceso a Ciudad Vieja y el inicio de 18 de Julio. El primer recorrido permitirá caminar y correr con un personaje en tercera persona sobre un blockout basado en datos reales. Después se incorporará un primer vehículo conducible. Los criterios exactos de extensión y rendimiento se fijarán antes del blockout; no se construirá toda la ciudad de una vez.

## Organización
- `AGENTS.md`: instrucciones para futuros agentes Codex.
- `docs/ARCHITECTURE.md`: módulos conceptuales y reglas de integración.
- `docs/ROADMAP.md`: milestones M0–M11 y criterios de salida.
- `docs/MAP_PIPELINE.md`: datos reales, normalización, conversión y futura importación.
- `docs/GAMEPLAY_SYSTEMS.md`: contratos y responsabilidades de clases futuras.
- `Scripts/`: futuros scripts Python reproducibles de procesamiento y validación.
- `Data/raw/`: originales y metadatos de procedencia/licencia.
- `Data/processed/`: derivados reproducibles, manifiestos y GeoJSON.
- `Tools/`: futuras herramientas auxiliares de desarrollo, distintas de los scripts del pipeline.

Los archivos `.gitkeep` conservan directorios vacíos en Git. `Content/`, `Config/` y `Source/` contienen el proyecto real y son versionables; `Binaries/`, `Intermediate/`, `Saved/` y cachés no se versionan.
