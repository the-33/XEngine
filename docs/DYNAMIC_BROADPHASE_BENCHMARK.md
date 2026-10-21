# Calibración de broadphase con distribuciones dinámicas — XEngine v14

## Objetivo

Comprobar que `Collision.broadphase: "auto"` y `cellSize: "auto"` se adaptan a **cambios de distribución** (no solo al número de colliders), y cuantificar la pérdida frente a la mejor estrategia fija para cada fase.

Esta prueba **no establece un umbral universal** de colliders. Mide el comportamiento en tu hardware con las distribuciones ensayadas, y conserva los modos fijos como referencia.

## Cambios en el motor

El selector de celdas separa sus calibraciones por:

1. Escala del número de colliders (intervalos en potencias de dos).
2. Disponibilidad de trabajadores.
3. Ocupación global de AABB (tres clases).
4. Concentración de centros en una rejilla normalizada de 4 × 4 (tres clases).

La concentración se estima como el número efectivo de celdas ocupadas, `N² / sum(conteo_celda²)`. Las tres clases son **contextos de medición**, no umbrales que impongan directamente `grid` o `bruteforce`. Las alternativas se siguen comparando por costes medidos; continúa la histéresis de v13.

Para evitar sondeos reactivados solo por variaciones relativas en escenas muy pequeñas, la detección de regresiones exige además un incremento absoluto de tiempo superior a 0,025 ms. Ese valor es una **protección provisional contra ruido**, no un umbral universal calibrado de rendimiento; deberá revalidarse en distintas plataformas.

El profiler incorpora:

- `collision_auto_context_last`: último contexto espacial (0–8) utilizado en ese fotograma.
- `collision_auto_context_transitions`: cantidad de cambios de contexto durante el fotograma.

Conserva las demás columnas y métricas anteriores.

## Distribuciones del benchmark

`XEngine Benchmark` acepta `--layout` y `--phase-frames`:

| Layout | Comportamiento |
|---|---|
| `static` | Escena original (por defecto, sin controlador adicional) |
| `clustered` | Concentración central persistente y anclas en los extremos |
| `spread` | Dos grupos separados con las mismas anclas exteriores |
| `alternating` | Alterna distribución regular y concentrada |
| `cycling` | Rota regular → concentrada → dos grupos separados |

Los cambios se programan mediante **índice de fotograma**, nunca por reloj, y se aplican en `Update`. El mismo número de colliders se mantiene durante las fases. El esquema de paso físico determinista permite que el experimento sea reproducible. Los cambios se realizan mediante posiciones de los objetos, por lo que los perfiles muestran el coste real de los contactos que ocurren en cada fase.

## Prueba de integración reducida

Desde la raíz del proyecto:

```powershell
python tools/compare_adaptive_broadphase.py --build --layouts alternating --scenarios collision balanced --modes single collision --repetitions 1 --frames 240 --warmup 60 --phase-frames 60 --settle-frames 10
```

Ejecuciones: 2 escenarios × 2 modos × 4 estrategias = **16 procesos**. Se alterna el orden de las cuatro estrategias y se comprueban sus firmas de contacto y contadores para todos los fotogramas, incluidos los de calentamiento.

## Batería completa

```powershell
python tools/compare_adaptive_broadphase.py --build --layouts alternating cycling --scenarios collision balanced stress --modes single collision --repetitions 3 --frames 480 --warmup 120 --phase-frames 120 --settle-frames 16
```

Total: **144 procesos**. Cada proceso usa las estrategias `bruteforce`, `grid1`, `grid2` y `auto_cells`; el orden rota entre repeticiones. Las fases se evalúan individualmente.

## Resultados

La herramienta crea en `games/XEngine Benchmark/results/adaptive_pair_.../`:

- `runs.csv`, `summary.csv` y `paired_comparisons.csv`: comparativas globales, compatibles con v13.
- `equivalence.csv`: equivalencia de perfiles por fase completa, incluidos los cambios.
- `phase_summary.csv`: p50 de fotograma, media de coste de broadphase, sondeos y clases de contexto **por fase**.
- `phase_paired.csv`: coste de `auto_cells` frente a la mejor de las tres estrategias fijas para esa misma fase, escenario, modo y repetición.
- `metadata.json`, perfiles y logs individuales.

Para evitar atribuir a la broadphase los picos de recolocar todos los objetos, `--settle-frames` excluye los primeros N fotogramas de cada fase **solo de las estadísticas por fase**. La equivalencia sigue comprobando todos los fotogramas. Si un tramo termina antes del calentamiento o su período utilizable está vacío, no aparece como fase medida.

### Cómo interpretarlo

- Una firma idéntica confirma equivalencia **para los casos ensayados**, no prueba formal universal.
- Una caída del coste en `phase_paired.csv` indica una ventaja para la broadphase, sin confundirla con renderizado.
- `auto_context_transitions` y `auto_probe_passes` permiten investigar cambios de clasificación, calibraciones y sobrecoste.
- Las diferencias inferiores al 2–3 % deben repetirse y contrastarse con una batería mayor y mediciones Release antes de atribuirlas a una optimización.

## Compatibilidad

- `bruteforce`, `grid`, `auto` con `cellSize` numérico siguen disponibles.
- El benchmark antiguo permanece en `--layout static` cuando no se especifican opciones nuevas.
- No se cambian los `engine.json` del usuario ni de la plantilla: las decisiones de algoritmo y celdas continúan controladas por sus valores existentes.
