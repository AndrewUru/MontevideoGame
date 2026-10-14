#include "Geospatial/GeoReference.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Opt-in integration test. Python validates the snapshot and supplies its centroid;
// no geographic constants or JSON runtime dependency are added to gameplay.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeoProvenanceOriginTest, "Montevideo.DataProvenance.PlazaOrigin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGeoProvenanceOriginTest::RunTest(const FString&)
{
	double Latitude = 0.0;
	double Longitude = 0.0;
	if (!FParse::Value(FCommandLine::Get(), TEXT("GeoTestLatitude="), Latitude)
		|| !FParse::Value(FCommandLine::Get(), TEXT("GeoTestLongitude="), Longitude))
	{
		AddError(TEXT("Run Scripts/geospatial/validate_public_spaces.py --unreal-editor <executable> to supply the verified centroid."));
		return false;
	}

	// These heights are mathematical fixtures, NOT measured heights of the plaza.
	// Testing two heights verifies origin cancellation without choosing a real datum.
	for (double SyntheticHeightMeters : {0.0, 123.0})
	{
		const FGeoCoordinate Origin(Latitude, Longitude, SyntheticHeightMeters);
		FGeoReference Reference;
		if (!TestEqual(TEXT("Set verified horizontal origin"), Reference.SetOrigin(Origin), EGeoConversionResult::Success))
		{
			return false;
		}
		FLocalGeoCoordinate Local;
		if (!TestEqual(TEXT("Convert centroid"), Reference.GeographicToLocal(Origin, Local), EGeoConversionResult::Success))
		{
			return false;
		}
		TestTrue(TEXT("Origin maps to zero within 0.001 cm"), Local.Centimeters.Equals(FVector::ZeroVector, 0.001));
		AddInfo(FString::Printf(TEXT("Verified centroid lat=%.15f lon=%.15f; synthetic height=%.1f m; local=%s cm"),
			Latitude, Longitude, SyntheticHeightMeters, *Local.Centimeters.ToString()));
	}
	return true;
}
#endif
