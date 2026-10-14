#include "Geospatial/GeoReference.h"
#include "Geospatial/GeospatialSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace
{
	constexpr EAutomationTestFlags GeoTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr double PositionToleranceCm = 0.001; // 10 micrometers; numerical error, not survey accuracy.
	constexpr double AngleToleranceDegrees = 1.e-9;
	constexpr double HeightToleranceMeters = 1.e-5;

	bool CheckGeo(FAutomationTestBase& Test, const FGeoCoordinate& Actual, const FGeoCoordinate& Expected)
	{
		const double LongitudeError = FMath::Abs(FMath::FindDeltaAngleDegrees(Actual.LongitudeDegrees, Expected.LongitudeDegrees));
		bool bOk = Test.TestTrue(TEXT("Latitude within 1e-9 degrees"), FMath::Abs(Actual.LatitudeDegrees - Expected.LatitudeDegrees) <= AngleToleranceDegrees);
		bOk &= Test.TestTrue(TEXT("Longitude within 1e-9 degrees (wrapped)"), LongitudeError <= AngleToleranceDegrees);
		bOk &= Test.TestTrue(TEXT("Ellipsoidal height within 1e-5 m"), FMath::Abs(Actual.AltitudeMeters - Expected.AltitudeMeters) <= HeightToleranceMeters);
		return bOk;
	}

	bool CheckLocal(FAutomationTestBase& Test, const FLocalGeoCoordinate& Actual, const FVector& Expected)
	{
		return Test.TestTrue(TEXT("3D local error <= 0.001 cm"), FVector::Distance(Actual.Centimeters, Expected) <= PositionToleranceCm);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeoOriginTest, "Montevideo.Geospatial.01Origin", GeoTestFlags)
bool FGeoOriginTest::RunTest(const FString&)
{
	FGeoReference Reference;
	const FGeoCoordinate Origin(-35.0, -56.0, 50.0); // Synthetic, NOT Plaza Independencia.
	TestEqual(TEXT("Set origin"), Reference.SetOrigin(Origin), EGeoConversionResult::Success);
	FLocalGeoCoordinate Local;
	TestEqual(TEXT("Convert origin"), Reference.GeographicToLocal(Origin, Local), EGeoConversionResult::Success);
	CheckLocal(*this, Local, FVector::ZeroVector);
	FGeoCoordinate Back;
	TestEqual(TEXT("Convert zero"), Reference.LocalToGeographic(FLocalGeoCoordinate(), Back), EGeoConversionResult::Success);
	CheckGeo(*this, Back, Origin);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeoRoundTripTest, "Montevideo.Geospatial.02RoundTrip", GeoTestFlags)
bool FGeoRoundTripTest::RunTest(const FString&)
{
	for (const FGeoCoordinate& Origin : {FGeoCoordinate(-35, -56, 50), FGeoCoordinate(0, 0), FGeoCoordinate(45, 12, 1200)})
	{
		FGeoReference Reference;
		Reference.SetOrigin(Origin);
		for (double Offset : {-0.05, -0.001, 0.0, 0.001, 0.05})
		{
			const FGeoCoordinate Target(Origin.LatitudeDegrees + Offset, Origin.LongitudeDegrees - Offset, Origin.AltitudeMeters + 123.456);
			FLocalGeoCoordinate Local;
			FGeoCoordinate Back;
			TestEqual(TEXT("Forward"), Reference.GeographicToLocal(Target, Local), EGeoConversionResult::Success);
			TestEqual(TEXT("Inverse"), Reference.LocalToGeographic(Local, Back), EGeoConversionResult::Success);
			CheckGeo(*this, Back, Target);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeoNorthTest, "Montevideo.Geospatial.03KnownNorth", GeoTestFlags)
bool FGeoNorthTest::RunTest(const FString&)
{
	FGeoReference Reference;
	Reference.SetOrigin(FGeoCoordinate(0, 0));
	FLocalGeoCoordinate Local;
	// Independent meridian ellipse fixture: x=a*cos(t), z=b*sin(t), t=0.001 rad.
	// Geodetic latitude=atan((a/b)*tan(t)). This tests north, curvature and centimeters.
	TestEqual(TEXT("North fixture"), Reference.GeographicToLocal(FGeoCoordinate(0.057488527532579305, 0), Local), EGeoConversionResult::Success);
	CheckLocal(*this, Local, FVector(635675.12547865135, 0, -318.90682341943504));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeoEastTest, "Montevideo.Geospatial.04KnownEast", GeoTestFlags)
bool FGeoEastTest::RunTest(const FString&)
{
	FGeoReference Reference;
	Reference.SetOrigin(FGeoCoordinate(0, 0));
	FLocalGeoCoordinate Local;
	// Equatorial circle: east=a*sin(0.01 deg); up=a*(cos(0.01 deg)-1).
	TestEqual(TEXT("East fixture"), Reference.GeographicToLocal(FGeoCoordinate(0, 0.01), Local), EGeoConversionResult::Success);
	CheckLocal(*this, Local, FVector(0, 111319.49022810932, -9.714458121567937));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeoAltitudeTest, "Montevideo.Geospatial.05Altitude", GeoTestFlags)
bool FGeoAltitudeTest::RunTest(const FString&)
{
	FGeoReference Reference;
	Reference.SetOrigin(FGeoCoordinate(-35, -56, 50));
	for (double Height : {-1000.0, -100.0, 0.0, 50.0, 150.0, 10000.0})
	{
		FLocalGeoCoordinate Local;
		FGeoCoordinate Back;
		const FGeoCoordinate Target(-35, -56, Height);
		TestEqual(TEXT("Height forward"), Reference.GeographicToLocal(Target, Local), EGeoConversionResult::Success);
		CheckLocal(*this, Local, FVector(0, 0, (Height - 50.0) * 100.0));
		TestEqual(TEXT("Height inverse"), Reference.LocalToGeographic(Local, Back), EGeoConversionResult::Success);
		CheckGeo(*this, Back, Target);
	}
	TestEqual(TEXT("Omitted altitude is ellipsoid zero"), FGeoCoordinate(1, 2).AltitudeMeters, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeoUrbanTest, "Montevideo.Geospatial.06UrbanStability", GeoTestFlags)
bool FGeoUrbanTest::RunTest(const FString&)
{
	FGeoReference Reference;
	Reference.SetOrigin(FGeoCoordinate(-35, -56, 50));
	double MaxErrorCm = 0;
	int32 Count = 0;
	// 605 positions spanning centimeters to ~19.8 km, including signed heights.
	for (int32 X = -5; X <= 5; ++X)
	for (int32 Y = -5; Y <= 5; ++Y)
	for (double Z : {-10000.0, 0.0, 0.01, 10000.0, 50000.0})
	{
		const FLocalGeoCoordinate Target(FVector(X * 280000.0 + 0.125, Y * 280000.0 + 0.375, Z));
		FGeoCoordinate Geo;
		FLocalGeoCoordinate Back;
		TestEqual(TEXT("Urban inverse"), Reference.LocalToGeographic(Target, Geo), EGeoConversionResult::Success);
		TestEqual(TEXT("Urban forward"), Reference.GeographicToLocal(Geo, Back), EGeoConversionResult::Success);
		MaxErrorCm = FMath::Max(MaxErrorCm, FVector::Distance(Back.Centimeters, Target.Centimeters));
		++Count;
	}
	TestTrue(TEXT("Urban maximum error <= 0.001 cm"), MaxErrorCm <= PositionToleranceCm);
	AddInfo(FString::Printf(TEXT("%d urban samples; maximum 3D round-trip error %.12g cm"), Count, MaxErrorCm));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeoInvalidTest, "Montevideo.Geospatial.07InvalidCoordinates", GeoTestFlags)
bool FGeoInvalidTest::RunTest(const FString&)
{
	const double Nan = std::numeric_limits<double>::quiet_NaN();
	const double Inf = std::numeric_limits<double>::infinity();
	FGeoReference Reference;
	FLocalGeoCoordinate Local(FVector(1, 2, 3));
	FGeoCoordinate Geo(1, 2, 3);
	TestEqual(TEXT("No implicit origin forward"), Reference.GeographicToLocal(Geo, Local), EGeoConversionResult::OriginNotConfigured);
	TestEqual(TEXT("No implicit origin inverse"), Reference.LocalToGeographic(Local, Geo), EGeoConversionResult::OriginNotConfigured);
	TestFalse(TEXT("TryGetOrigin unavailable"), Reference.TryGetOrigin(Geo));
	CheckGeo(*this, Geo, FGeoCoordinate(1, 2, 3));
	Reference.SetOrigin(FGeoCoordinate(0, 0));
	for (const FGeoCoordinate& Invalid : {FGeoCoordinate(91, 0), FGeoCoordinate(-91, 0), FGeoCoordinate(0, 181),
		FGeoCoordinate(0, -181), FGeoCoordinate(Nan, 0), FGeoCoordinate(0, Inf), FGeoCoordinate(0, 0, Nan),
		FGeoCoordinate(0, 0, -1001), FGeoCoordinate(0, 0, 10001)})
	{
		TestEqual(TEXT("Reject invalid origin"), Reference.SetOrigin(Invalid), EGeoConversionResult::InvalidCoordinate);
		TestEqual(TEXT("Reject invalid point"), Reference.GeographicToLocal(Invalid, Local), EGeoConversionResult::InvalidCoordinate);
		CheckLocal(*this, Local, FVector(1, 2, 3));
	}
	Reference.TryGetOrigin(Geo);
	CheckGeo(*this, Geo, FGeoCoordinate(0, 0));
	for (const FVector& Invalid : {FVector(Nan, 0, 0), FVector(0, Inf, 0), FVector(0, 0, -Inf)})
	{
		TestEqual(TEXT("Reject non-finite local"), Reference.LocalToGeographic(FLocalGeoCoordinate(Invalid), Geo), EGeoConversionResult::InvalidCoordinate);
		CheckGeo(*this, Geo, FGeoCoordinate(0, 0));
	}
	TestEqual(TEXT("Huge finite local rejected without overflow"), Reference.LocalToGeographic(
		FLocalGeoCoordinate(FVector(std::numeric_limits<double>::max(), 0, 0)), Geo), EGeoConversionResult::OutsideLocalDomain);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeoBoundaryTest, "Montevideo.Geospatial.08DomainPolesDateline", GeoTestFlags)
bool FGeoBoundaryTest::RunTest(const FString&)
{
	FGeoReference Reference;
	Reference.SetOrigin(FGeoCoordinate(0, 0));
	FGeoCoordinate Geo;
	FLocalGeoCoordinate Local;
	TestEqual(TEXT("20 km boundary accepted"), Reference.LocalToGeographic(FLocalGeoCoordinate(FVector(2000000, 0, 0)), Geo), EGeoConversionResult::Success);
	TestEqual(TEXT("Boundary converts back"), Reference.GeographicToLocal(Geo, Local), EGeoConversionResult::Success);
	CheckLocal(*this, Local, FVector(2000000, 0, 0));
	const FGeoCoordinate PreviousGeo = Geo;
	TestEqual(TEXT("Beyond 20 km rejected"), Reference.LocalToGeographic(FLocalGeoCoordinate(FVector(2000001, 0, 0)), Geo), EGeoConversionResult::OutsideLocalDomain);
	CheckGeo(*this, Geo, PreviousGeo);
	TestEqual(TEXT("Distant geographic rejected"), Reference.GeographicToLocal(FGeoCoordinate(1, 0), Local), EGeoConversionResult::OutsideLocalDomain);
	CheckLocal(*this, Local, FVector(2000000, 0, 0));
	TestEqual(TEXT("Height outside domain rejected"), Reference.LocalToGeographic(FLocalGeoCoordinate(FVector(0, 0, 1000100)), Geo), EGeoConversionResult::OutsideLocalDomain);
	Reference.SetOrigin(FGeoCoordinate(0, 179.99));
	TestEqual(TEXT("Cross dateline"), Reference.GeographicToLocal(FGeoCoordinate(0, -179.99), Local), EGeoConversionResult::Success);
	TestTrue(TEXT("Dateline moves east"), Local.Centimeters.Y > 0);
	TestEqual(TEXT("Dateline inverse"), Reference.LocalToGeographic(Local, Geo), EGeoConversionResult::Success);
	CheckGeo(*this, Geo, FGeoCoordinate(0, -179.99));
	for (double Latitude : {-90.0, 90.0})
	{
		Reference.SetOrigin(FGeoCoordinate(Latitude, 45, 10));
		TestEqual(TEXT("Pole inverse"), Reference.LocalToGeographic(FLocalGeoCoordinate(), Geo), EGeoConversionResult::Success);
		CheckGeo(*this, Geo, FGeoCoordinate(Latitude, 45, 10));
		const FLocalGeoCoordinate NearPole(FVector(10000, 20000, 300));
		TestEqual(TEXT("Near pole inverse"), Reference.LocalToGeographic(NearPole, Geo), EGeoConversionResult::Success);
		TestEqual(TEXT("Near pole forward"), Reference.GeographicToLocal(Geo, Local), EGeoConversionResult::Success);
		CheckLocal(*this, Local, NearPole.Centimeters);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeoSubsystemTest, "Montevideo.Geospatial.09SubsystemLifecycle", GeoTestFlags)
bool FGeoSubsystemTest::RunTest(const FString&)
{
	// Real subsystem collection: tests automatic creation, session isolation and shutdown.
	if (!TestNotNull(TEXT("Engine available"), GEngine)) { return false; }
	TStrongObjectPtr<UGameInstance> First(NewObject<UGameInstance>(GEngine));
	TStrongObjectPtr<UGameInstance> Second(NewObject<UGameInstance>(GEngine));
	First->InitializeStandalone();
	Second->InitializeStandalone();
	UGeospatialSubsystem* A = First->GetSubsystem<UGeospatialSubsystem>();
	UGeospatialSubsystem* B = Second->GetSubsystem<UGeospatialSubsystem>();
	if (TestNotNull(TEXT("First auto-created subsystem"), A) && TestNotNull(TEXT("Second auto-created subsystem"), B))
	{
		TestFalse(TEXT("No fabricated default origin"), A->HasOrigin());
		TestFalse(TEXT("No fabricated second origin"), B->HasOrigin());
		TestEqual(TEXT("Set through subsystem"), A->SetOrigin(FGeoCoordinate(-35, -56, 50)), EGeoConversionResult::Success);
		TestFalse(TEXT("Independent sessions"), B->HasOrigin());
		FLocalGeoCoordinate Local;
		FGeoCoordinate Back;
		TestEqual(TEXT("Subsystem forward"), A->GeographicToLocal(FGeoCoordinate(-35, -56, 150), Local), EGeoConversionResult::Success);
		CheckLocal(*this, Local, FVector(0, 0, 10000));
		TestEqual(TEXT("Subsystem inverse"), A->LocalToGeographic(Local, Back), EGeoConversionResult::Success);
		CheckGeo(*this, Back, FGeoCoordinate(-35, -56, 150));
		TestTrue(TEXT("Get configured origin"), A->TryGetOrigin(Back));
		CheckGeo(*this, Back, FGeoCoordinate(-35, -56, 50));
		A->LogStatus();
		A->ClearOrigin();
		TestFalse(TEXT("Explicit clear"), A->HasOrigin());
		TestEqual(TEXT("Conversion disabled after clear"), A->GeographicToLocal(Back, Local), EGeoConversionResult::OriginNotConfigured);
		A->SetOrigin(FGeoCoordinate(0, 0));
	}
	UWorld* FirstWorld = First->GetWorld();
	UWorld* SecondWorld = Second->GetWorld();
	First->Shutdown();
	Second->Shutdown();
	if (A) { TestFalse(TEXT("Deinitialize clears origin"), A->HasOrigin()); }
	// Shutdown releases subsystems, but the owner must dispose of standalone worlds/contexts.
	for (UWorld* World : {FirstWorld, SecondWorld})
	{
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	}
	return true;
}
#endif
