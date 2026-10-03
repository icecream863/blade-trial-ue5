#pragma once

#include "AI/Boss/SCLBossTypes.h"
#include "Characters/SCLEnemyCharacter.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "SCLBossCharacter.generated.h"

struct FOnAttributeChangeData;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSCLBossPhaseChangedSignature,
	ESCLBossPhase,
	PreviousPhase,
	ESCLBossPhase,
	NewPhase);

// Boss 角色：在敌人基础上处理阶段、招式配置和 Boss 专用战斗状态。
UCLASS()
class SOULCOMBATLAB_API ASCLBossCharacter final : public ASCLEnemyCharacter
{
	GENERATED_BODY()

public:
	ASCLBossCharacter();

	UFUNCTION(BlueprintPure, Category = "Boss")
	ESCLBossPhase GetBossPhase() const { return BossPhase; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	ESCLBossAttack GetCurrentAttack() const { return CurrentAttack; }

	FSCLBossAttackDecision SelectNextAttack(float DistanceToTarget);
	FSCLBossAttackDecision SelectNextExecutableAttack(float DistanceToTarget);
	bool PrepareCurrentAttack();
	void StartCurrentAttackMovement(AActor* TargetActor);
	float GetCurrentAttackRecoveryDuration() const;
	virtual float GetCombatEnterDistance() const override;
	virtual float GetCombatExitDistance() const override;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Events")
	FSCLBossPhaseChangedSignature OnBossPhaseChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleAttackingTagChanged(FGameplayTag Tag, int32 NewCount);
	void RefreshBossPhase(float CurrentHealth);
	FSCLBossAttackDecision SelectAttack(float DistanceToTarget, bool bExecutableOnly);
	void StopDashMovement();
	void HideAreaTelegraph();
	float GetHealthFraction() const;

	UPROPERTY(VisibleAnywhere, Category = "Boss|Presentation")
	TObjectPtr<UStaticMeshComponent> AreaTelegraph;

	UPROPERTY(VisibleInstanceOnly, Category = "Boss")
	ESCLBossPhase BossPhase{ESCLBossPhase::PhaseOne};

	UPROPERTY(VisibleInstanceOnly, Category = "Boss")
	ESCLBossAttack CurrentAttack{ESCLBossAttack::None};

	UPROPERTY(VisibleInstanceOnly, Category = "Boss")
	ESCLBossAttack AttackBeforePrevious{ESCLBossAttack::None};

	UPROPERTY(VisibleInstanceOnly, Category = "Boss")
	FSCLBossAttackExecutionProfile CurrentExecutionProfile;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle AttackingTagDelegateHandle;
	FTimerHandle DashStopTimerHandle;
	uint16 DashRootMotionId{0};
	FTimerHandle AreaTelegraphTimerHandle;
};
