#pragma once

#include "AIController.h"
#include "AI/SCLAIState.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"

#include "SCLAIController.generated.h"

class ASCLPlayerCharacter;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBehaviorTree;
class UBlackboardData;
class USCLAbilitySystemComponent;

// 敌人 AI 控制器：维护感知目标、黑板上下文和行为树的生命周期。
UCLASS()
class SOULCOMBATLAB_API ASCLAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASCLAIController();

	UFUNCTION(BlueprintPure, Category = "AI|Perception")
	ASCLPlayerCharacter* GetPerceivedTarget() const { return PerceivedTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "AI|Perception")
	bool HasPerceivedTarget() const { return PerceivedTarget.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "AI|Perception")
	float GetConfiguredSightRadius() const;

	UFUNCTION(BlueprintPure, Category = "AI|Perception")
	float GetConfiguredLoseSightRadius() const;

	UFUNCTION(BlueprintPure, Category = "AI|Perception")
	float GetConfiguredPeripheralVisionHalfAngle() const;

	UFUNCTION(BlueprintPure, Category = "AI|Perception")
	float GetConfiguredSightMaxAge() const;

	UFUNCTION(BlueprintPure, Category = "AI|Perception")
	float GetConfiguredSightLossGracePeriod() const;

	UFUNCTION(BlueprintPure, Category = "AI|Behavior Tree")
	UBehaviorTree* GetRuntimeBehaviorTree() const { return RuntimeBehaviorTree; }

	UFUNCTION(BlueprintPure, Category = "AI|Behavior Tree")
	UBlackboardData* GetRuntimeBlackboardData() const;

	UFUNCTION(BlueprintPure, Category = "AI|Behavior Tree")
	ESCLEnemyAIState GetCurrentCombatState() const;

	// 感知/受击更新目标后，把目标、距离与状态写给行为树共用的黑板。
	void RefreshBlackboardContext();
	void NotifyDamageReceived(ASCLPlayerCharacter& DamageInstigator);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void AcquireTarget(ASCLPlayerCharacter& NewTarget);
	void RegisterTarget(
		ASCLPlayerCharacter& NewTarget,
		bool bConfirmedVisible,
		FName AwarenessSource);
	// 短暂丢失视线时延后清除目标，避免遮挡一帧就反复切换状态。
	void BeginSightLossGrace();
	void CancelSightLossGrace();
	void HandleSightLossGraceExpired();
	void ClearTarget(FName Reason);
	void TryAcquireReplacementTarget();
	bool ShouldRetainUnseenTarget(const ASCLPlayerCharacter& Target) const;
	bool IsValidPerceptionTarget(const ASCLPlayerCharacter* Candidate) const;
	void HandleTargetDeadTagChanged(FGameplayTag Tag, int32 NewCount);
	void InitializeBehaviorTree();
	ESCLEnemyAIState ResolveCombatState(
		float DistanceToTarget,
		ESCLEnemyAIState PreviousState) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTree> RuntimeBehaviorTree;

	TWeakObjectPtr<ASCLPlayerCharacter> PerceivedTarget;
	TWeakObjectPtr<USCLAbilitySystemComponent> PerceivedTargetAbilitySystem;
	FDelegateHandle TargetDeadTagDelegateHandle;
	FTimerHandle SightLossGraceTimerHandle;
	bool bTargetCurrentlyVisible{false};
};
