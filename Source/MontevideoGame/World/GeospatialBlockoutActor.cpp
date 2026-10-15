#include "World/GeospatialBlockoutActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshAttributeUtil.h"
#include "Geospatial/GeospatialSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogMontevideoBlockout, Log, All);
using UE::Geometry::FDynamicMesh3;

namespace
{
	// Linear RGB, consumed directly by the built-in unlit vertex-color material.
	const FVector3f PlazaColor(0.025f, 0.5f, 0.08f);
	const FVector3f BlockColor(0.035f, 0.065f, 0.11f);
	constexpr double OutlineZ = 2, RoadZ = 4, AxisZ = 8;

	void PrepareRenderAttributes(FDynamicMesh3& Mesh)
	{
		// UDynamicMeshComponent renders overlay colors, not FDynamicMesh3 vertex colors.
		Mesh.EnableAttributes();
		Mesh.Attributes()->SetNumUVLayers(0);
		UE::Geometry::CopyVertexNormalsToOverlay(Mesh, *Mesh.Attributes()->PrimaryNormals());
		Mesh.Attributes()->EnablePrimaryColors();
		auto* Colors = Mesh.Attributes()->PrimaryColors();
		TArray<int32> Elements;
		Elements.SetNum(Mesh.MaxVertexID());
		for (int32 Id : Mesh.VertexIndicesItr())
		{
			const FVector3f Color = Mesh.GetVertexColor(Id);
			Elements[Id] = Colors->AppendElement(FVector4f(Color.X, Color.Y, Color.Z, 1));
		}
		for (int32 Id : Mesh.TriangleIndicesItr())
		{
			const auto Triangle = Mesh.GetTriangle(Id);
			Colors->SetTriangle(Id, UE::Geometry::FIndex3i(Elements[Triangle.A], Elements[Triangle.B], Elements[Triangle.C]));
		}
	}

	int32 Vertex(FDynamicMesh3& Mesh, const FVector& Position, const FVector3f& Color)
	{
		const int32 Id = Mesh.AppendVertex(Position);
		Mesh.SetVertexNormal(Id, FVector3f(0, 0, 1));
		Mesh.SetVertexColor(Id, Color);
		return Id;
	}
	void Stroke(FDynamicMesh3& Mesh, FVector A, FVector B, double Width, double Z, const FVector3f& Color)
	{
		A.Z = B.Z = Z; // Presentation offset only; source coordinates remain unchanged.
		const FVector Side = FVector::CrossProduct(B-A, FVector::UpVector).GetSafeNormal() * Width * 0.5;
		if (Side.IsNearlyZero()) { return; }
		const int32 I = Vertex(Mesh, A+Side, Color);
		Vertex(Mesh, B+Side, Color); Vertex(Mesh, B-Side, Color); Vertex(Mesh, A-Side, Color);
		// UE rendering/collision winding is clockwise viewed from above; source contract is mathematical CCW.
		Mesh.AppendTriangle(I, I+2, I+1); Mesh.AppendTriangle(I, I+3, I+2);
	}
}

AGeospatialBlockoutActor::AGeospatialBlockoutActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Surface = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("OfficialPolygons"));
	Surface->SetupAttachment(RootComponent);
	Surface->SetCollisionProfileName(TEXT("BlockAll"));
	Surface->bUseAsyncCooking = false; // Collision must be ready before spawning the player.
	Markings = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("CartographicLines"));
	Markings->SetupAttachment(RootComponent);
	Markings->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DevelopmentFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SyntheticDevelopmentPlane"));
	DevelopmentFloor->SetupAttachment(RootComponent);
	DevelopmentFloor->SetCollisionProfileName(TEXT("BlockAll"));
}

bool AGeospatialBlockoutActor::Reload(FString& Error)
{
	if (!GetActorTransform().Equals(FTransform::Identity)) { Error = TEXT("Blockout transform must be identity: coordinates are already centimeters"); return false; }
	FGeospatialBlockoutData Candidate;
	if (!FGeospatialBlockoutLoader::Load(FPaths::ProjectDir(), ManifestPath, Candidate, Error)) { return false; }
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly"));
	UMaterialInterface* FloorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/DebugMeshMaterial.DebugMeshMaterial"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Material || !FloorMaterial || !Cube) { Error = TEXT("Required built-in debug materials/cube unavailable"); return false; }
	FDynamicMesh3 Polygons(true, true, false, false), Lines(true, true, false, false);
	for (const auto& Feature : Candidate.Features)
	{
		if (Feature.bPolygon)
		{
			const FVector3f Color = Feature.Role == TEXT("plaza") ? PlazaColor : BlockColor;
			const int32 Offset = Polygons.MaxVertexID();
			for (const FVector& Position : Feature.Vertices) { Vertex(Polygons, Position, Color); }
			for (int32 I = 0; I < Feature.Triangles.Num(); I += 3)
			{
				// UDynamicMeshComponent flips collision normals internally. Adapt winding here, not geographic axes.
				if (Polygons.AppendTriangle(Offset+Feature.Triangles[I], Offset+Feature.Triangles[I+2], Offset+Feature.Triangles[I+1]) < 0)
				{
					Error = TEXT("Invalid mesh topology: ") + Feature.Id; return false;
				}
			}
			for (const auto& Ring : Feature.Rings)
			{
				for (int32 I = 0; I < Ring.Num(); ++I)
				{
					Stroke(Lines, Feature.Vertices[Ring[I]], Feature.Vertices[Ring[(I+1)%Ring.Num()]], 30, OutlineZ, FVector3f(0.12f,0.18f,0.24f));
				}
			}
		}
		else
		{
			for (int32 I = 1; I < Feature.Vertices.Num(); ++I)
			{
				// 2 m is a debug stroke width for aerial readability, NOT a measured road width.
				Stroke(Lines, Feature.Vertices[I-1], Feature.Vertices[I], 200, RoadZ, FVector3f(1.0f,0.45f,0.015f));
			}
		}
	}
	// Development positions/axes: explicit non-cartographic 20 m reference marks.
	for (const auto& Direction : {FVector(2000,0,0), FVector(0,2000,0)})
	{
		const FVector3f Color = Direction.X > 0 ? FVector3f(0.9f,0.015f,0.01f) : FVector3f(0.05f,1,0.04f);
		Stroke(Lines, FVector::ZeroVector, Direction, 100, AxisZ, Color);
		const FVector Unit = Direction.GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Unit,FVector::UpVector);
		Stroke(Lines, Direction, Direction-Unit*300+Side*200, 100, AxisZ, Color);
		Stroke(Lines, Direction, Direction-Unit*300-Side*200, 100, AxisZ, Color);
	}
	PrepareRenderAttributes(Polygons);
	PrepareRenderAttributes(Lines);
	Surface->SetMaterial(0, Material);
	Markings->SetMaterial(0, Material);
	Surface->SetVertexColorSpaceTransformMode(EDynamicMeshVertexColorTransformMode::NoTransform);
	Markings->SetVertexColorSpaceTransformMode(EDynamicMeshVertexColorTransformMode::NoTransform);
	Surface->SetMesh(MoveTemp(Polygons));
	Surface->SetComplexAsSimpleCollisionEnabled(true, true);
	Markings->SetMesh(MoveTemp(Lines));
	// Neutral engineering support under the AOI, not inferred terrain or land cover.
	DevelopmentFloor->SetStaticMesh(Cube);
	UMaterialInstanceDynamic* FloorStyle = Cast<UMaterialInstanceDynamic>(DevelopmentFloor->GetMaterial(0));
	if (!FloorStyle || FloorStyle->Parent != FloorMaterial)
	{
		FloorStyle = UMaterialInstanceDynamic::Create(FloorMaterial, this);
	}
	FloorStyle->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.018f, 0.018f, 0.018f));
	DevelopmentFloor->SetMaterial(0, FloorStyle);
	const FVector Size = Candidate.Bounds.GetSize();
	const FVector Center = Candidate.Bounds.GetCenter();
	DevelopmentFloor->SetRelativeLocation(FVector(Center.X, Center.Y, -50.5));
	DevelopmentFloor->SetRelativeScale3D(FVector((Size.X+200)/100, (Size.Y+200)/100, 1));
	Data = MoveTemp(Candidate);
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetSubsystem<UGeospatialSubsystem>()->SetOrigin(Data.MathematicalOrigin);
	}
	LogStatus();
	return true;
}

void AGeospatialBlockoutActor::LogStatus() const
{
	const FVector Size = Data.Bounds.GetSize()/100.0;
	UE_LOG(LogMontevideoBlockout, Display, TEXT("M3: %d polygons, %d lines, %d vertices; bounds cm [%s] to [%s]; size %.3f x %.3f m; X north / Y east / Z up; Z=0 DEVELOPMENT PLANE, altitude UNKNOWN."),
		Data.PolygonCount, Data.LineCount, Data.VertexCount, *Data.Bounds.Min.ToString(), *Data.Bounds.Max.ToString(), Size.X, Size.Y);
	UE_LOG(LogMontevideoBlockout, Display, TEXT("Origin source %s; lat=%.14f lon=%.14f; h=0 is mathematical only. Manifest: %s"),
		*Data.OriginSource, Data.MathematicalOrigin.LatitudeDegrees, Data.MathematicalOrigin.LongitudeDegrees, *ManifestPath);
	for (const auto& Source : Data.Sources) { UE_LOG(LogMontevideoBlockout, Display, TEXT("Dataset: %s"), *Source); }
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 30, FColor::White,
			TEXT("M3 | Z=0 plano de desarrollo; altitud desconocida\nVerde: plaza | Azul/gris: manzanas | Amarillo: ejes viales (sin ancho real)\nFlecha roja: X norte; verde: Y este. Ambas 20 m. Montevideo.M3.Bounds"));
	}
}

void AGeospatialBlockoutActor::ShowBounds() const
{
	LogStatus();
	DrawDebugBox(GetWorld(), Data.Bounds.GetCenter()+FVector(0,0,100), Data.Bounds.GetExtent()+FVector(0,0,100), FColor::Cyan, false, 30, 0, 10);
	DrawDebugString(GetWorld(), FVector(2000,0,150), TEXT("N / +X / 20 m"), nullptr, FColor::Red, 30);
	DrawDebugString(GetWorld(), FVector(0,2000,150), TEXT("E / +Y / 20 m"), nullptr, FColor::Green, 30);
}

#if !UE_BUILD_SHIPPING
namespace
{
	void Command(UWorld* World, int32 Action)
	{
		if (!World) { return; }
		for (TActorIterator<AGeospatialBlockoutActor> It(World); It; ++It)
		{
			if (Action == 0) { It->LogStatus(); }
			else if (Action == 1) { It->ShowBounds(); }
			else { FString Error; if (!It->Reload(Error)) { UE_LOG(LogMontevideoBlockout, Error, TEXT("Reload rejected; previous scene retained: %s"), *Error); } }
			return;
		}
		UE_LOG(LogMontevideoBlockout, Display, TEXT("No blockout in this world. Play the M3 development map/mode."));
	}
	FAutoConsoleCommandWithWorld Status(TEXT("Montevideo.M3.Status"), TEXT("Print source, origin, geometry counts and dimensions."), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W){ Command(W,0); }));
	FAutoConsoleCommandWithWorld Bounds(TEXT("Montevideo.M3.Bounds"), TEXT("Show AOI and north/east reference marks for 30 seconds."), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W){ Command(W,1); }));
	FAutoConsoleCommandWithWorld Reload(TEXT("Montevideo.M3.Reload"), TEXT("Validate and rebuild transient geometry from processed files."), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W){ Command(W,2); }));
}
#endif
