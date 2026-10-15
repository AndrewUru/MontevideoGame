# M3.3.1 — Facade Optimization

**Estado: implementación y validación técnica completadas; aceptación visual pendiente del usuario.** M3.3 fue aceptado técnica y visualmente antes de iniciar esta subfase. No autoriza M3.4 ni M4.

## Problema medido en M3.3

El layout lógico contiene 9 edificios, 12 footprints, 506 segmentos exteriores, 430 bays, 1.762 ventanas, 9 puertas, 465 balcones y 13.362 piezas rectangulares. `FFacadeMeshGenerator` generaba una caja independiente para cada pieza: **106.896 vértices únicos**, **160.344 triángulos únicos** y **22.448.160 bytes (~21,41 MiB) de payload CPU estimado** en un `UDynamicMeshComponent`. La duplicación nace en los 13.362 bucles de 8 vértices y 12 triángulos, no en el loader GIS ni en el massing M3.2. La medición standalone M3.3 fue: layout 97,093 ms; mesh 448,822 ms; asignación 1,014 ms; total 547,048 ms. Una recarga del mismo proceso tardó 990,749 ms. Estos son puntos de referencia, no benchmarks estadísticos.

Antes había un componente de fachada, una malla, una sección y un material Engine debug. La colisión pertenecía al massing separado. Los datos M3.3 se conservan como contrato de comparación.

## Arquitectura elegida

`FBuildingFootprintData` → `FFacadeLayoutGenerator` **sin cambios** → `FFacadeInstanceBatches` → `FFacadeModuleLibrary` → `AFacadeBlockoutActor` con `UInstancedStaticMeshComponent` (ISM). La malla original `FFacadeMeshGenerator` permanece disponible para los tests de referencia, pero ya no se genera durante Play. El GameMode y el actor/mesh de massing no cambian.

`FFacadeInstanceBatches` crea transformaciones de **cada `FFacadeElement` existente**; `ElementIndices` conserva el vínculo a segmento, estilo, floor, source_feature_id, BuildingId y seed del footprint. `CompareLegacy` regenera el layout usando las mismas reglas y compara campos lógicos, colores e índices; también comprueba dos esquinas transformadas de cada pieza (tolerancia de 0,02 cm por redondeo float de la rotación). No requiere dos renderers simultáneos. No cambia recuentos, estilo, bays ni geometría GIS.

Un prisma unitario transitorio usa coordenadas locales X=[0,1], Y=[-1,0], Z=[0,1]. La rotación X=tangente/Z=arriba, la traslación y la escala positiva reproducen los rangos U/D/Z del layout. La cara Y negativa es exterior. El origen y la altura reales continúan en el massing; la fachada usa solo las coordenadas locales M3.3.

`FFacadeModuleLibrary` construye fuentes `UStaticMesh` transitorias con `FMeshDescription`, `BuildFromMeshDescriptions(bFastBuild)`, sin colisión ni MeshDescription persistido. Los colores de vértices reutilizan `/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly`. No se guarda ningún `.uasset`. Los colores de estilo y las piezas opacas/oscuras/clara de M3.3 se conservan; el sombreado lateral fino de la malla antigua dependía de la normal mundial de cada caja. Una fuente compartida no puede codificar ese matiz diferente por orientación sin un material nuevo. Se usa un factor lateral neutro y uno superior para mantener lectura parecida; **la equivalencia visual precisa requiere inspección del usuario**. El cambio no altera offsets ni posiciones.

### Módulos y batches

Son cajas simples tintadas por estilo o papel. `WindowSimple` es el panel oscuro; `WindowFrame` son cuatro instancias de marco por ventana. `DoorSimple` comparte forma y tono oscuro, pero mantiene batch/identidad propios. `BalconySlab` y `BalconyRail` componen un balcón con cuatro instancias. `CorniceSimple` se escala a la longitud del edge. Piel, planta baja y banda también se instancian porque son cajas rectangulares escalables. El número de fuentes únicas puede ser menor que el número de batches: colores iguales comparten malla fuente.

Slots de batches estables: 4 pieles por estilo, 4 plantas bajas por estilo, banda, ventana, marco, puerta, losa, baranda y cornisa: **máximo 15**. Solo los slots no vacíos necesitan componente. Las piezas del lote actual usan Classic, Residential y Civic; Modern se puede probar con `Style 1`. Un batch usa una sección y el único material debug. Cada índice de instancia se corresponde con un `ElementIndex`; no se crean actores por ventana ni por balcón.

### ISM y HISM

ISM permite `AddInstances` por batch y `ClearInstances` en recarga con una lista pequeña y fija de componentes. HISM construye un árbol espacial útil para culling cuando el lote ocupa sectores grandes; también añade coste de actualización/reconstrucción al reemplazar miles de instancias. El lote actual está acotado a 700 m y el objetivo inmediato es reducir geometría y recarga. Se selecciona ISM por sencillez y coste de actualización medible. La ejecución NullRHI mide CPU, no GPU: no se afirma que ISM gane FPS o culling frente a HISM. HISM queda para una medición posterior con varias áreas y cámara real.

El antiguo render tenía una sección. La nueva ruta tiene una sección por batch activo; esto **puede aumentar draw calls base** aunque reduzca drásticamente geometría única y el trabajo de generación. No hay contador de draw calls fiable en NullRHI, así que no se inventa una cifra medida. En un render gráfico posterior se deben comparar `stat RHI`, `stat SceneRendering` y capturas de GPU; sombras y pases pueden multiplicar las cifras.

## Recarga y ciclo de vida

Reload construye layout y batches candidatos y comprueba paridad antes de tocar componentes. Las fuentes coloreadas se cachean por actor; solo se construyen cuando aparece un color nuevo. Cada slot conserva su componente registrado; `ClearInstances` retira instancias anteriores y `AddInstances` coloca una copia por pieza. Slots inactivos quedan vacíos. Cambiar de estilo y regresar reutiliza módulos y componentes creados. Ante fallo de layout, batch o fuente, la escena previa se conserva. Los componentes y fuentes son propiedades del actor y se liberan al destruirlo con el mundo.

`Montevideo.Facades.Status`, `Reload`, `Style` y `Building` se conservan. `Montevideo.Facades.CompareLegacy` compara únicamente los datos y transforma esquinas; no activa la malla antigua. Status informa fuentes, vértices/triángulos únicos, instancias, batches activos, componentes registrados, bytes estimados y tiempos. Además enumera instancias por batch.

## Colisión y LOD futuro

Todos los ISM tienen colisión desactivada; la colisión y sus raycasts siguen en `ABuildingBlockoutActor`. Las puertas permanecen opacas y no interactuables. La estructura de slots/metadata permitiría ocultar batches por distancia o sustituir la fuente por un módulo simplificado conservando el layout; no se implementan Near/Mid/Far ni streaming en M3.3.1.

## Resultados medidos (standalone NullRHI, 15-10-2026)

| Métrica | M3.3 previo | M3.3.1 ISM |
| --- | ---: | ---: |
| Piezas lógicas / instancias | 13.362 | 13.362 |
| Vértices de posición únicos | 106.896 | 72 (9 fuentes × 8) |
| Triángulos de geometría única | 160.344 | 108 (9 fuentes × 12) |
| Payload CPU estimado de fachada | 22.448.160 B (21,41 MiB) | 1.348.728 B (1,29 MiB) |
| Tiempo inicial total | 547,048 ms | 182,242 ms |
| Recarga total del mismo proceso | 990,749 ms | 144,803 ms; segunda recarga 302,449 ms |
| Componentes / secciones activas | 1 / 1 | 13 / 13 |
| Materiales distintos | 1 | 1 |

Reducción de geometría única **99,93 %** y del payload estimado **93,99 %**. La geometría renderizada sigue repitiéndose mediante transformaciones en GPU; esas cifras se refieren a *fuentes únicas*, no a triángulos potencialmente dibujados ni a VRAM total. El payload nuevo desglosa 12.528 B de fuentes y 1.336.200 B de transforms + índices de elemento; `UObject`, capacidad, buffers GPU y layout quedan fuera. Las 9 fuentes tienen 8 posiciones y 24 esquinas de render cada una, con 36 índices; los vértices de posición se comparan con los del `UDynamicMesh` anterior. Al forzar el estilo Modern en tests se crean dos fuentes adicionales cacheadas y hasta 15 componentes registrados, pero el estilo original vuelve a **9 fuentes, 13 componentes y 13 batches activos** en un proceso standalone limpio.

Desglose inicial nuevo: layout 168,670 ms; preparación de módulos 4,340 ms; batching 4,409 ms; creación/asignación de componentes e instancias 3,419 ms; total 182,242 ms. En la primera recarga: layout 134,197 ms; módulos cacheados 0,004 ms; batching 5,328 ms; componentes/instancias 3,303 ms; total 144,803 ms. La segunda recarga dio layout 291,074 ms y total 302,449 ms. Son muestras individuales con variación apreciable del layout; no se presentan como promedios ni como prueba de FPS. Frente a la recarga previa de 990,749 ms, ambas muestras fueron menores (85,38 % y 69,47 %). La colisión de fachada es cero y la del massing permanece.

Instancias por batch activo: piel Classic 247, Residential 71 y Civic 188; planta baja de esos estilos 247, 71 y 188; bandas 1.138; paneles de ventana 1.762; piezas de marco 7.075; puertas 9; losas de balcón 465; barandas 1.395; cornisas 506. `WindowFramed` y `BalconySimple` son composiciones de piezas instanciadas, no mallas monolíticas. No se midieron draw calls de GPU: NullRHI no ofrece un dato fiable, y 13 secciones no equivalen necesariamente a 13 draw calls finales. Tampoco se hizo un benchmark controlado ISM/HISM; esa comparación de culling y GPU sigue pendiente antes de extender el área.

## Validación y deuda

La suite antigua M3.3 sigue comprobando layout, concavidad, holes, edges cortos/largos, orientación, normales de la malla de referencia y colisión. Las pruebas nuevas fijan los recuentos M3.3, comparan campos y esquinas transformadas, IDs/seed, batches por tipo, NaN/Inf y recarga repetida sin duplicados. `MontevideoGameEditor Win64 Development` compiló con UE 5.8.3 (`Result: Succeeded`). La ejecución de M2/M3/M3.2/M3.3/M3.3.1 dio **28/28 tests Unreal aprobados, 0 warnings de test**, y las pruebas Python **21/21**. Standalone terminó con código 0; `CompareLegacy` dio `PASS` y dos recargas conservaron todos los recuentos. SHA-256: **779/779 archivos** `.uasset`, `.umap` y `Data/` idénticos al inventario anterior; ningún asset/dato nuevo, ausente o alterado.

La estimación de payload nuevo incluye las fuentes de malla y `sizeof(FTransform)+ElementIndex` por instancia. Excluye capacidad reservada, `UObject`, buffers del renderer, GPU, layout lógico y colisión del massing; no representa memoria total. La prueba final visual debe revisar color lateral, orientación de caras, seams, patios y ausencia de duplicados.

Limitaciones: transformaciones agrupadas por tipo pueden elevar secciones/draw calls; ISM no aporta LOD por sí solo ni culling fino por sector. La fuente parcial “Construcciones notables” y las alturas sintéticas siguen igual. Faltan mediciones de GPU/VRAM y rendimiento con cientos de edificios. No se cambia de fuente GIS ni se incorporan assets artísticos.

Warnings de validación: el toolchain MSVC 14.51.36260 es más nuevo que el preferido 14.50.35717 por UBT; la compilación terminó sin errores. En standalone el Engine avisó que siete comandos diferidos, incluidas dos recargas, ocuparon 584,46 ms en conjunto. Hubo además avisos de widgets editoriales de Engine sin relación con fachadas. La aceptación visual de los nuevos módulos transitorios sigue abierta.

## Inspección visual

1. Reinicia Unreal 5.8.3 tras compilar y abre `Content/Montevideo/Maps/MVD_Prototype`.
2. Play → Selected Viewport. Ejecuta `Montevideo.Facades.Status` y `Montevideo.Facades.CompareLegacy`.
3. Usa `Montevideo.Facades.Building 106187939` para localizar Teatro Solís; F8, selecciona `FacadeBlockoutActor`, pulsa F y revisa desde arriba y a nivel de calle.
4. Compara plantas, paneles, puertas, marcos, balcones y remates con tu impresión de M3.3. Comprueba color lateral, orientación, esquinas cóncavas y patios.
5. Ejecuta varias veces `Montevideo.Facades.Reload` y después `Status`; ventanas/puertas/balcones, instancias, batches y componentes no deben crecer. Prueba `Style 1` y `Reload` para recuperar estilos por seed.
6. Vuelve al personaje con F8 y camina entre volúmenes; empuja contra paredes y puertas. La colisión debe ser la del massing existente. Detén Play sin guardar simulación.

La aceptación visual de M3.3.1 corresponde al usuario. El siguiente paso es resolver únicamente diferencias que observe antes de autorizar otra fase.
