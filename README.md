# MontevideoGame

<p align="center">
  <img
    src="https://elsaltoweb.es/wp-content/uploads/2026/10/Montevideo_-Atardecer-en-mundo-abierto.webp"
    alt="MontevideoGame — Open World Prototype"
    width="560"
  />
</p>

<p align="center">
  <strong>Open-world Unreal Engine prototype based on real geospatial data from Montevideo, Uruguay.</strong>
</p>

---

## Overview

**MontevideoGame** is an experimental open-world game built with **Unreal Engine 5.8.3 and C++**.

The project explores how a real city can be transformed into a playable Unreal Engine world using official geospatial datasets instead of manually recreating the map.

The current prototype focuses on **Plaza Independencia and its surrounding urban grid**.

The long-term architecture is intended to support:

- Real street geometry
- Procedural urban generation
- Driveable vehicles
- Traffic
- Pedestrians
- Police / wanted systems
- Missions
- Large-world streaming
- Progressive expansion across Montevideo

The project is intentionally developed in small, validated milestones instead of attempting to generate the full city at once.

---

# Current status

Current milestone:

# M3.1 / M3.2 — Prototype world and building generator proof of concept

Completed:
M0    Repository architecture              ✅
M1    Unreal C++ gameplay foundation       ✅
M2    Geospatial coordinate system         ✅
M2.5  Official data provenance pipeline    ✅
M3    Runtime urban blockout                ✅ User visually validated
M3.1  Dedicated MVD_Prototype world          Implemented
M3.2  Building generator proof of concept    Technical validation; user visual acceptance pending

Not started:
M4    Driveable vehicle
M5    Extended road network
M6    Procedural buildings
M7    Traffic
M8    Pedestrians
M9    Police / Wanted system
M10   Missions
M11   Montevideo expansion

Current M3 dataset
The current runtime blockout covers approximately:
Metric	Value
Processing window	700 × 700 m
Covered area	0.49 km²
Polygons	67
Street lines	160
Geographic vertices	4,121
Generated surface triangles	2,579
Local bounds	~705.5 × 705.8 m
Plaza Independencia	~15,842 m²
Terrain elevation	Z = 0 development plane


Current geometry contains:
- Plaza Independencia
- Official urban blocks
- Official street axes
- Debug geographic orientation
- Collision on generated surfaces
- Third-person traversal
M3.2 adds 9 notable buildings / 12 footprints as a procedural proof of concept, with explicitly synthetic heights. This is not complete building coverage. No terrain elevation, traffic or vehicles are generated.
Technology stack
Runtime
- Unreal Engine 5.8.3
- C++
- Enhanced Input
- GeometryCore
- GeometryFramework
- Unreal JSON
Geospatial processing
- Python
- pyproj
- Shapely
- pyshp
- NumPy
Pinned geospatial dependencies are defined in:
requirements-geospatial.txt

Development
- Visual Studio C++ toolchain
- VS Code
- Codex
- Git
- Git LFS
Architecture
The project separates:
geographic data
world generation
gameplay
rendering / debug visualization

as independent concerns.

# High-level pipeline:
Official Montevideo GIS
        │
        ▼
    Data/raw
        │
        ▼
Python processing pipeline
        │
        ▼
 Data/processed
        │
        ▼
  FGeoReference
        │
        ▼
UGeospatialSubsystem
        │
        ▼
Unreal local coordinates
        │
        ▼
FGeospatialBlockoutLoader
        │
        ▼
AGeospatialBlockoutActor
        │
        ▼
Runtime procedural blockout

Gameplay systems are not allowed to depend directly on GIS providers or external datasets.
Geospatial coordinate system
The geographic layer implemented in M2 uses a local tangent coordinate system.
# Conversion pipeline:
WGS84
  │
  ▼
ECEF
Earth-Centered, Earth-Fixed
  │
  ▼
Local tangent frame
  │
  ▼
Unreal Engine coordinates

All calculations are performed in double precision before conversion to Unreal coordinates.
Unreal axis convention
+X = North
+Y = East
+Z = Up

Unreal units:
1 meter = 100 Unreal units
1 Unreal unit = 1 centimeter

Geographic origin
The development origin is based on the official geometry of:
Plaza Independencia, Montevideo
Processed centroid:
Latitude:  -34.90650688716759
Longitude: -56.19975332071957

The value is not manually selected.
It is derived reproducibly from the official public-space polygon processed during M2.5.
The geographic origin maps approximately to:
FVector(0.0, 0.0, 0.0)

inside the Unreal local frame.
Altitude is currently intentionally undefined.
altitude = null
vertical datum = null

Until a reliable vertical reference is integrated, M3 uses:
Z = 0

for all cartographic geometry.
Core geospatial API
The main runtime subsystem is:
UGeospatialSubsystem : public UGameInstanceSubsystem

Public API:
SetOrigin(const FGeoCoordinate&)
ClearOrigin()
HasOrigin()
TryGetOrigin(FGeoCoordinate&)

GeographicToLocal(
    const FGeoCoordinate&,
    FLocalGeoCoordinate&
)

LocalToGeographic(
    const FLocalGeoCoordinate&,
    FGeoCoordinate&
)

LogStatus()

Conversions report explicit results through:
EGeoConversionResult

instead of silently failing.
Geospatial reference implementation
FGeoReference contains the conversion mathematics independently from gameplay.
Responsibilities:
WGS84 validation
        ↓
ECEF conversion
        ↓
local tangent transformation
        ↓
meter → centimeter conversion
        ↓
inverse transformation

Current operational constraints:
maximum local radius: 20 km

supported ellipsoid height:
-1000 m → 10000 m

The current city prototype is therefore intentionally treated as a local urban coordinate frame rather than a planet-scale system.
M3 runtime blockout
M3 introduces the first visualization of real Montevideo data.
Main classes:
FGeospatialBlockoutLoader
AGeospatialBlockoutActor
AMontevideoBlockoutGameMode

FGeospatialBlockoutLoader
Responsible for:
- Reading processed JSON
- Validating geometry
- Loading metadata
- Converting geographic coordinates
- Producing local Unreal geometry
It does not render the world itself.
AGeospatialBlockoutActor
Responsible for runtime representation of:
polygons
polylines
local points

The actor currently creates temporary blockout geometry for:
- Public spaces
- Urban blocks
- Streets
No coordinates are embedded directly into C++.
Geometry is loaded from processed datasets.
AMontevideoBlockoutGameMode
Responsible for:
- Initializing the M3 runtime scene
- Configuring the geographic origin
- Creating the blockout actor
- Preserving the existing Third Person gameplay stack
- Positioning the development player
It reuses the existing:
BP_ThirdPersonCharacter
BP_ThirdPersonPlayerController

instead of duplicating their configuration.
Gameplay architecture
The base gameplay hierarchy is currently:
ACharacter
    ↓
AMontevideoGameCharacter
    ↓
BP_ThirdPersonCharacter

APlayerController
    ↓
AMontevideoGamePlayerController
    ↓
BP_ThirdPersonPlayerController

AGameModeBase
    ↓
AMontevideoGameGameMode
    ↓
BP_ThirdPersonGameMode

The architecture follows this rule:
C++       → gameplay behaviour / architecture
Blueprint → Unreal asset configuration / presentation

Critical gameplay systems should not exist exclusively inside Blueprints.
Official geographic data
Current datasets come from the:
Servicio de Geomática — Intendencia de Montevideo
M3 currently uses official layers equivalent to:
Espacios Públicos
Vías
Manzanas materializadas

The original coordinate reference system is:
EPSG:32721
WGS 84 / UTM zone 21S

Processed output is converted explicitly to WGS84 when required.
Data provenance
Original geographic data is treated as immutable.
Data/raw/

contains downloaded originals and provenance metadata.
Derived files are stored under:
Data/processed/

Each dataset records information such as:
dataset name
provider
catalog URL
download URL
download timestamp
original CRS
format
license
SHA-256
processing notes

Example original M2.5 ZIP SHA-256:
35fa31f9cc6a683f65dc61358db164dfaee69feb6aca02140784b4fa59dce403

See:
[M2_5_DATA_PROVENANCE.md](docs/M2_5_DATA_PROVENANCE.md)
for the complete provenance pipeline.
Geographic processing pipeline
Current processing model:
WFS / official dataset
        │
        ▼
download
        │
        ▼
Data/raw
        │
        ▼
integrity validation
        │
        ▼
CRS inspection
        │
        ▼
feature filtering
        │
        ▼
geometry processing
        │
        ▼
WGS84 normalization
        │
        ▼
Data/processed
        │
        ▼
Unreal runtime loader

Python scripts are located under:
Scripts/geospatial/

They are designed to be deterministic.
Running the same pipeline against the same raw dataset should produce identical processed output.
Automated validation
M2 geospatial tests
Current M2 suite:
9 / 9 passing

Coverage includes:
- Origin → zero
- Geographic round trip
- North displacement
- East displacement
- Altitude
- Urban numerical stability
- Invalid coordinates
- Poles and antimeridian
- GameInstance lifecycle isolation
Measured maximum urban positional error:
3.1181 × 10^-7 cm

M2.5 provenance validation
Python validation:
6 / 6 passing

Includes checks for:
- Deterministic output
- Original data immutability
- Correct feature selection
- Valid CRS
- Centroid validity
- Geographic bounds
M3 validation
Current validation:
M2 tests          9 / 9
M3 tests          6 / 6
Python tests      5 / 5

Additional checks include:
- Runtime geometry loading
- Plaza Independencia polygon
- Expected area
- Local bounds
- Coordinate radius
- North/east orientation
- Raw-data immutability
- Generated surface collision
Binary asset protection
The GIS pipeline is intentionally designed to avoid unnecessary modification of Unreal binary assets.
After M3 validation:
753 .uasset / .umap files

were verified through SHA-256.
Result:
modified: 0
added:    0
removed:  0

The current M3 blockout is generated at runtime from C++ and processed data.
Debug commands
Development console commands currently include:
Montevideo.Geo.Status
Montevideo.M3.Status
Montevideo.M3.Bounds

They expose information such as:
- Active geographic origin
- Loaded geometry
- Local bounds
- Source datasets
- Geographic status
Running the current prototype
Open:
MontevideoGame.uproject

with:
Unreal Engine 5.8.3

The dedicated prototype world is:
Content/Montevideo/Maps/MVD_Prototype.umap

Run:
Play → Selected Viewport

Controls:
WASD     Movement
Mouse    Camera
Space    Jump
F8       Detach PIE camera
Esc      Stop / release input depending on PIE state

M3 currently generates the geographic scene only while the game is running.
Building
Current validated target:
MontevideoGameEditor
Win64
Development

Validated with Unreal Engine:
5.8.3

Latest M3 build status:
Result: Succeeded
Exit code: 0

Generated folders are not versioned:
Binaries/
DerivedDataCache/
Intermediate/
Saved/
.vs/

Source folders are versioned:
Source/
Content/
Config/
Scripts/
Data/
docs/

Repository structure
MontevideoGame/
│
├── Config/
│
├── Content/
│
├── Data/
│   ├── raw/
│   │   └── montevideo/
│   │
│   └── processed/
│       └── montevideo/
│
├── Scripts/
│   └── geospatial/
│
├── Source/
│   └── MontevideoGame/
│       ├── Geospatial/
│       ├── Tests/
│       └── ...
│
├── Tools/
│
├── docs/
│   ├── ARCHITECTURE.md
│   ├── GAMEPLAY_SYSTEMS.md
│   ├── MAP_PIPELINE.md
│   ├── ROADMAP.md
│   ├── M1_BASELINE.md
│   ├── M2_GEOSPATIAL.md
│   ├── M2_5_DATA_PROVENANCE.md
│   └── M3_MONTEVIDEO_BLOCKOUT.md
│
├── AGENTS.md
├── README.md
├── requirements-geospatial.txt
└── MontevideoGame.uproject

Engineering principles
The project follows several explicit constraints.
Real data before approximation
Whenever possible, urban geometry should originate from verifiable geographic sources rather than manual approximation.
Reproducibility
Every transformation from official data to game-ready data should be repeatable.
Provenance
The origin and license of every geographic dataset should remain traceable.
Separation of concerns
GIS processing ≠ world renderer ≠ gameplay

These systems should remain replaceable independently.
No full-city loading
Montevideo will eventually require spatial partitioning and streaming.
The architecture must never assume that the entire city can remain loaded simultaneously.
C++ as gameplay source of truth
Blueprints may configure and extend gameplay classes, but core systems should remain inspectable and testable in C++.
Known limitations
Current M3 limitations:
- No real terrain elevation
- No vertical datum
- Development geometry uses Z = 0
- No detailed buildings
- No façades
- No vegetation
- No driveable vehicles
- No traffic
- No pedestrians
- No police
- No missions
- Minimal prototype map; no production world streaming
- M3 debug rendering visually validated; M3.2 buildings await user acceptance
- City-scale streaming has not yet been introduced
Next validation step
Inspect MVD_Prototype in Play and accept or reject the M3.2 building-generator proof of concept.
"Construcciones notables" is a limited official dataset, not complete urban building coverage.
M3.3 and M4 are not authorized.

Documentation
Detailed technical documentation:
- [Architecture](docs/ARCHITECTURE.md)
- [Roadmap](docs/ROADMAP.md)
- [Map pipeline](docs/MAP_PIPELINE.md)
- [Gameplay systems](docs/GAMEPLAY_SYSTEMS.md)
- [M1 baseline](docs/M1_BASELINE.md)
- [M2 geospatial system](docs/M2_GEOSPATIAL.md)
- [M2.5 data provenance](docs/M2_5_DATA_PROVENANCE.md)
- [M3 Montevideo blockout](docs/M3_MONTEVIDEO_BLOCKOUT.md)
- [M3.1 project world](docs/M3_1_PROJECT_WORLD.md)
- [M3.2 building generator proof of concept](docs/M3_2_BUILDING_GENERATOR.md)
Disclaimer
MontevideoGame is an independent game-development and engineering prototype.
It is not affiliated with, endorsed by, or connected to Rockstar Games or the Grand Theft Auto franchise.
Third-party geographic datasets retain their respective licenses and attribution requirements.
```