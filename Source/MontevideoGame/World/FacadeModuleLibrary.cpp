#include "World/FacadeModuleLibrary.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#include "Materials/MaterialInterface.h"
#include "DynamicMesh/DynamicMesh3.h"

UStaticMesh* FFacadeModuleLibrary::CreateUnitPrism(UObject* Owner,UMaterialInterface* Material,const FVector4f& Color,FString& Error)
{
	Error=TEXT("Unable to build transient facade module");
	if(!Owner || !Material || !FMath::IsFinite(Color.X) || !FMath::IsFinite(Color.Y) || !FMath::IsFinite(Color.Z) || !FMath::IsFinite(Color.W)) { return nullptr; }
	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);Attributes.Register();
	FMeshDescriptionBuilder Builder;Builder.SetMeshDescription(&Description);Builder.SetNumUVLayers(1);
	const FPolygonGroupID Group=Builder.AppendPolygonGroup();
	const FVector Points[8]={{0,-1,0},{1,-1,0},{0,0,0},{1,0,0},{0,-1,1},{1,-1,1},{0,0,1},{1,0,1}};
	FVertexID Vertices[8];for(int32 I=0;I<8;++I) { Vertices[I]=Builder.AppendVertex(Points[I]); }
	const int32 Faces[6][4]={{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
	const FVector Center(.5,-.5,.5);
	for(const auto& Face:Faces) for(int32 T=0;T<2;++T)
	{
		int32 IA=Face[0],IB=Face[T+1],IC=Face[T+2];
		FVector N=UE::Geometry::VectorUtil::Normal(Points[IA],Points[IB],Points[IC]);
		if(FVector::DotProduct(N,Points[IA]-Center)<0) { Swap(IB,IC);N=-N; }
		if(N.SizeSquared()<.5) { return nullptr; }
		const FVector Ref=FMath::Abs(N.Z)>.5?FVector::ForwardVector:FVector::UpVector;
		const FVector Tangent=(Ref-N*FVector::DotProduct(Ref,N)).GetSafeNormal();
		const float Shade=.80f+.12f*static_cast<float>(N.Z); // Neutral side shade: orientation changes per instance.
		const FVector4f FaceColor(Color.X*Shade,Color.Y*Shade,Color.Z*Shade,1);
		const int32 Indices[3]={IA,IB,IC};FVertexInstanceID Instances[3];
		for(int32 K=0;K<3;++K)
		{
			Instances[K]=Builder.AppendInstance(Vertices[Indices[K]]);
			Builder.SetInstanceNormal(Instances[K],N);
			Builder.SetInstanceTangentSpace(Instances[K],N,Tangent,1);
			Builder.SetInstanceColor(Instances[K],FaceColor);
			Builder.SetInstanceUV(Instances[K],FVector2D(Points[Indices[K]].X,Points[Indices[K]].Z));
		}
		Builder.AppendTriangle(Instances[0],Instances[1],Instances[2],Group);
	}
	UStaticMesh* Mesh=NewObject<UStaticMesh>(Owner,NAME_None,RF_Transient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(Material));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bFastBuild=true;Params.bCommitMeshDescription=false;Params.bMarkPackageDirty=false;Params.bBuildSimpleCollision=false;Params.bAllowCpuAccess=false;
	TArray<const FMeshDescription*> Descriptions;Descriptions.Add(&Description);
	if(!Mesh->BuildFromMeshDescriptions(Descriptions,Params)) { return nullptr; }
	Error.Reset();return Mesh;
}
