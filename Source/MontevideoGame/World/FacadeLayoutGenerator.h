#pragma once
#include "Geospatial/BuildingFootprintData.h"

enum class EFacadePiece : uint8 { Wall, Ground, Band, Window, Frame, Door, Balcony, Cornice };

/** Generic development styles, never architectural classification of the source building. Units: cm. */
struct FFacadeStyleDefinition
{
	FString Name;
	double BayWidth=300, WindowWidth=120, WindowHeight=150, GroundRatio=1.1, CorniceDepth=12;
	uint32 BalconyPercent=0;
	FVector4f Color=FVector4f(0.6f,0.5f,0.4f,1);
};

struct FFacadeSegment
{
	int32 Footprint=0, Edge=0, Style=0, Floors=0, Bays=0;
	FString BuildingId, SourceFeatureId;
	FVector Origin=FVector::ZeroVector, Tangent=FVector::ForwardVector, Outward=FVector::RightVector;
	double Length=0, Height=0, Margin=0;
	TArray<double> FloorZ;
};

/** Oriented box in one segment's (along edge, outward, Z) coordinates. */
struct FFacadeElement
{
	int32 Segment=0, Floor=0;
	EFacadePiece Kind=EFacadePiece::Wall;
	double U0=0,U1=0,D0=0,D1=0,Z0=0,Z1=0;
	FVector4f Color=FVector4f(1,1,1,1);
};

struct FBuildingFacadeData
{
	TArray<FFacadeSegment> Segments;
	TArray<FFacadeElement> Elements;
	int32 Buildings=0,Bays=0,Windows=0,Doors=0,Balconies=0,ShortEdges=0,RejectedDecorations=0;
	int32 StyleBuildings[4]={0,0,0,0};
	double LayoutMilliseconds=0;
};

class MONTEVIDEOGAME_API FFacadeLayoutGenerator
{
public:
	static FFacadeStyleDefinition Style(int32 Id);
	/** StyleOverride -1: seed selection; 0..3: temporary debug override. No GIS mutation. */
	static bool Build(const FBuildingFootprintData& Source,FBuildingFacadeData& Out,FString& Error,int32 StyleOverride=-1);
	/** Conservative plan clearance: projected decoration must not overlap any mass polygon. */
	static bool IsClear(const FBuildingFootprintData& Source,const FFacadeSegment& Segment,double U0,double U1,double D0,double D1);
};
