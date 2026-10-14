#pragma once

#include "CoreMinimal.h"
#include "GeoCoordinate.generated.h"

/** WGS84 geodetic latitude/longitude in degrees, ellipsoidal height in meters. */
USTRUCT(BlueprintType)
struct MONTEVIDEOGAME_API FGeoCoordinate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geospatial")
	double LatitudeDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geospatial")
	double LongitudeDegrees = 0.0;

	/** Omitted height means zero on the WGS84 ellipsoid, NOT sea level or terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geospatial")
	double AltitudeMeters = 0.0;

	FGeoCoordinate() = default;
	FGeoCoordinate(double Latitude, double Longitude, double Altitude = 0.0)
		: LatitudeDegrees(Latitude), LongitudeDegrees(Longitude), AltitudeMeters(Altitude) {}
};

/** Position relative to a configured origin: X north, Y east, Z up; centimeters. */
USTRUCT(BlueprintType)
struct MONTEVIDEOGAME_API FLocalGeoCoordinate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geospatial")
	FVector Centimeters = FVector::ZeroVector;

	FLocalGeoCoordinate() = default;
	explicit FLocalGeoCoordinate(const FVector& Value) : Centimeters(Value) {}
};

/** Failed operations leave the output and any previously valid origin unchanged. */
UENUM(BlueprintType)
enum class EGeoConversionResult : uint8
{
	Success,
	OriginNotConfigured,
	InvalidCoordinate,
	OutsideLocalDomain,
	NumericalFailure
};
