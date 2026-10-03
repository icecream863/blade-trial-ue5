#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SCLPrepareDemoMapCommandlet.generated.h"

// 显式运行的地图准备命令，不参与普通游戏启动。
UCLASS()
class USCLPrepareDemoMapCommandlet final : public UCommandlet
{
	GENERATED_BODY()
public:
	USCLPrepareDemoMapCommandlet();
	virtual int32 Main(const FString& Params) override;
};
