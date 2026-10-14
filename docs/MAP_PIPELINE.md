# Pipeline del mapa

## Flujo previsto
```text
datos geográficos reales
→ descarga
→ Data/raw
→ normalización
→ Data/processed
→ GeoJSON
→ importador Unreal
→ carreteras / manzanas / edificios
→ World Partition
```

La importación y generación siguen siendo futuras. M2 implementó la conversión de coordenadas; M2.5 implementa descarga de Espacios Públicos, snapshot inmutable con licencia/hashes, selección por atributos, validación y reproyección offline, y JSON del polígono/centroide de Plaza Independencia. No existe importador Unreal. Scripts, resultados y comandos en [M2_5_DATA_PROVENANCE.md](M2_5_DATA_PROVENANCE.md). No requiere Cesium. Otras fuentes se evaluarán por cobertura, calidad y licencia antes de seleccionarse.

## Etapas y entregables
1. **Selección y descarga:** definir un área pequeña alrededor de Plaza Independencia, acceso a Ciudad Vieja e inicio de 18 de Julio. Registrar consulta, extensión y parámetros; descargar solo tras aprobación de la fase correspondiente.
2. **Originales:** guardar sin alteraciones en `Data/raw/<dataset>/<version>/`, con manifiesto de procedencia. Evitar incorporar volúmenes grandes a Git sin acordar almacenamiento y distribución.
3. **Normalización:** scripts Python en `Scripts/` validarán geometrías, atributos, unidades y CRS; recortarán por área y separarán redes viales, manzanas y edificios. Registrar geometrías rechazadas y reparaciones. No asumir que todos los polígonos son edificios o que todos los cruces viales conectan al mismo nivel.
4. **Derivados:** escribir en `Data/processed/<dataset>/<version>/`, conservando enlace a los originales, parámetros, versión de herramienta e IDs estables. Particionar por sectores y asignar propietario a elementos que cruzan límites para evitar duplicados.
5. **GeoJSON:** producir colecciones por capa y sector, con esquema y propiedades documentadas. GeoJSON de intercambio usará longitud/latitud WGS84 en ese orden; no guardar metros locales bajo un GeoJSON que aparente coordenadas geográficas. El procesamiento métrico intermedio declarará su CRS en su propio formato/manifiesto.
6. **Importador Unreal futuro:** validar el manifiesto y esquema, aplicar Geospatial, convertir datos en representaciones editables y reportar errores. Será una herramienta de editor; los datos crudos no se cargarán de forma masiva en runtime. Reimportar deberá reconocer IDs estables y definir cómo preservar ajustes manuales antes de sobrescribir assets.
7. **Generación:** construir carreteras, manzanas y edificios con colisiones, escalas y materiales de blockout; alturas ausentes serán estimaciones explícitas con procedencia, no medidas reales inventadas. El grafo vial deberá distinguir sentido, conectividad y niveles cuando los datos lo permitan.
8. **World Partition:** integrar actores y sectores en el mundo particionado. Validar bordes, carga/descarga, navegación, persistencia y rendimiento. El particionado de datos de entrada no sustituye las celdas de streaming del motor.

## Manifiesto mínimo
Cada dataset y derivado conservará: identificador y versión; proveedor y URL; fecha de obtención; licencia y atribución; restricciones de uso/redistribución; CRS y orden de ejes; unidades; referencia vertical si existe; extensión espacial; checksum de originales; herramienta, versión y parámetros de procesamiento; relación con dataset padre; esquema e informes de validación.

M2.5 utiliza JSON (`source.json` y el derivado con trazabilidad al manifiesto). La licencia y atribución deberán propagarse hasta el contenido distribuido según sus condiciones. Si faltan derechos o metadatos esenciales, detener la incorporación y documentar el faltante.

## Contrato de coordenadas
La única ruta permitida será:
```text
longitud / latitud (+ altura documentada)
→ proyección métrica de trabajo elegida para el área
→ desplazamiento respecto a un origen estable
→ transformación explícita de ejes y unidades
→ posición local Unreal en centímetros
```

M2 fija entrada WGS84 geodésica 3D (semántica EPSG:4979), ECEF intermedio y marco tangente local, con X norte, Y este, Z arriba y salida en centímetros. `FGeoCoordinate` nombra explícitamente latitud y longitud: los futuros adaptadores GeoJSON deberán respetar su orden longitud/latitud al construir ese tipo. Altitud omitida significa 0 m elipsoidales; no es altura del terreno ni del nivel del mar. Radio admitido: 20 km; altura: -1000 a 10000 m. La inversa y el tratamiento de errores están implementados en C++; ver [M2_GEOSPATIAL.md](M2_GEOSPATIAL.md).

M2.5 obtiene el origen horizontal de Plaza Independencia del centroide de su polígono oficial, guardado en `Data/processed/montevideo/plaza_independencia.json`. La geometría original EPSG:32721 se usa para área/centroide y se transforma explícitamente a EPSG:4326 con orden longitud/latitud; la conversión C++ a local mantiene el contrato M2. Altitud y referencia vertical son null: la configuración permanece desactivada y no se asigna altura real cero. La prueba C++ usa dos alturas sintéticas exclusivamente para comprobar cancelación al origen. Los cambios de origen requieren recalcular posiciones; la capa no aplica rebasing ni mueve actores.

Antes de importar, probar puntos de control conocidos, orden de ejes, distancias, ida/vuelta dentro de una tolerancia acordada y continuidad entre sectores. Registrar resultados y tolerancias en unidades físicas. Evaluar precisión lejos del origen; World Partition no corrige errores de proyección.

## Criterios de validación futura
- Procedencia/licencia completas y trazabilidad entre original y derivado.
- Geometrías válidas e IDs únicos; edificios y calles alineados en puntos de control.
- Resultado reproducible con las mismas entradas, configuración y versión de herramientas.
- Reimportación sin duplicados y sin sobrescribir ajustes manuales inadvertidamente.
- Procesamiento por lotes y streaming dentro de presupuestos medidos; nunca toda la ciudad en memoria.
