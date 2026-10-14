// Copyright Epic Games, Inc. All Rights Reserved.

#include "MontevideoGameGameMode.h"
#include "MontevideoGameCharacter.h"
#include "MontevideoGamePlayerController.h"
#include "UObject/ConstructorHelpers.h"

AMontevideoGameGameMode::AMontevideoGameGameMode()
{
	// These existing subclasses supply mesh, animation, input assets and touch UI.
	// Preserve their paths and the TP_ThirdPerson redirects while assets reference them.
	static ConstructorHelpers::FClassFinder<AMontevideoGameCharacter> PlayerPawn(
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	static ConstructorHelpers::FClassFinder<AMontevideoGamePlayerController> PlayerController(
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"));

	if (ensureMsgf(PlayerPawn.Succeeded(), TEXT("Third Person player Blueprint is missing.")))
	{
		DefaultPawnClass = PlayerPawn.Class;
	}
	if (ensureMsgf(PlayerController.Succeeded(), TEXT("Third Person controller Blueprint is missing.")))
	{
		PlayerControllerClass = PlayerController.Class;
	}
}
