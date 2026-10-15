#pragma once
#include "World/FacadeLayoutGenerator.h"
#include "DynamicMesh/DynamicMesh3.h"

struct FFacadeMeshMetrics
{
	int32 Vertices=0,Triangles=0;
	int64 PayloadBytes=0;
	double MeshMilliseconds=0;
	FBox Bounds=FBox(ForceInit);
};

class MONTEVIDEOGAME_API FFacadeMeshGenerator
{
public:
	static bool Build(const FBuildingFacadeData& Layout,UE::Geometry::FDynamicMesh3& Out,FFacadeMeshMetrics& Metrics,FString& Error);
};
