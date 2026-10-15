#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/FacadeInstanceBatches.h"
#include "FacadeBlockoutActor.generated.h"

class ABuildingBlockoutActor;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class USceneComponent;

struct FFacadeOptimizationMetrics
{
	int32 Vertices=0,Triangles=0,Instances=0,ActiveBatches=0,Components=0;
	int64 ModulePayloadBytes=0,InstancePayloadBytes=0,LegacyPayloadBytes=0;
	double ModulePreparationMilliseconds=0,ComponentMilliseconds=0,TotalMilliseconds=0;
};

UCLASS()
class MONTEVIDEOGAME_API AFacadeBlockoutActor : public AActor
{
	GENERATED_BODY()
public:
	AFacadeBlockoutActor();
	bool Initialize(ABuildingBlockoutActor* InSource,FString& Error);
	bool Reload(FString& Error,int32 Style=-1);
	void LogStatus() const;
	void ShowBuilding(const FString& FeatureId) const;
	bool CompareLegacy(FString& Error) const;
	const FBuildingFacadeData& GetLayout() const { return Layout; }
	const FFacadeInstanceData& GetInstances() const { return InstanceData; }
	const FFacadeOptimizationMetrics& GetMetrics() const { return Metrics; }
private:
	UPROPERTY() TObjectPtr<USceneComponent> FacadeRoot;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Components;
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Modules;
	UPROPERTY() TWeakObjectPtr<ABuildingBlockoutActor> Source;
	TArray<FVector4f> ModuleColors;
	FBuildingFacadeData Layout;
	FFacadeInstanceData InstanceData;
	FFacadeOptimizationMetrics Metrics;
	int32 CurrentStyle=-1;
};
