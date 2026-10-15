#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/BuildingMeshGenerator.h"
#include "BuildingBlockoutActor.generated.h"

class UDynamicMeshComponent;

UCLASS()
class MONTEVIDEOGAME_API ABuildingBlockoutActor : public AActor
{
	GENERATED_BODY()
public:
	ABuildingBlockoutActor();
	bool Reload(FString& Error);
	void LogStatus() const;
	void ShowBounds() const;
	const FBuildingFootprintData& GetData() const { return Data; }
	const FBuildingMeshMetrics& GetMetrics() const { return Metrics; }
	UPROPERTY(EditAnywhere,Category="Buildings")
	FString ManifestPath=TEXT("Data/processed/montevideo/m3/buildings_manifest.json");
private:
	UPROPERTY() TObjectPtr<UDynamicMeshComponent> Buildings;
	FBuildingFootprintData Data;
	FBuildingMeshMetrics Metrics;
	double CollisionMilliseconds=0, ComponentMilliseconds=0, TotalMilliseconds=0;
};
