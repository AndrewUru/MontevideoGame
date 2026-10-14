# MontevideoGame

MontevideoGame es un videojuego de mundo abierto tipo GTA ambientado en una recreación geográfica de Montevideo, Uruguay. Se desarrollará principalmente con Unreal Engine 5 y C++, con Python para el procesamiento geográfico y VS Code como editor.

## Estado actual
M0: estructura y documentación inicial. Unreal Engine aún está instalándose; no hay proyecto Unreal, código de gameplay, importador ni assets descargados. La versión y las rutas del motor se confirmarán cuando esté disponible. No existen todavía comandos de compilación o ejecución del juego.

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

Los archivos `.gitkeep` conservan directorios vacíos en Git. `Content/`, `Config/` y `Source/` se incorporarán al crear el proyecto Unreal y no están ignorados. La continuación a M1 requiere aprobación del usuario y comprobar la instalación real del motor.
