#include "Geospatial/GeospatialSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGeospatial, Log, All);

void UGeospatialSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Reference.ClearOrigin();
	if (bConfigureOriginFromConfig && SetOrigin(ConfigOrigin) != EGeoConversionResult::Success)
	{
		UE_LOG(LogGeospatial, Warning, TEXT("Invalid configured origin; georeference remains unconfigured."));
	}
	LogStatus();
}

void UGeospatialSubsystem::Deinitialize()
{
	Reference.ClearOrigin();
	Super::Deinitialize();
}

EGeoConversionResult UGeospatialSubsystem::SetOrigin(const FGeoCoordinate& Origin)
{
	return Reference.SetOrigin(Origin);
}

void UGeospatialSubsystem::ClearOrigin() { Reference.ClearOrigin(); }
bool UGeospatialSubsystem::HasOrigin() const { return Reference.HasOrigin(); }
bool UGeospatialSubsystem::TryGetOrigin(FGeoCoordinate& OutOrigin) const { return Reference.TryGetOrigin(OutOrigin); }

EGeoConversionResult UGeospatialSubsystem::GeographicToLocal(const FGeoCoordinate& Geographic, FLocalGeoCoordinate& OutLocal) const
{
	return Reference.GeographicToLocal(Geographic, OutLocal);
}

EGeoConversionResult UGeospatialSubsystem::LocalToGeographic(const FLocalGeoCoordinate& Local, FGeoCoordinate& OutGeographic) const
{
	return Reference.LocalToGeographic(Local, OutGeographic);
}

void UGeospatialSubsystem::LogStatus() const
{
#if !UE_BUILD_SHIPPING
	FGeoCoordinate Origin;
	if (Reference.TryGetOrigin(Origin))
	{
		UE_LOG(LogGeospatial, Display, TEXT("WGS84 origin: lat=%.10f deg lon=%.10f deg h=%.4f m (ellipsoid); X=North Y=East Z=Up; cm; radius <= %.0f m."),
			Origin.LatitudeDegrees, Origin.LongitudeDegrees, Origin.AltitudeMeters, FGeoReference::MaxLocalRadiusMeters);
	}
	else
	{
		UE_LOG(LogGeospatial, Display, TEXT("Origin unconfigured. Plaza Independencia coordinates await a verified source."));
	}
#endif
}

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorldAndArgs GeoStatusCommand(
		TEXT("Montevideo.Geo.Status"), TEXT("Print this GameInstance's georeference; does not change origin or assets."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			UGeospatialSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UGeospatialSubsystem>() : nullptr;
			if (Subsystem) { Subsystem->LogStatus(); }
			else { UE_LOG(LogGeospatial, Display, TEXT("No GameInstance georeference. Run in PIE or a game world.")); }
		}));
}
#endif
