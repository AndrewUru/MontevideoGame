// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MontevideoGameGameMode.generated.h"

/** Main session composition. Keeps the existing Third Person Blueprint presentation classes. */
UCLASS()
class MONTEVIDEOGAME_API AMontevideoGameGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AMontevideoGameGameMode();
};



