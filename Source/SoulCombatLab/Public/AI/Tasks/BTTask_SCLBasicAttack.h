#pragma once

#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "BTTask_SCLBasicAttack.generated.h"

class UBehaviorTreeComponent;
class USCLAbilitySystemComponent;

// 行为树攻击任务：向 GAS 请求一次基础攻击。
UCLASS()
class SOULCOMBATLAB_API UBTTask_SCLBasicAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_SCLBasicAttack();

	float GetRecoveryDuration() const { return RecoveryDuration; }
	float GetActivationRetryDelay() const { return ActivationRetryDelay; }
	int32 GetAttacksPerBurst() const { return AttacksPerBurst; }

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	virtual void OnTaskFinished(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		EBTNodeResult::Type TaskResult) override;

private:
	void TryStartAttack();
	void HandleAttackingTagChanged(FGameplayTag Tag, int32 NewCount);
	void ScheduleNextAttempt(float DelaySeconds);
	void HandleAttemptTimerElapsed();
	void CleanupTask(bool bCancelActiveAttack);

	UPROPERTY(EditDefaultsOnly, Category = "AI|Attack", meta = (ClampMin = "0.0", Units = "s"))
	float RecoveryDuration{0.75F};

	UPROPERTY(EditDefaultsOnly, Category = "AI|Attack", meta = (ClampMin = "0.1", Units = "s"))
	float ActivationRetryDelay{0.50F};

	UPROPERTY(EditDefaultsOnly, Category = "AI|Attack", meta = (ClampMin = "1"))
	int32 AttacksPerBurst{2};

	TWeakObjectPtr<UBehaviorTreeComponent> ActiveOwnerComponent;
	TWeakObjectPtr<USCLAbilitySystemComponent> ActiveAbilitySystem;
	FDelegateHandle AttackingTagDelegateHandle;
	FTimerHandle AttemptTimerHandle;
	int32 AttacksStarted{0};
	int32 ActiveAttacksPerBurst{2};
	float ActiveRecoveryDuration{0.75F};
	bool bAttackActive{false};
};
