#include "World/FacadeBlockoutActor.h"
#include "World/BuildingBlockoutActor.h"
#include "World/FacadeModuleLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogMontevideoFacades,Log,All);

AFacadeBlockoutActor::AFacadeBlockoutActor()
{
	PrimaryActorTick.bCanEverTick=false;
	FacadeRoot=CreateDefaultSubobject<USceneComponent>(TEXT("FacadeRoot"));RootComponent=FacadeRoot;
}

bool AFacadeBlockoutActor::Initialize(ABuildingBlockoutActor* InSource,FString& Error)
{
	Source=InSource;return Reload(Error);
}

bool AFacadeBlockoutActor::Reload(FString& Error,int32 Style)
{
	const double Start=FPlatformTime::Seconds();
	if(!Source.IsValid() || !GetActorTransform().Equals(FTransform::Identity) || !Source->GetActorTransform().Equals(FTransform::Identity))
	{ Error=TEXT("Facades require a live massing source and identity transforms");return false; }
	FBuildingFacadeData Candidate;
	FFacadeInstanceData CandidateInstances;
	if(!FFacadeLayoutGenerator::Build(Source->GetData(),Candidate,Error,Style)
		|| !FFacadeInstanceBatches::Build(Candidate,CandidateInstances,Error)
		|| !FFacadeInstanceBatches::Compare(Candidate,Candidate,CandidateInstances,Error)) { return false; }
	UMaterialInterface* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly"));
	if(!Material) { Error=TEXT("Existing Engine vertex color material unavailable");return false; }
	const double ModuleStart=FPlatformTime::Seconds();
	TArray<UStaticMesh*> BatchMeshes;BatchMeshes.SetNum(FFacadeInstanceBatches::SlotCount);
	for(int32 Slot=0;Slot<FFacadeInstanceBatches::SlotCount;++Slot)
	{
		const auto& Batch=CandidateInstances.Batches[Slot];if(Batch.Transforms.IsEmpty()) { continue; }
		int32 Found=INDEX_NONE;
		for(int32 I=0;I<ModuleColors.Num();++I) { if(ModuleColors[I]==Batch.Color) { Found=I;break; } }
		if(Found==INDEX_NONE)
		{
			UStaticMesh* NewModule=FFacadeModuleLibrary::CreateUnitPrism(this,Material,Batch.Color,Error);
			if(!NewModule) { return false; }
			Found=Modules.Add(NewModule);ModuleColors.Add(Batch.Color);
		}
		BatchMeshes[Slot]=Modules[Found];
	}
	const double ModuleMilliseconds=(FPlatformTime::Seconds()-ModuleStart)*1000;
	const double ComponentStart=FPlatformTime::Seconds();
	if(Components.Num()==0) { Components.SetNum(FFacadeInstanceBatches::SlotCount); }
	int32 ComponentCount=0,ActiveCount=0;
	for(int32 Slot=0;Slot<FFacadeInstanceBatches::SlotCount;++Slot)
	{
		const auto& Batch=CandidateInstances.Batches[Slot];
		UInstancedStaticMeshComponent* Component=Components[Slot];
		if(!Component && !Batch.Transforms.IsEmpty())
		{
			Component=NewObject<UInstancedStaticMeshComponent>(this,*FString::Printf(TEXT("FacadeBatch_%02d"),Slot),RF_Transient);
			Component->SetupAttachment(FacadeRoot);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetCanEverAffectNavigation(false);
			Component->RegisterComponent();
			Components[Slot]=Component;
		}
		if(!Component) { continue; }
		++ComponentCount;
		Component->ClearInstances();
		if(!Batch.Transforms.IsEmpty())
		{
			Component->SetStaticMesh(BatchMeshes[Slot]);
			Component->SetMaterial(0,Material);
			Component->AddInstances(Batch.Transforms,false,false,false);
			++ActiveCount;
		}
	}
	const double ComponentMilliseconds=(FPlatformTime::Seconds()-ComponentStart)*1000;
	Layout=MoveTemp(Candidate);InstanceData=MoveTemp(CandidateInstances);CurrentStyle=Style;
	FFacadeOptimizationMetrics NewMetrics;
	NewMetrics.Vertices=Modules.Num()*8;NewMetrics.Triangles=Modules.Num()*12;
	NewMetrics.Instances=InstanceData.Instances;NewMetrics.ActiveBatches=ActiveCount;NewMetrics.Components=ComponentCount;
	// Payload estimate: unit mesh positions, 24 render corners, 36 indices; transform and metadata index per instance.
	NewMetrics.ModulePayloadBytes=static_cast<int64>(Modules.Num())*(8*sizeof(FVector3f)+24*(3*sizeof(FVector3f)+sizeof(FVector2f)+sizeof(FColor))+36*sizeof(uint32));
	NewMetrics.InstancePayloadBytes=static_cast<int64>(InstanceData.Instances)*(sizeof(FTransform)+sizeof(int32));
	NewMetrics.LegacyPayloadBytes=static_cast<int64>(Layout.Elements.Num())*1680;
	NewMetrics.ModulePreparationMilliseconds=ModuleMilliseconds;
	NewMetrics.ComponentMilliseconds=ComponentMilliseconds;
	NewMetrics.TotalMilliseconds=(FPlatformTime::Seconds()-Start)*1000;
	Metrics=NewMetrics;LogStatus();return true;
}

bool AFacadeBlockoutActor::CompareLegacy(FString& Error) const
{
	if(!Source.IsValid()) { Error=TEXT("Missing building mass");return false; }
	FBuildingFacadeData Regenerated;
	return FFacadeLayoutGenerator::Build(Source->GetData(),Regenerated,Error,CurrentStyle)
		&& FFacadeInstanceBatches::Compare(Regenerated,Layout,InstanceData,Error);
}

void AFacadeBlockoutActor::LogStatus() const
{
	const double Reduction=Metrics.LegacyPayloadBytes>0?100.0*(1.0-static_cast<double>(Metrics.ModulePayloadBytes+Metrics.InstancePayloadBytes)/Metrics.LegacyPayloadBytes):0;
	UE_LOG(LogMontevideoFacades,Display,TEXT("Facades: buildings=%d facades=%d bays=%d windows=%d doors=%d balconies=%d styles Classic/Modern/Residential/Civic=%d/%d/%d/%d; instances=%d batches=%d components=%d sources=%d unique vertices=%d triangles=%d (legacy %d/%d); payload modules=%lld transforms+IDs=%lld total=%lld legacy=%lld reduction=%.2f%% bytes; layout=%.3f ms modules=%.3f ms batching=%.3f ms components=%.3f ms total=%.3f ms. Material=Engine VertexColorViewMode_ColorOnly; collision=mass only."),
		Layout.Buildings,Layout.Segments.Num(),Layout.Bays,Layout.Windows,Layout.Doors,Layout.Balconies,
		Layout.StyleBuildings[0],Layout.StyleBuildings[1],Layout.StyleBuildings[2],Layout.StyleBuildings[3],
		Metrics.Instances,Metrics.ActiveBatches,Metrics.Components,Modules.Num(),Metrics.Vertices,Metrics.Triangles,
		Layout.Elements.Num()*8,Layout.Elements.Num()*12,Metrics.ModulePayloadBytes,Metrics.InstancePayloadBytes,
		Metrics.ModulePayloadBytes+Metrics.InstancePayloadBytes,Metrics.LegacyPayloadBytes,Reduction,
		Layout.LayoutMilliseconds,Metrics.ModulePreparationMilliseconds,InstanceData.BatchingMilliseconds,Metrics.ComponentMilliseconds,Metrics.TotalMilliseconds);
	for(int32 Slot=0;Slot<InstanceData.Batches.Num();++Slot)
	{
		const auto& B=InstanceData.Batches[Slot];if(!B.Transforms.IsEmpty())
		{ UE_LOG(LogMontevideoFacades,Display,TEXT("Facade batch %02d %s: %d instances, one section, one Engine debug material"),Slot,*B.Name,B.Transforms.Num()); }
	}
}

void AFacadeBlockoutActor::ShowBuilding(const FString& FeatureId) const
{
	FBox Bounds(ForceInit);int32 Count=0;
	for(const auto& S:Layout.Segments) if(S.SourceFeatureId==FeatureId || S.BuildingId==FeatureId)
	{
		Bounds+=S.Origin;Bounds+=S.Origin+S.Tangent*S.Length+FVector(0,0,S.Height);++Count;
		DrawDebugLine(GetWorld(),S.Origin+FVector(0,0,S.Height+30),S.Origin+S.Tangent*S.Length+FVector(0,0,S.Height+30),FColor::Cyan,false,30,0,6);
	}
	if(Bounds.IsValid) { DrawDebugBox(GetWorld(),Bounds.GetCenter(),Bounds.GetExtent(),FColor::Cyan,false,30,0,8);DrawDebugString(GetWorld(),Bounds.GetCenter()+FVector(0,0,Bounds.GetExtent().Z+100),FeatureId,nullptr,FColor::White,30); }
	UE_LOG(LogMontevideoFacades,Display,TEXT("Building %s: %d exterior segments, bounds %s"),*FeatureId,Count,*Bounds.ToString());
}

#if !UE_BUILD_SHIPPING
namespace FacadeCommands
{
	void Run(const TArray<FString>& Args,UWorld* World,int32 Action)
	{
		if(!World) { return; }
		for(TActorIterator<AFacadeBlockoutActor> It(World);It;++It)
		{
			if(Action==0) { It->LogStatus(); }
			else if(Action==3) { if(Args.Num()==1) { It->ShowBuilding(Args[0]); } }
			else if(Action==4)
			{
				FString Error;const bool Pass=It->CompareLegacy(Error);
				UE_LOG(LogMontevideoFacades,Display,TEXT("CompareLegacy logical layout, identity, colors and instance corners: %s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),*Error);
			}
			else
			{
				int32 Style=-1;
				if(Action==2 && (Args.Num()!=1 || !LexTryParseString(Style,*Args[0]) || Style < -1 || Style>3)) { UE_LOG(LogMontevideoFacades,Warning,TEXT("Style requires -1 (seed) or 0 Classic, 1 Modern, 2 Residential, 3 Civic"));return; }
				FString Error;if(!It->Reload(Error,Style)) { UE_LOG(LogMontevideoFacades,Error,TEXT("Reload rejected, old facade retained: %s"),*Error); }
			}
			return;
		}
	}
	FAutoConsoleCommandWithWorldAndArgs Status(TEXT("Montevideo.Facades.Status"),TEXT("Facade counts/styles/timings."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W){Run(A,W,0);}));
	FAutoConsoleCommandWithWorldAndArgs Reload(TEXT("Montevideo.Facades.Reload"),TEXT("Rebuild from current massing, reset seed styles."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W){Run(A,W,1);}));
	FAutoConsoleCommandWithWorldAndArgs Style(TEXT("Montevideo.Facades.Style"),TEXT("Temporary style -1(seed), 0..3. No source mutation."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W){Run(A,W,2);}));
	FAutoConsoleCommandWithWorldAndArgs Building(TEXT("Montevideo.Facades.Building"),TEXT("Highlight source_feature_id for 30 seconds."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W){Run(A,W,3);}));
	FAutoConsoleCommandWithWorldAndArgs Compare(TEXT("Montevideo.Facades.CompareLegacy"),TEXT("Compare source layout and instance transforms without rendering twice."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W){Run(A,W,4);}));
}
#endif
