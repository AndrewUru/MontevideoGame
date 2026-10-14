#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Geospatial/GeoReference.h"
#include "GeospatialSubsystem.generated.h"

/** Session-local georeference. No actors are moved when the origin changes. Game-thread API. */
UCLASS(Config=Game)
class MONTEVIDEOGAME_API UGeospatialSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Explicitly replaces the frame; previously computed local positions must be discarded. */
	UFUNCTION(BlueprintCallable, Category="Geospatial")
	EGeoConversionResult SetOrigin(const FGeoCoordinate& Origin);

	UFUNCTION(BlueprintCallable, Category="Geospatial")
	void ClearOrigin();

	UFUNCTION(BlueprintPure, Category="Geospatial")
	bool HasOrigin() const;

	UFUNCTION(BlueprintPure, Category="Geospatial")
	bool TryGetOrigin(FGeoCoordinate& OutOrigin) const;

	/** Degrees / ellipsoidal meters -> north/east/up centimeters. Output unchanged on failure. */
	UFUNCTION(BlueprintCallable, Category="Geospatial")
	EGeoConversionResult GeographicToLocal(const FGeoCoordinate& Geographic, FLocalGeoCoordinate& OutLocal) const;

	/** North/east/up centimeters -> WGS84 degrees / ellipsoidal meters. */
	UFUNCTION(BlueprintCallable, Category="Geospatial")
	EGeoConversionResult LocalToGeographic(const FLocalGeoCoordinate& Local, FGeoCoordinate& OutGeographic) const;

	/** Development inspection only; also available as Montevideo.Geo.Status in a game world. */
	void LogStatus() const;

private:
	/** Disabled until a verified origin is explicitly provided; (0,0,0) is not a Plaza preset. */
	UPROPERTY(Config)
	bool bConfigureOriginFromConfig = false;

	UPROPERTY(Config)
	FGeoCoordinate ConfigOrigin;

	FGeoReference Reference;
};
