#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_SCLDrawWeapon.generated.h"

// 拔刀动画手握住刀的单次事件：只通知武器表现组件换挂点，不负责出招或扣费。
UCLASS()
class SOULCOMBATLAB_API UAN_SCLDrawWeapon : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("SCL 手握刀"); }
};
