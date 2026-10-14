#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Geospatial/GeospatialBlockoutData.h"
#include "GeospatialBlockoutActor.generated.h"

class UDynamicMeshComponent;
class UStaticMeshComponent;

/** Transient, data-driven polygon and polyline visualization. No geographic constants or saved assets. */
UCLASS()
class MONTEVIDEOGAME_API AGeospatialBlockoutActor : public AActor
{
	GENERATED_BODY()
public:
	AGeospatialBlockoutActor();
	bool Reload(FString& Error);
	void LogStatus() const;
	void ShowBounds() const;
	const FGeospatialBlockoutData& GetData() const { return Data; }
	UPROPERTY(EditAnywhere, Category="Geospatial")
	FString ManifestPath = TEXT("Data/processed/montevideo/m3/manifest.json");
private:
	UPROPERTY() TObjectPtr<UDynamicMeshComponent> Surface;
	UPROPERTY() TObjectPtr<UDynamicMeshComponent> Markings;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DevelopmentFloor;
	FGeospatialBlockoutData Data;
};
