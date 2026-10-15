#include "World/MontevideoBlockoutGameMode.h"
#include "World/GeospatialBlockoutActor.h"
#include "World/BuildingBlockoutActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"

void AMontevideoBlockoutGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	AGeospatialBlockoutActor* Blockout = GetWorld()->SpawnActor<AGeospatialBlockoutActor>();
	if (!Blockout || !Blockout->Reload(ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("M3 initialization failed: %s"), *ErrorMessage);
		return;
	}
	ABuildingBlockoutActor* Buildings = GetWorld()->SpawnActor<ABuildingBlockoutActor>();
	if (!Buildings || !Buildings->Reload(ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("M3.2 building initialization failed: %s"), *ErrorMessage);
		return;
	}
	// The verified origin lies inside the plaza. Capsule center starts 120 cm above its development surface.
	DevelopmentStart = GetWorld()->SpawnActor<APlayerStart>(FVector(0,0,120), FRotator(0,90,0));
	ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,10000), FRotator(-55,-30,0));
	Sun->SetMobility(EComponentMobility::Movable);
	UDirectionalLightComponent* SunComponent = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
	SunComponent->SetIntensity(8.0f);
	SunComponent->bAtmosphereSunLight = true;
	GetWorld()->SpawnActor<ASkyAtmosphere>();
	ASkyLight* Sky = GetWorld()->SpawnActor<ASkyLight>();
	Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Sky->GetLightComponent()->SetRealTimeCaptureEnabled(true);
	Sky->GetLightComponent()->SetIntensity(1.0f);
	APostProcessVolume* Exposure = GetWorld()->SpawnActor<APostProcessVolume>();
	Exposure->bUnbound = true;
	Exposure->Settings.bOverride_AutoExposureMinBrightness = true;
	Exposure->Settings.bOverride_AutoExposureMaxBrightness = true;
	Exposure->Settings.AutoExposureMinBrightness = 0;
	Exposure->Settings.AutoExposureMaxBrightness = 0;
	// Local M3 debug presentation only. Keep colors stable without changing project CVars.
	Exposure->Settings.bOverride_AutoExposureBias = true;
	Exposure->Settings.AutoExposureBias = 0;
	Exposure->Settings.bOverride_BloomIntensity = true;
	Exposure->Settings.BloomIntensity = 0;
}

AActor* AMontevideoBlockoutGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	return DevelopmentStart ? DevelopmentStart.Get() : Super::ChoosePlayerStart_Implementation(Player);
}
