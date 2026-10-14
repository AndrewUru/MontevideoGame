#pragma once

#include "Geospatial/GeoCoordinate.h"

/** Value-type transform; no actors, world, subsystem, content or external services. */
class MONTEVIDEOGAME_API FGeoReference
{
public:
	static constexpr double MaxLocalRadiusMeters = 20000.0;
	static constexpr double MinAltitudeMeters = -1000.0;
	static constexpr double MaxAltitudeMeters = 10000.0;

	EGeoConversionResult SetOrigin(const FGeoCoordinate& InOrigin);
	void ClearOrigin();
	bool HasOrigin() const { return bHasOrigin; }
	bool TryGetOrigin(FGeoCoordinate& OutOrigin) const;
	EGeoConversionResult GeographicToLocal(const FGeoCoordinate& Geographic, FLocalGeoCoordinate& OutLocal) const;
	EGeoConversionResult LocalToGeographic(const FLocalGeoCoordinate& Local, FGeoCoordinate& OutGeographic) const;
	static bool IsValidCoordinate(const FGeoCoordinate& Coordinate);

private:
	bool bHasOrigin = false;
	FGeoCoordinate Origin;
	FVector3d OriginEcefMeters = FVector3d::ZeroVector;
	FVector3d North = FVector3d::ZeroVector;
	FVector3d East = FVector3d::ZeroVector;
	FVector3d Up = FVector3d::ZeroVector;
};
