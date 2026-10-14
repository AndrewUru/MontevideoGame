# MontevideoGame — instrucciones para agentes

## Proyecto y estado
Videojuego de mundo abierto ambientado en Montevideo, Uruguay. Tecnología principal: Unreal Engine 5 y C++; VS Code como editor de trabajo. Python se usa para herramientas de conversión, validación y procesamiento geográfico.

M0–M2.5 están completados y guardados. M3 está autorizado para un blockout horizontal 1:1 acotado a Plaza Independencia y su entorno (máximo 500 m), con fuentes oficiales IM y plano de desarrollo Z=0. Las altitudes reales siguen null; el cero numérico de conversión no es una elevación medida. Ver `docs/M3_MONTEVIDEO_BLOCKOUT.md`. M3 permanece pendiente de validación visual por el usuario; no marcarlo completo solo por tests headless. Conservar UCLASS/redirects/Blueprints Third Person. Crear o modificar `.uasset`/`.umap` requiere detenerse y obtener aprobación explícita; la implementación usa C++ y assets existentes sin editarlos. Descubrir instalación/toolchain antes de compilar. Solo dependencias incluidas en UE justificadas; sin plugins externos. Nuevas capturas oficiales acotadas nunca sobrescriben Data/raw. Sin vehículos, gameplay nuevo, terreno real ni World Partition personalizado. M4 y posteriores no están autorizados.

## Reglas de arquitectura
- C++ es la fuente de verdad del gameplay. Evitar lógica crítica exclusivamente en Blueprints. En fases posteriores, Blueprints pueden servir para presentación, configuración y composición sobre contratos C++.
- Mantener código modular, responsabilidades pequeñas e interfaces explícitas. Los módulos documentados son conceptuales; no crear módulos de compilación de Unreal sin evaluar sus dependencias.
- Tráfico, peatones, policía, vehículos y misiones deben permanecer desacoplados. Comunicar intención y eventos mediante contratos; evitar referencias globales y dependencias circulares.
- No introducir dependencias sin justificar propósito, alternativas, licencia, mantenimiento e impacto. Registrar las decisiones en `docs/ARCHITECTURE.md` antes de incorporarlas.
- No modificar automáticamente assets binarios de Unreal (`.uasset`, `.umap`, etc.). Cualquier modificación requiere una solicitud explícita y un flujo de revisión adecuado.
- Diseñar pensando en World Partition: carga por proximidad, presupuestos por sector y entidades identificables sin depender de actores cargados. Nunca intentar cargar Montevideo completa en memoria.

## Datos geográficos
- Mantener `Data/raw/` separado de `Data/processed/`. Preservar originales; generar derivados de forma reproducible y no sobrescribirlos sobre los originales.
- Conservar procedencia y licencia de cada conjunto de datos y sus derivados: proveedor, URL, fecha de obtención, licencia/atribución, CRS, extensión espacial, versión y checksum cuando corresponda. Si no se conoce la licencia, no incorporar el conjunto al juego.
- Nunca mezclar coordenadas geográficas con coordenadas locales de Unreal directamente. Usar una capa de conversión con CRS, origen, unidades, orientación y tratamiento de altura explícitos.
- Procesar por áreas acotadas o lotes. Los scripts deben declarar entradas, salidas y parámetros; evitar rutas absolutas de la máquina.
- No subir datos grandes, secretos o contenido con restricciones de redistribución sin revisar su tratamiento. No usar `.gitignore` como sustituto de una política de licencia.

## Flujo de trabajo
1. Leer `README.md` y los documentos relevantes antes de editar; respetar instrucciones más específicas si existen.
2. Mantener el alcance de la fase aprobada. No convertir propuestas futuras en implementaciones por iniciativa propia.
3. Documentar contratos y cambios de arquitectura junto al código cuando exista. Validar conversiones, streaming y persistencia con pruebas adecuadas en sus respectivas fases.
4. Revisar cambios y reportar qué se verificó y qué queda pendiente. No afirmar que Unreal compila o ejecuta sin comprobarlo.
5. Mantener `Content/`, `Config/`, `Source/`, `Scripts/` y `docs/` versionables. No versionar cachés ni productos de compilación.

## Documentación
- `docs/ARCHITECTURE.md`: límites, dependencias y decisiones.
- `docs/MAP_PIPELINE.md`: procedencia, transformación y conversión del mapa.
- `docs/GAMEPLAY_SYSTEMS.md`: responsabilidades de clases futuras.
- `docs/ROADMAP.md`: hitos y criterios de salida.
