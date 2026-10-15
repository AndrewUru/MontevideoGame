#include "World/FacadeInstanceBatches.h"
#include "HAL/PlatformTime.h"

int32 FFacadeInstanceBatches::Slot(const FFacadeElement& E,const FFacadeSegment& S)
{
	switch(E.Kind)
	{
	case EFacadePiece::Wall: return S.Style;
	case EFacadePiece::Ground: return 4+S.Style;
	case EFacadePiece::Band: return 8;
	case EFacadePiece::Window: return 9;
	case EFacadePiece::Frame: return 10;
	case EFacadePiece::Door: return 11;
	case EFacadePiece::Balcony: return E.Color.X<.2f?13:12;
	case EFacadePiece::Cornice: return 14;
	}
	return INDEX_NONE;
}

bool FFacadeInstanceBatches::Build(const FBuildingFacadeData& Layout,FFacadeInstanceData& Out,FString& Error)
{
	const double Start=FPlatformTime::Seconds();
	Error=TEXT("Invalid facade instance input");
	if(Layout.Segments.IsEmpty() || Layout.Elements.IsEmpty() || Layout.Elements.Num()>100000) { return false; }
	FFacadeInstanceData Result;
	static const TCHAR* Names[SlotCount]={TEXT("WallClassic"),TEXT("WallModern"),TEXT("WallResidential"),TEXT("WallCivic"),
		TEXT("GroundClassic"),TEXT("GroundModern"),TEXT("GroundResidential"),TEXT("GroundCivic"),
		TEXT("FloorBand"),TEXT("WindowSimple"),TEXT("WindowFrame"),TEXT("DoorSimple"),
		TEXT("BalconySlab"),TEXT("BalconyRail"),TEXT("CorniceSimple")};
	Result.Batches.SetNum(SlotCount);
	for(int32 I=0;I<SlotCount;++I) { Result.Batches[I].Name=Names[I]; }
	bool Used[SlotCount]={};
	for(int32 Index=0;Index<Layout.Elements.Num();++Index)
	{
		const FFacadeElement& E=Layout.Elements[Index];
		if(!Layout.Segments.IsValidIndex(E.Segment)) { return false; }
		const FFacadeSegment& S=Layout.Segments[E.Segment];
		if(S.Style<0 || S.Style>3 || S.Origin.ContainsNaN() || S.Tangent.ContainsNaN() || S.Outward.ContainsNaN()
			|| !FMath::IsFinite(S.Length) || !FMath::IsFinite(S.Height) || !S.Tangent.IsNormalized() || !S.Outward.IsNormalized()
			|| !FMath::IsFinite(E.U0) || !FMath::IsFinite(E.U1) || !FMath::IsFinite(E.D0) || !FMath::IsFinite(E.D1)
			|| !FMath::IsFinite(E.Z0) || !FMath::IsFinite(E.Z1) || E.U0<0 || E.U1>S.Length || E.U1<=E.U0
			|| E.D0<0 || E.D1<=E.D0 || E.Z0<0 || E.Z1>S.Height || E.Z1<=E.Z0
			|| !FMath::IsFinite(E.Color.X) || !FMath::IsFinite(E.Color.Y) || !FMath::IsFinite(E.Color.Z) || !FMath::IsFinite(E.Color.W)) { return false; }
		const int32 SlotIndex=Slot(E,S); if(SlotIndex<0) { return false; }
		FFacadeInstanceBatch& Batch=Result.Batches[SlotIndex];
		if(Used[SlotIndex] && Batch.Color!=E.Color) { Error=TEXT("A facade batch has nonuniform color");return false; }
		Batch.Color=E.Color;Used[SlotIndex]=true;
		const FVector Origin=S.Origin+S.Tangent*E.U0+S.Outward*E.D0+FVector(0,0,E.Z0);
		const FQuat Rotation=FRotationMatrix::MakeFromXZ(S.Tangent,FVector::UpVector).ToQuat();
		// Module y is [-1,0]. Rotation +Y is inward, so positive Y scale projects outward.
		const FTransform Transform(Rotation,Origin,FVector(E.U1-E.U0,E.D1-E.D0,E.Z1-E.Z0));
		if(Origin.ContainsNaN() || Transform.ContainsNaN()) { return false; }
		Batch.Transforms.Add(Transform);Batch.ElementIndices.Add(Index);++Result.Instances;
		Result.Bounds+=Origin;Result.Bounds+=Origin+S.Tangent*(E.U1-E.U0)+S.Outward*(E.D1-E.D0)+FVector(0,0,E.Z1-E.Z0);
		if(E.Kind==EFacadePiece::Window) { ++Result.Windows; }
		else if(E.Kind==EFacadePiece::Door) { ++Result.Doors; }
		else if(E.Kind==EFacadePiece::Balcony && SlotIndex==12) { ++Result.Balconies; }
	}
	if(Result.Windows!=Layout.Windows || Result.Doors!=Layout.Doors || Result.Balconies!=Layout.Balconies) { Error=TEXT("Instance semantic counts differ from layout");return false; }
	Result.BatchingMilliseconds=(FPlatformTime::Seconds()-Start)*1000;
	Out=MoveTemp(Result);Error.Reset();return true;
}

bool FFacadeInstanceBatches::Compare(const FBuildingFacadeData& Legacy,const FBuildingFacadeData& Current,const FFacadeInstanceData& Instances,FString& Error)
{
	Error=TEXT("Layout or instance parity mismatch");
	if(Legacy.Buildings!=Current.Buildings || Legacy.Windows!=Current.Windows || Legacy.Doors!=Current.Doors || Legacy.Balconies!=Current.Balconies
		|| Legacy.Segments.Num()!=Current.Segments.Num() || Legacy.Elements.Num()!=Current.Elements.Num() || Instances.Instances!=Current.Elements.Num()) { return false; }
	for(int32 I=0;I<4;++I) { if(Legacy.StyleBuildings[I]!=Current.StyleBuildings[I]) { return false; } }
	for(int32 I=0;I<Legacy.Segments.Num();++I)
	{
		const auto& A=Legacy.Segments[I];const auto& B=Current.Segments[I];
		if(A.BuildingId!=B.BuildingId || A.SourceFeatureId!=B.SourceFeatureId || A.Footprint!=B.Footprint || A.Edge!=B.Edge || A.Style!=B.Style
			|| A.Floors!=B.Floors || A.Bays!=B.Bays || A.Origin!=B.Origin || A.Tangent!=B.Tangent || A.Outward!=B.Outward
			|| A.Length!=B.Length || A.Height!=B.Height || A.Margin!=B.Margin || A.FloorZ!=B.FloorZ) { return false; }
	}
	TArray<bool> Found;Found.Init(false,Current.Elements.Num());
	for(int32 SlotIndex=0;SlotIndex<Instances.Batches.Num();++SlotIndex)
	{
		const auto& Batch=Instances.Batches[SlotIndex];if(Batch.Transforms.Num()!=Batch.ElementIndices.Num()) { return false; }
		for(int32 K=0;K<Batch.Transforms.Num();++K)
		{
			const int32 I=Batch.ElementIndices[K];if(!Current.Elements.IsValidIndex(I) || Found[I]) { return false; }Found[I]=true;
			const auto& E=Current.Elements[I];const auto& Old=Legacy.Elements[I];
			if(E.Segment!=Old.Segment || E.Floor!=Old.Floor || E.Kind!=Old.Kind || E.U0!=Old.U0 || E.U1!=Old.U1
				|| E.D0!=Old.D0 || E.D1!=Old.D1 || E.Z0!=Old.Z0 || E.Z1!=Old.Z1 || E.Color!=Old.Color || Slot(E,Current.Segments[E.Segment])!=SlotIndex || Batch.Color!=E.Color) { return false; }
			const auto& S=Current.Segments[E.Segment];const auto& T=Batch.Transforms[K];
			const FVector Low=S.Origin+S.Tangent*E.U0+S.Outward*E.D0+FVector(0,0,E.Z0);
			const FVector High=S.Origin+S.Tangent*E.U1+S.Outward*E.D1+FVector(0,0,E.Z1);
			// Runtime mesh is a unit prism x=[0,1], y=[-1,0], z=[0,1].
			if(!T.TransformPosition(FVector(0,0,0)).Equals(Low,0.02) || !T.TransformPosition(FVector(1,-1,1)).Equals(High,0.02)) { return false; }
		}
	}
	for(bool Has:Found) { if(!Has) { return false; } }
	Error.Reset();return true;
}
