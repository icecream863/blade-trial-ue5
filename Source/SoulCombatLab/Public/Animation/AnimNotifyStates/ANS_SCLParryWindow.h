#pragma once
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_SCLParryWindow.generated.h"

/** 弹反判定只在动画手臂真正挡住刀的时间段生效。 */
UCLASS(DisplayName="SCL Parry Window")
class SOULCOMBATLAB_API UANS_SCLParryWindow : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	virtual void NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& Reference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& Reference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("SCL 弹反有效窗口"); }
};
