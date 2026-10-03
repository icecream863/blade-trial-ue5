#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"

#include "SCLAbilityTask_VisualRoll.generated.h"

class USkeletalMeshComponent;

// 旧版人工翻滚任务：保留旧类型；当前八向闪避不使用，姿势由原版动画提供。
UCLASS()
class SOULCOMBATLAB_API USCLAbilityTask_VisualRoll : public UAbilityTask
{
	GENERATED_BODY()

public:
	USCLAbilityTask_VisualRoll();

	static USCLAbilityTask_VisualRoll* CreateVisualRoll(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		USkeletalMeshComponent* MeshComponent,
		float Duration);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

protected:
	virtual void OnDestroy(bool bAbilityEnded) override;

private:
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	FTransform InitialRelativeTransform{FTransform::Identity};
	float RollDuration{0.0F};
	float ElapsedTime{0.0F};
	bool bInitialTransformCaptured{false};
};
