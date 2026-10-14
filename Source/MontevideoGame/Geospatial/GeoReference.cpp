#include "Geospatial/GeoReference.h"

#include <cmath>

namespace
{
	// Defining WGS84 ellipsoid parameters. All internal distances are meters.
	constexpr double SemiMajor = 6378137.0;
	constexpr double Flattening = 1.0 / 298.257223563;
	constexpr double EccentricitySquared = Flattening * (2.0 - Flattening);
	constexpr double SemiMinor = SemiMajor * (1.0 - Flattening);
	constexpr double RadiansPerDegree = 0.01745329251994329576923690768489;
	constexpr double CentimetersPerMeter = 100.0;
	// Allows only floating-point noise at the explicit domain boundary (0.1 micrometer).
	constexpr double BoundaryToleranceMeters = 1.e-7;

	bool IsFinite(const FVector3d& Value)
	{
		return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
	}

	FVector3d ToEcef(const FGeoCoordinate& Coordinate)
	{
		const double Latitude = Coordinate.LatitudeDegrees * RadiansPerDegree;
		const double Longitude = Coordinate.LongitudeDegrees * RadiansPerDegree;
		const double SinLatitude = std::sin(Latitude);
		const double CosLatitude = std::cos(Latitude);
		const double N = SemiMajor / std::sqrt(1.0 - EccentricitySquared * SinLatitude * SinLatitude);
		return FVector3d(
			(N + Coordinate.AltitudeMeters) * CosLatitude * std::cos(Longitude),
			(N + Coordinate.AltitudeMeters) * CosLatitude * std::sin(Longitude),
			(N * (1.0 - EccentricitySquared) + Coordinate.AltitudeMeters) * SinLatitude);
	}

	bool FromEcef(const FVector3d& Ecef, double PoleLongitude, FGeoCoordinate& Result)
	{
		const double P = std::hypot(Ecef.X, Ecef.Y);
		if (P < 1.e-8)
		{
			// Longitude is undefined on the pole; preserve the origin's chosen meridian.
			Result = FGeoCoordinate(Ecef.Z >= 0.0 ? 90.0 : -90.0, PoleLongitude, std::abs(Ecef.Z) - SemiMinor);
			return true;
		}

		double Latitude = std::atan2(Ecef.Z, P * (1.0 - EccentricitySquared));
		bool bConverged = false;
		for (int32 Iteration = 0; Iteration < 16; ++Iteration)
		{
			const double SinLatitude = std::sin(Latitude);
			const double N = SemiMajor / std::sqrt(1.0 - EccentricitySquared * SinLatitude * SinLatitude);
			const double NextLatitude = std::atan2(Ecef.Z + EccentricitySquared * N * SinLatitude, P);
			bConverged = std::abs(NextLatitude - Latitude) < 1.e-14;
			Latitude = NextLatitude;
			if (bConverged) { break; }
		}
		if (!bConverged) { return false; }

		const double SinLatitude = std::sin(Latitude);
		// Projection on the ellipsoid normal avoids division by cos(latitude) at poles.
		const double Height = P * std::cos(Latitude) + Ecef.Z * SinLatitude
			- SemiMajor * std::sqrt(1.0 - EccentricitySquared * SinLatitude * SinLatitude);
		Result = FGeoCoordinate(Latitude / RadiansPerDegree,
			std::atan2(Ecef.Y, Ecef.X) / RadiansPerDegree, Height);
		return true;
	}
}

bool FGeoReference::IsValidCoordinate(const FGeoCoordinate& Coordinate)
{
	return std::isfinite(Coordinate.LatitudeDegrees) && std::isfinite(Coordinate.LongitudeDegrees)
		&& std::isfinite(Coordinate.AltitudeMeters)
		&& Coordinate.LatitudeDegrees >= -90.0 && Coordinate.LatitudeDegrees <= 90.0
		&& Coordinate.LongitudeDegrees >= -180.0 && Coordinate.LongitudeDegrees <= 180.0
		&& Coordinate.AltitudeMeters >= MinAltitudeMeters && Coordinate.AltitudeMeters <= MaxAltitudeMeters;
}

EGeoConversionResult FGeoReference::SetOrigin(const FGeoCoordinate& InOrigin)
{
	if (!IsValidCoordinate(InOrigin)) { return EGeoConversionResult::InvalidCoordinate; }
	const double Latitude = InOrigin.LatitudeDegrees * RadiansPerDegree;
	const double Longitude = InOrigin.LongitudeDegrees * RadiansPerDegree;
	const double SinLat = std::sin(Latitude), CosLat = std::cos(Latitude);
	const double SinLon = std::sin(Longitude), CosLon = std::cos(Longitude);
	Origin = InOrigin;
	OriginEcefMeters = ToEcef(InOrigin);
	North = FVector3d(-SinLat * CosLon, -SinLat * SinLon, CosLat);
	East = FVector3d(-SinLon, CosLon, 0.0);
	Up = FVector3d(CosLat * CosLon, CosLat * SinLon, SinLat);
	bHasOrigin = true;
	return EGeoConversionResult::Success;
}

void FGeoReference::ClearOrigin()
{
	*this = FGeoReference();
}

bool FGeoReference::TryGetOrigin(FGeoCoordinate& OutOrigin) const
{
	if (!bHasOrigin) { return false; }
	OutOrigin = Origin;
	return true;
}

EGeoConversionResult FGeoReference::GeographicToLocal(const FGeoCoordinate& Geographic, FLocalGeoCoordinate& OutLocal) const
{
	if (!bHasOrigin) { return EGeoConversionResult::OriginNotConfigured; }
	if (!IsValidCoordinate(Geographic)) { return EGeoConversionResult::InvalidCoordinate; }
	const FVector3d Delta = ToEcef(Geographic) - OriginEcefMeters;
	if (Delta.Length() > MaxLocalRadiusMeters + BoundaryToleranceMeters) { return EGeoConversionResult::OutsideLocalDomain; }
	const FVector3d Local = FVector3d(FVector3d::DotProduct(North, Delta),
		FVector3d::DotProduct(East, Delta), FVector3d::DotProduct(Up, Delta)) * CentimetersPerMeter;
	if (!IsFinite(Local)) { return EGeoConversionResult::NumericalFailure; }
	OutLocal = FLocalGeoCoordinate(Local);
	return EGeoConversionResult::Success;
}

EGeoConversionResult FGeoReference::LocalToGeographic(const FLocalGeoCoordinate& Local, FGeoCoordinate& OutGeographic) const
{
	if (!bHasOrigin) { return EGeoConversionResult::OriginNotConfigured; }
	if (!IsFinite(Local.Centimeters)) { return EGeoConversionResult::InvalidCoordinate; }
	const FVector3d Meters = Local.Centimeters / CentimetersPerMeter;
	// hypot is overflow-safe even for finite but enormous input coordinates.
	if (std::hypot(std::hypot(Meters.X, Meters.Y), Meters.Z) > MaxLocalRadiusMeters + BoundaryToleranceMeters)
	{
		return EGeoConversionResult::OutsideLocalDomain;
	}
	const FVector3d Ecef = OriginEcefMeters + North * Meters.X + East * Meters.Y + Up * Meters.Z;
	FGeoCoordinate Result;
	if (!FromEcef(Ecef, Origin.LongitudeDegrees, Result)) { return EGeoConversionResult::NumericalFailure; }
	// Remove only sub-micrometer inverse roundoff at the accepted height boundaries.
	if (Result.AltitudeMeters < MinAltitudeMeters - BoundaryToleranceMeters
		|| Result.AltitudeMeters > MaxAltitudeMeters + BoundaryToleranceMeters)
	{
		return EGeoConversionResult::OutsideLocalDomain;
	}
	Result.AltitudeMeters = FMath::Clamp(Result.AltitudeMeters, MinAltitudeMeters, MaxAltitudeMeters);
	if (!IsValidCoordinate(Result)) { return EGeoConversionResult::NumericalFailure; }
	OutGeographic = Result;
	return EGeoConversionResult::Success;
}
