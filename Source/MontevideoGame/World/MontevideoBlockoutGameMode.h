#pragma once

#include "MontevideoGameGameMode.h"
#include "MontevideoBlockoutGameMode.generated.h"

/** Dedicated development scene composition; preserves the Third Person gameplay classes. */
UCLASS()
class MONTEVIDEOGAME_API AMontevideoBlockoutGameMode : public AMontevideoGameGameMode
{
	GENERATED_BODY()
public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
private:
	UPROPERTY() TObjectPtr<AActor> DevelopmentStart;
};
