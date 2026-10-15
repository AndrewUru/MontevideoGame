#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/FacadeMeshGenerator.h"
#include "FacadeBlockoutActor.generated.h"

class ABuildingBlockoutActor;
class UDynamicMeshComponent;

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
	const FBuildingFacadeData& GetLayout() const { return Layout; }
	const FFacadeMeshMetrics& GetMetrics() const { return Metrics; }
private:
	UPROPERTY() TObjectPtr<UDynamicMeshComponent> Facades;
	UPROPERTY() TWeakObjectPtr<ABuildingBlockoutActor> Source;
	FBuildingFacadeData Layout;
	FFacadeMeshMetrics Metrics;
	double TotalMilliseconds=0,ComponentMilliseconds=0;
};
