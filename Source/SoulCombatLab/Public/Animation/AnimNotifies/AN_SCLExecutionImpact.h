#pragma once
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_SCLExecutionImpact.generated.h"

/** 处决刀刃接触的单帧通知；实际伤害仍由处决 Ability 结算。 */
UCLASS()
class SOULCOMBATLAB_API UAN_SCLExecutionImpact : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& Reference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("SCL 处决命中"); }
};
