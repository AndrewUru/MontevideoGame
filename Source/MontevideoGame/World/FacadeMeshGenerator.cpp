#include "World/FacadeMeshGenerator.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "HAL/PlatformTime.h"

bool FFacadeMeshGenerator::Build(const FBuildingFacadeData& Layout,UE::Geometry::FDynamicMesh3& Out,FFacadeMeshMetrics& Metrics,FString& Error)
{
	using namespace UE::Geometry;
	const double Start=FPlatformTime::Seconds();
	FDynamicMesh3 Mesh(false,false,false,true);Mesh.EnableAttributes();Mesh.Attributes()->SetNumUVLayers(0);Mesh.Attributes()->EnablePrimaryColors();
	FFacadeMeshMetrics M;Error=TEXT("Invalid facade geometry");
	if(Layout.Elements.Num()>100000 || Layout.Elements.IsEmpty()) { return false; }
	const int32 Faces[6][4]={{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
	for(const auto& E:Layout.Elements)
	{
		if(!Layout.Segments.IsValidIndex(E.Segment)) { return false; }
		const auto& S=Layout.Segments[E.Segment];
		if(!FMath::IsFinite(E.U0)||!FMath::IsFinite(E.U1)||!FMath::IsFinite(E.D0)||!FMath::IsFinite(E.D1)||!FMath::IsFinite(E.Z0)||!FMath::IsFinite(E.Z1)
			|| E.U0<0 || E.U1>S.Length || E.U1<=E.U0 || E.D0<0 || E.D1<=E.D0 || E.Z0<0 || E.Z1>S.Height || E.Z1<=E.Z0) { return false; }
		const int32 Base=Mesh.MaxVertexID(); FVector Center=FVector::ZeroVector;
		for(int32 K=0;K<8;++K)
		{
			const FVector P=S.Origin+S.Tangent*((K&1)?E.U1:E.U0)+S.Outward*((K&2)?E.D1:E.D0)+FVector(0,0,(K&4)?E.Z1:E.Z0);
			if(P.ContainsNaN()) { return false; }Mesh.AppendVertex(P);M.Bounds+=P;Center+=P/8;
		}
		for(const auto& Face:Faces) for(int32 T=0;T<2;++T)
		{
			int32 A=Base+Face[0],B=Base+Face[T+1],C=Base+Face[T+2];
			FVector3d N=VectorUtil::Normal(Mesh.GetVertex(A),Mesh.GetVertex(B),Mesh.GetVertex(C));
			if(N.Dot(Mesh.GetVertex(A)-FVector3d(Center))<0) { Swap(B,C);N=-N; }
			if(N.SquaredLength()<.5) { return false; }
			const int32 Triangle=Mesh.AppendTriangle(A,B,C,E.Segment);if(Triangle<0) { return false; }
			const float Shade=.80f+.12f*static_cast<float>(N.Z)+.06f*static_cast<float>(N.X)-.04f*static_cast<float>(N.Y);
			FVector4f Color=E.Color*Shade;Color.W=1;
			FIndex3i NI,CI;for(int32 K=0;K<3;++K) { NI[K]=Mesh.Attributes()->PrimaryNormals()->AppendElement(FVector3f(N));CI[K]=Mesh.Attributes()->PrimaryColors()->AppendElement(Color); }
			Mesh.Attributes()->PrimaryNormals()->SetTriangle(Triangle,NI);Mesh.Attributes()->PrimaryColors()->SetTriangle(Triangle,CI);
		}
	}
	if(!Mesh.CheckValidity(false,EValidityCheckFailMode::ReturnOnly)) { return false; }
	M.Vertices=Mesh.VertexCount();M.Triangles=Mesh.TriangleCount();
	M.PayloadBytes=static_cast<int64>(M.Vertices)*sizeof(FVector3d)+static_cast<int64>(M.Triangles)*(sizeof(FIndex3i)+sizeof(int32)+3*(sizeof(FVector3f)+sizeof(FVector4f))+2*sizeof(FIndex3i));
	M.MeshMilliseconds=(FPlatformTime::Seconds()-Start)*1000;Out=MoveTemp(Mesh);Metrics=M;Error.Reset();return true;
}
