#include "World/FacadeLayoutGenerator.h"
#include "HAL/PlatformTime.h"

namespace FacadeLayoutPrivate
{
	double Cross(const FVector& A,const FVector& B) { return A.X*B.Y-A.Y*B.X; }
	bool Inside(const FVector& P,const FGeospatialBlockoutFeature& G)
	{
		bool Result=false;
		for (const auto& R:G.Rings)
		{
			for(int32 I=0,J=R.Num()-1;I<R.Num();J=I++)
			{
				const FVector A=G.Vertices[R[I]],B=G.Vertices[R[J]];
				if ((A.Y>P.Y)!=(B.Y>P.Y) && P.X<(B.X-A.X)*(P.Y-A.Y)/(B.Y-A.Y)+A.X) { Result=!Result; }
			}
		}
		return Result;
	}
	bool Intersects(const FVector& A,const FVector& B,const FVector& C,const FVector& D)
	{
		const double P=Cross(B-A,C-A),Q=Cross(B-A,D-A),R=Cross(D-C,A-C),S=Cross(D-C,B-C);
		return ((P>0 && Q<0)||(P<0 && Q>0)) && ((R>0 && S<0)||(R<0 && S>0));
	}
	uint32 Mix(uint32 Seed,uint32 Edge,uint32 Floor,uint32 Bay)
	{
		uint32 H=Seed ^ (Edge*0x9e3779b9u) ^ (Floor*0x85ebca6bu) ^ (Bay*0xc2b2ae35u);
		H^=H>>16; H*=0x7feb352du; H^=H>>15; H*=0x846ca68bu; return H^(H>>16);
	}
}

FFacadeStyleDefinition FFacadeLayoutGenerator::Style(int32 Id)
{
	FFacadeStyleDefinition S;
	switch(Id)
	{
	case 0:S.Name=TEXT("Classic");S.Color=FVector4f(.62f,.49f,.32f,1);S.BalconyPercent=24;S.CorniceDepth=18;break;
	case 1:S.Name=TEXT("Modern");S.Color=FVector4f(.36f,.43f,.49f,1);S.BayWidth=340;S.WindowWidth=180;S.CorniceDepth=8;S.GroundRatio=1.15;break;
	case 2:S.Name=TEXT("Residential");S.Color=FVector4f(.55f,.29f,.20f,1);S.BayWidth=300;S.BalconyPercent=40;S.CorniceDepth=12;break;
	default:S.Name=TEXT("Civic");S.Color=FVector4f(.62f,.61f,.51f,1);S.BayWidth=380;S.WindowWidth=140;S.WindowHeight=170;S.GroundRatio=1.2;S.CorniceDepth=20;break;
	}
	return S;
}

bool FFacadeLayoutGenerator::IsClear(const FBuildingFootprintData& Source,const FFacadeSegment& S,double U0,double U1,double D0,double D1)
{
	using namespace FacadeLayoutPrivate;
	const FVector P[4]={S.Origin+S.Tangent*U0+S.Outward*D0,S.Origin+S.Tangent*U1+S.Outward*D0,
		S.Origin+S.Tangent*U1+S.Outward*D1,S.Origin+S.Tangent*U0+S.Outward*D1};
	for(const auto& F:Source.Footprints)
	{
		const auto& G=F.Geometry;
		for(const FVector& Corner:P) { if(Inside(Corner,G)) { return false; } }
		for(const auto& Ring:G.Rings) for(int32 I=0;I<Ring.Num();++I)
		{
			const FVector A=G.Vertices[Ring[I]],B=G.Vertices[Ring[(I+1)%Ring.Num()]];
			const FVector V=A-S.Origin;
			const double U=FVector::DotProduct(V,S.Tangent),D=FVector::DotProduct(V,S.Outward);
			if(U>U0 && U<U1 && D>D0 && D<D1) { return false; }
			for(int32 K=0;K<4;++K) { if(Intersects(P[K],P[(K+1)%4],A,B)) { return false; } }
		}
	}
	return true;
}

bool FFacadeLayoutGenerator::Build(const FBuildingFootprintData& Source,FBuildingFacadeData& Out,FString& Error,int32 Override)
{
	const double Start=FPlatformTime::Seconds();
	Error=TEXT("Invalid facade input or budget exceeded");
	if(Override < -1 || Override>3 || Source.Footprints.IsEmpty() || Source.Footprints.Num()>2000) { return false; }
	FBuildingFacadeData Data;
	TMap<FString,uint32> Seen;
	for(int32 F=0;F<Source.Footprints.Num();++F)
	{
		const auto& B=Source.Footprints[F]; const auto& G=B.Geometry;
		if(B.BuildingId.IsEmpty() || B.SourceFeatureId.IsEmpty() || B.Floors<1 || B.Floors>100
			|| !FMath::IsFinite(B.HeightMeters) || B.HeightMeters<=0 || B.HeightMeters>1000 || G.Rings.IsEmpty()) { return false; }
		const double Average=B.HeightMeters*100/B.Floors;
		if(Average<220 || Average>600) { Error=TEXT("Facade floors require 2.2 to 6 m average height");return false; }
		for(const FVector& V:G.Vertices) { if(V.ContainsNaN() || V.Z!=0 || V.Size2D()>50000) { return false; } }
		for(const auto& R:G.Rings) { if(R.Num()<3) { return false; } for(int32 I:R) { if(!G.Vertices.IsValidIndex(I)) { return false; } } }
		const int32 StyleId=Override<0?static_cast<int32>(B.Seed%4):Override;
		if(const uint32* Existing=Seen.Find(B.BuildingId)) { if(*Existing!=B.Seed) { return false; } }
		else { Seen.Add(B.BuildingId,B.Seed);++Data.StyleBuildings[StyleId]; }
		const auto StyleDef=Style(StyleId);
		const auto& Ring=G.Rings[0]; double Area=0;
		for(int32 I=0;I<Ring.Num();++I) { Area+=FacadeLayoutPrivate::Cross(G.Vertices[Ring[I]],G.Vertices[Ring[(I+1)%Ring.Num()]]); }
		if(FMath::Abs(Area)<1e-8) { return false; }
		for(int32 I=0;I<Ring.Num();++I)
		{
			FVector A=G.Vertices[Ring[I]],BPoint=G.Vertices[Ring[(I+1)%Ring.Num()]];
			if(Area<0) { Swap(A,BPoint); }
			FFacadeSegment S; S.Footprint=F; S.Edge=I; S.Style=StyleId;S.Floors=B.Floors;
			S.BuildingId=B.BuildingId;S.SourceFeatureId=B.SourceFeatureId;
			S.Origin=A;S.Length=(BPoint-A).Size(); if(S.Length<1e-6) { return false; }
			S.Tangent=(BPoint-A)/S.Length; S.Outward=FVector(S.Tangent.Y,-S.Tangent.X,0);
			S.Height=B.HeightMeters*100; S.Margin=75;
			S.Bays=FMath::Max(0,FMath::FloorToInt((S.Length-2*S.Margin)/StyleDef.BayWidth));
			if(S.Bays==0) { ++Data.ShortEdges; }
			if(S.Bays>1000 || Data.Segments.Num()>10000) { return false; }
			const double Ground=B.Floors==1?S.Height:Average*StyleDef.GroundRatio;
			S.FloorZ.Add(0);for(int32 Floor=1;Floor<B.Floors;++Floor) { S.FloorZ.Add(Ground+(Floor-1)*(S.Height-Ground)/(B.Floors-1)); } S.FloorZ.Add(S.Height);
			Data.Bays+=S.Bays;Data.Segments.Add(MoveTemp(S));
		}
	}
	Data.Buildings=Seen.Num();
	// A single development door per building, longest viable exterior edge across all parts.
	TMap<FString,int32> DoorSegments;
	for(int32 I=0;I<Data.Segments.Num();++I)
	{
		const auto& S=Data.Segments[I];if(S.Bays<1) { continue; }
		const double Bay=(S.Length-2*S.Margin)/S.Bays;
		const double Center=S.Margin+(S.Bays/2+.5)*Bay;
		if(!IsClear(Source,S,Center-90,Center+90,2,10)) { continue; }
		const int32* Previous=DoorSegments.Find(S.BuildingId);
		if(!Previous || S.Length>Data.Segments[*Previous].Length) { DoorSegments.Add(S.BuildingId,I); }
	}
	auto Add=[&](int32 Segment,int32 Floor,EFacadePiece Kind,double U0,double U1,double D0,double D1,double Z0,double Z1,FVector4f Color)
	{
		FFacadeElement E;E.Segment=Segment;E.Floor=Floor;E.Kind=Kind;E.U0=U0;E.U1=U1;E.D0=D0;E.D1=D1;E.Z0=Z0;E.Z1=Z1;E.Color=Color;Data.Elements.Add(E);
	};
	const FVector4f Dark(.025f,.045f,.065f,1),Trim(.72f,.70f,.61f,1),Rail(.10f,.12f,.14f,1);
	for(int32 I=0;I<Data.Segments.Num();++I)
	{
		const auto& S=Data.Segments[I];const auto Def=Style(S.Style);const uint32 Seed=Source.Footprints[S.Footprint].Seed;
		const double SkinMargin=FMath::Min(2.0,S.Length*.2);
		// Thin presentation skin leaves the original solid, coordinates and collision untouched.
		if(IsClear(Source,S,SkinMargin,S.Length-SkinMargin,.5,1.7))
		{
			Add(I,0,EFacadePiece::Wall,SkinMargin,S.Length-SkinMargin,.5,1,1,S.Height,Def.Color);
			FVector4f Ground=Def.Color*.65f;Ground.W=1;
			Add(I,0,EFacadePiece::Ground,SkinMargin,S.Length-SkinMargin,1.2,1.7,1,S.FloorZ[1]-4,Ground);
		}
		const double TrimMargin=FMath::Min(75.0,S.Length*.25);
		if(IsClear(Source,S,TrimMargin,S.Length-TrimMargin,2,Def.CorniceDepth))
		{
			Add(I,S.Floors-1,EFacadePiece::Cornice,TrimMargin,S.Length-TrimMargin,2,Def.CorniceDepth,S.Height-18,S.Height,Trim);
			for(int32 Floor=1;Floor<S.Floors;++Floor) { Add(I,Floor,EFacadePiece::Band,TrimMargin,S.Length-TrimMargin,2,4,S.FloorZ[Floor]-5,S.FloorZ[Floor]+5,Trim); }
		}
		if(S.Bays==0) { continue; }
		const double Bay=(S.Length-2*S.Margin)/S.Bays;
		for(int32 Floor=0;Floor<S.Floors;++Floor) for(int32 B=0;B<S.Bays;++B)
		{
			const double Center=S.Margin+(B+.5)*Bay;
			const int32* Door=DoorSegments.Find(S.BuildingId);
			const bool IsDoor=Floor==0 && Door && *Door==I && B==S.Bays/2;
			const double Width=IsDoor?150.0:Def.WindowWidth*(Floor==0?1.15:1.0);
			const double U0=Center-Width/2,U1=Center+Width/2;
			const double Low=S.FloorZ[Floor],High=S.FloorZ[Floor+1];
			const double Z0=IsDoor?2:Low+(Floor==0?55:80),Z1=IsDoor?FMath::Min(245.0,High-35):FMath::Min(Z0+Def.WindowHeight,High-35);
			if(!IsClear(Source,S,U0-7,U1+7,2,10)) { ++Data.RejectedDecorations;continue; }
			Add(I,Floor,IsDoor?EFacadePiece::Door:EFacadePiece::Window,U0,U1,2,3,Z0,Z1,Dark);
			Add(I,Floor,EFacadePiece::Frame,U0-7,U0,3,8,Z0,Z1+7,Trim);
			Add(I,Floor,EFacadePiece::Frame,U1,U1+7,3,8,Z0,Z1+7,Trim);
			Add(I,Floor,EFacadePiece::Frame,U0,U1,3,8,Z1,Z1+7,Trim);
			if(!IsDoor) { Add(I,Floor,EFacadePiece::Frame,U0,U1,3,8,Z0-7,Z0,Trim);++Data.Windows; } else { ++Data.Doors; }
			if(Floor>0 && Bay>=280 && FacadeLayoutPrivate::Mix(Seed,S.Edge+S.Footprint*1009,Floor,B)%100<Def.BalconyPercent)
			{
				const double Left=Center-100,Right=Center+100;
				if(Left>=S.Margin && Right<=S.Length-S.Margin && IsClear(Source,S,Left,Right,2,45))
				{
					Add(I,Floor,EFacadePiece::Balcony,Left,Right,2,45,Low+4,Low+14,Trim);
					Add(I,Floor,EFacadePiece::Balcony,Left,Right,39,45,Low+14,Low+105,Rail);
					Add(I,Floor,EFacadePiece::Balcony,Left,Left+6,2,39,Low+14,Low+105,Rail);
					Add(I,Floor,EFacadePiece::Balcony,Right-6,Right,2,39,Low+14,Low+105,Rail);++Data.Balconies;
				}
				else { ++Data.RejectedDecorations; }
			}
		}
		if(Data.Elements.Num()>100000) { return false; }
	}
	Data.LayoutMilliseconds=(FPlatformTime::Seconds()-Start)*1000;Out=MoveTemp(Data);Error.Reset();return true;
}
