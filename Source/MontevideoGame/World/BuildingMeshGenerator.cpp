#include "World/BuildingMeshGenerator.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "HAL/PlatformTime.h"

using namespace UE::Geometry;

bool FBuildingMeshGenerator::Build(const FBuildingFootprintData& Data, FDynamicMesh3& Out, FBuildingMeshMetrics& OutMetrics, FString& Error)
{
	const double Start = FPlatformTime::Seconds();
	FDynamicMesh3 Mesh(false,false,false,true);
	Mesh.EnableAttributes();
	Mesh.Attributes()->SetNumUVLayers(0);
	Mesh.Attributes()->EnablePrimaryColors();
	FBuildingMeshMetrics Metrics;
	Error = TEXT("Invalid building extrusion topology");
	auto Triangle = [&](int32 A, int32 B, int32 C, int32 Group)
	{
		const FVector3d Cross = VectorUtil::Normal(Mesh.GetVertex(A),Mesh.GetVertex(B),Mesh.GetVertex(C));
		if (Cross.SquaredLength()<0.5) { return false; }
		const int32 Id = Mesh.AppendTriangle(A,B,C,Group);
		if (Id<0) { return false; }
		// VectorUtil::Normal already reverses the cross product for Unreal's clockwise front faces.
		const FVector3f Normal(Cross.X,Cross.Y,Cross.Z);
		const float Shade = Normal.Z>0.5f ? 1.0f : 0.65f + 0.12f*Normal.X - 0.08f*Normal.Y;
		const FVector4f Color(0.55f*Shade,0.49f*Shade,0.38f*Shade,1);
		auto* Colors = Mesh.Attributes()->PrimaryColors();
		auto* Normals = Mesh.Attributes()->PrimaryNormals();
		FIndex3i ColorIds, NormalIds;
		for (int32 K=0; K<3; ++K) { ColorIds[K]=Colors->AppendElement(Color); NormalIds[K]=Normals->AppendElement(Normal); }
		Colors->SetTriangle(Id,ColorIds); Normals->SetTriangle(Id,NormalIds);
		return true;
	};
	for (int32 Group=0; Group<Data.Footprints.Num(); ++Group)
	{
		const auto& Building = Data.Footprints[Group];
		const auto& Footprint = Building.Geometry;
		const int32 Count = Footprint.Vertices.Num(), Base = Mesh.MaxVertexID(), Top = Base+Count;
		if (Count<3 || !FMath::IsFinite(Building.HeightMeters) || Building.HeightMeters<=0 || Building.HeightMeters>1000
			|| Footprint.Triangles.IsEmpty() || Footprint.Triangles.Num()%3 || Footprint.Rings.IsEmpty()) { return false; }
		for (int32 Level=0; Level<2; ++Level)
		{
			for (const FVector& XY : Footprint.Vertices)
			{
				if (XY.ContainsNaN() || XY.Z!=0 || XY.Size2D()>50000) { return false; }
				const FVector Position(XY.X,XY.Y,Level*Building.HeightMeters*100);
				Mesh.AppendVertex(Position); Metrics.Bounds+=Position;
			}
		}
		for (int32 I=0; I<Footprint.Triangles.Num(); I+=3)
		{
			const int32 A=Footprint.Triangles[I], B=Footprint.Triangles[I+1], C=Footprint.Triangles[I+2];
			if (A<0 || B<0 || C<0 || A>=Count || B>=Count || C>=Count
				|| !Triangle(Top+A,Top+C,Top+B,Group) || !Triangle(Base+A,Base+B,Base+C,Group)) { return false; }
			++Metrics.RoofTriangles;
		}
		for (int32 R=0; R<Footprint.Rings.Num(); ++R)
		{
			const auto& Ring = Footprint.Rings[R];
			if (Ring.Num()<3) { return false; }
			double Area2=0;
			for (int32 K=0; K<Ring.Num(); ++K)
			{
				const int32 A=Ring[K], B=Ring[(K+1)%Ring.Num()];
				if (A<0 || A>=Count || B<0 || B>=Count || A==B) { return false; }
				Area2+=Footprint.Vertices[A].X*Footprint.Vertices[B].Y-Footprint.Vertices[B].X*Footprint.Vertices[A].Y;
			}
			if (FMath::Abs(Area2)<1e-8) { return false; }
			const bool Reverse = (Area2>0) != (R==0); // Outer CCW, holes CW; independent of source winding.
			for (int32 K=0; K<Ring.Num(); ++K)
			{
				int32 A=Ring[K], B=Ring[(K+1)%Ring.Num()];
				if (Reverse) { Swap(A,B); }
				if (!Triangle(Base+A,Top+B,Base+B,Group) || !Triangle(Base+A,Top+A,Top+B,Group)) { return false; }
				Metrics.WallTriangles+=2;
			}
		}
	}
	if (Mesh.TriangleCount()==0 || !Mesh.CheckValidity(false,EValidityCheckFailMode::ReturnOnly)) { return false; }
	for (int32 Edge : Mesh.EdgeIndicesItr()) { if (Mesh.IsBoundaryEdge(Edge)) { Error=TEXT("Building solid has an open boundary"); return false; } }
	Metrics.Vertices=Mesh.VertexCount(); Metrics.Triangles=Mesh.TriangleCount();
	// Payload estimate only: excludes allocator slack, adjacency, collision and GPU resources.
	Metrics.ApproximateMeshBytes=static_cast<int64>(Metrics.Vertices)*sizeof(FVector3d)
		+static_cast<int64>(Metrics.Triangles)*(sizeof(FIndex3i)+sizeof(int32)+3*(sizeof(FVector3f)+sizeof(FVector4f))+2*sizeof(FIndex3i));
	Metrics.GenerationMilliseconds=(FPlatformTime::Seconds()-Start)*1000;
	Out=MoveTemp(Mesh); OutMetrics=Metrics; Error.Reset();
	return true;
}
