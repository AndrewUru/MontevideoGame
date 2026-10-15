#pragma once
#include "CoreMinimal.h"

class UMaterialInterface;
class UStaticMesh;

/** One transient, colored, unit prism. Local x=[0,1], y=[-1,0], z=[0,1]. */
class MONTEVIDEOGAME_API FFacadeModuleLibrary
{
public:
	static UStaticMesh* CreateUnitPrism(UObject* Owner,UMaterialInterface* Material,const FVector4f& Color,FString& Error);
};
