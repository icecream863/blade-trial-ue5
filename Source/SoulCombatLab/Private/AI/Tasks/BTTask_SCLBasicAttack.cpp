#include "AI/Tasks/BTTask_SCLBasicAttack.h"

#include "AI/SCLAIState.h"
#include "AIController.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/SCLBossCharacter.h"
#include "Characters/SCLEnemyCharacter.h"
#include "Combat/SCLCombatComponent.h"
#include "Engine/World.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "SoulCombatLab.h"
#include "TimerManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

UBTTask_SCLBasicAttack::UBTTask_SCLBasicAttack()
{
	NodeName = TEXT("Basic Attack Loop");
	BlackboardKey.SelectedKeyName = SCLBlackboardKeys::TargetActor;
	bCreateNodeInstance = true;
	bIgnoreRestartSelf = true;
	INIT_TASK_NODE_NOTIFY_FLAGS();
}

EBTNodeResult::Type UBTTask_SCLBasicAttack::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* const NodeMemory)
{
	CleanupTask(false);
	AAIController* const Controller = OwnerComp.GetAIOwner();
	ASCLEnemyCharacter* const Enemy = Controller != nullptr
		? Cast<ASCLEnemyCharacter>(Controller->GetPawn())
		: nullptr;
	USCLAbilitySystemComponent* const AbilitySystem = Enemy != nullptr
		? Enemy->GetSCLAbilitySystemComponent()
		: nullptr;
	if (Enemy == nullptr || AbilitySystem == nullptr)
	{
		return EBTNodeResult::Failed;
	}

	ActiveOwnerComponent = &OwnerComp;
	ActiveAbilitySystem = AbilitySystem;
	ActiveAttacksPerBurst = FMath::Max(Enemy->GetAttacksPerBurst(), 1);
	ActiveRecoveryDuration = FMath::Max(Enemy->GetAttackRecoveryDuration(), 0.0F);
	AttackingTagDelegateHandle = AbilitySystem->RegisterGameplayTagEvent(
		SCLGameplayTags::State_Attacking,
		EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&UBTTask_SCLBasicAttack::HandleAttackingTagChanged);

	TryStartAttack();
	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_SCLBasicAttack::AbortTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* const NodeMemory)
{
	CleanupTask(true);
	return EBTNodeResult::Aborted;
}

void UBTTask_SCLBasicAttack::OnTaskFinished(
	UBehaviorTreeComponent& OwnerComp,
	uint8* const NodeMemory,
	const EBTNodeResult::Type TaskResult)
{
	CleanupTask(TaskResult != EBTNodeResult::Succeeded);
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_SCLBasicAttack::TryStartAttack()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SCL_AIAttackDecision);
	UBehaviorTreeComponent* const OwnerComp = ActiveOwnerComponent.Get();
	USCLAbilitySystemComponent* const AbilitySystem = ActiveAbilitySystem.Get();
	AAIController* const Controller = OwnerComp != nullptr ? OwnerComp->GetAIOwner() : nullptr;
	ASCLEnemyCharacter* const Enemy = Controller != nullptr
		? Cast<ASCLEnemyCharacter>(Controller->GetPawn())
		: nullptr;
	AActor* const Target = OwnerComp != nullptr && OwnerComp->GetBlackboardComponent() != nullptr
		? Cast<AActor>(OwnerComp->GetBlackboardComponent()->GetValueAsObject(BlackboardKey.SelectedKeyName))
		: nullptr;
	if (OwnerComp == nullptr || AbilitySystem == nullptr || Enemy == nullptr || !IsValid(Target))
	{
		if (OwnerComp != nullptr)
		{
			CleanupTask(true);
			FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		}
		return;
	}

	FVector DirectionToTarget = Target->GetActorLocation() - Enemy->GetActorLocation();
	DirectionToTarget.Z = 0.0F;
	// Recheck the live distance, not only the service's cached Combat state.
	const float AttackReach = Enemy->IsA<ASCLBossCharacter>() ? 350.0F : 160.0F;
	if (DirectionToTarget.Size2D() > AttackReach || !Controller->LineOfSightTo(Target))
	{
		// Do not add our capsule radius to a stop distance already chosen for melee reach.
		Controller->MoveToActor(Target, 100.0F, false);
		ScheduleNextAttempt(ActivationRetryDelay);
		return;
	}
	if (!DirectionToTarget.IsNearlyZero())
	{
		Controller->StopMovement();
		const FRotator FacingRotation = DirectionToTarget.Rotation();
		Enemy->SetActorRotation(FRotator{0.0F, FacingRotation.Yaw, 0.0F});
		Controller->SetControlRotation(FRotator{0.0F, FacingRotation.Yaw, 0.0F});
	}

	ASCLBossCharacter* const Boss = Cast<ASCLBossCharacter>(Enemy);
	if (Boss != nullptr)
	{
		Boss->SelectNextExecutableAttack(DirectionToTarget.Size2D());
		if (!Boss->PrepareCurrentAttack())
		{
			UE_LOG(
				LogSoulCombatLab,
				Warning,
				TEXT("Boss attack preparation failed: Boss=%s Attack=%d"),
				*GetNameSafe(Boss),
				static_cast<int32>(Boss->GetCurrentAttack()));
			ScheduleNextAttempt(ActivationRetryDelay);
			return;
		}
	}

	FGameplayTagContainer AttackAbilityTags;
	if (USCLCombatComponent* const Combat = Enemy->GetCombatComponent_Implementation()) Combat->SetAttackTarget(Target);
	AttackAbilityTags.AddTag(SCLGameplayTags::Ability_Attack_Light);
	const bool bActivated = AbilitySystem->TryActivateAbilitiesByTag(AttackAbilityTags);
	bAttackActive = bActivated &&
		AbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Attacking);
	if (bAttackActive)
	{
		++AttacksStarted;
		if (Boss != nullptr)
		{
			ActiveRecoveryDuration = FMath::Max(Boss->GetCurrentAttackRecoveryDuration(), 0.0F);
			Boss->StartCurrentAttackMovement(Target);
		}
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("AI basic attack activated: Controller=%s Pawn=%s Target=%s Burst=%d/%d Recovery=%.2f"),
			*GetNameSafe(Controller),
			*GetNameSafe(Enemy),
			*GetNameSafe(Target),
			AttacksStarted,
			ActiveAttacksPerBurst,
			ActiveRecoveryDuration);
		return;
	}

	UE_LOG(
		LogSoulCombatLab,
		Verbose,
		TEXT("AI basic attack unavailable; retry scheduled: Controller=%s Pawn=%s Delay=%.2f"),
		*GetNameSafe(Controller),
		*GetNameSafe(Enemy),
		ActivationRetryDelay);
	ScheduleNextAttempt(ActivationRetryDelay);
}

void UBTTask_SCLBasicAttack::HandleAttackingTagChanged(
	const FGameplayTag Tag,
	const int32 NewCount)
{
	if (NewCount > 0)
	{
		bAttackActive = true;
		return;
	}
	if (!bAttackActive)
	{
		return;
	}

	bAttackActive = false;
	if (UBehaviorTreeComponent* const OwnerComp = ActiveOwnerComponent.Get())
	{
		if (AAIController* const Controller = OwnerComp->GetAIOwner())
		{
			if (ASCLEnemyCharacter* const Enemy = Cast<ASCLEnemyCharacter>(Controller->GetPawn()))
			{
				if (USCLCombatComponent* const CombatComponent = Enemy->GetCombatComponent_Implementation())
				{
					CombatComponent->CancelActiveAttack();
				}
			}
		}
	}
	ScheduleNextAttempt(ActiveRecoveryDuration);
}

void UBTTask_SCLBasicAttack::ScheduleNextAttempt(const float DelaySeconds)
{
	UBehaviorTreeComponent* const OwnerComp = ActiveOwnerComponent.Get();
	UWorld* const World = OwnerComp != nullptr ? OwnerComp->GetWorld() : nullptr;
	if (World == nullptr)
	{
		if (OwnerComp != nullptr)
		{
			CleanupTask(true);
			FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		}
		return;
	}

	World->GetTimerManager().SetTimer(
		AttemptTimerHandle,
		this,
		&UBTTask_SCLBasicAttack::HandleAttemptTimerElapsed,
		FMath::Max(DelaySeconds, 0.01F),
		false);
}

void UBTTask_SCLBasicAttack::HandleAttemptTimerElapsed()
{
	AttemptTimerHandle.Invalidate();
	if (AttacksStarted >= ActiveAttacksPerBurst)
	{
		if (UBehaviorTreeComponent* const OwnerComp = ActiveOwnerComponent.Get())
		{
			UE_LOG(
				LogSoulCombatLab,
				Verbose,
				TEXT("AI basic attack burst completed: Controller=%s Attacks=%d"),
				*GetNameSafe(OwnerComp->GetAIOwner()),
				AttacksStarted);
			FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
		}
		return;
	}
	TryStartAttack();
}

void UBTTask_SCLBasicAttack::CleanupTask(const bool bCancelActiveAttack)
{
	if (UBehaviorTreeComponent* const OwnerComp = ActiveOwnerComponent.Get())
	{
		if (UWorld* const World = OwnerComp->GetWorld())
		{
			World->GetTimerManager().ClearTimer(AttemptTimerHandle);
		}
		if (bCancelActiveAttack)
		{
			if (AAIController* const Controller = OwnerComp->GetAIOwner())
			{
				if (ASCLEnemyCharacter* const Enemy = Cast<ASCLEnemyCharacter>(Controller->GetPawn()))
				{
					if (USCLCombatComponent* const CombatComponent = Enemy->GetCombatComponent_Implementation())
					{
						CombatComponent->CancelActiveAttack();
					}
				}
			}
		}
	}

	if (USCLAbilitySystemComponent* const AbilitySystem = ActiveAbilitySystem.Get())
	{
		if (AttackingTagDelegateHandle.IsValid())
		{
			AbilitySystem->RegisterGameplayTagEvent(
				SCLGameplayTags::State_Attacking,
				EGameplayTagEventType::NewOrRemoved).Remove(AttackingTagDelegateHandle);
		}
		if (bCancelActiveAttack)
		{
			FGameplayTagContainer AttackingTags;
			AttackingTags.AddTag(SCLGameplayTags::State_Attacking);
			AbilitySystem->RemoveActiveEffectsWithGrantedTags(AttackingTags);
		}
	}

	AttackingTagDelegateHandle.Reset();
	AttemptTimerHandle.Invalidate();
	AttacksStarted = 0;
	ActiveAttacksPerBurst = FMath::Max(AttacksPerBurst, 1);
	ActiveRecoveryDuration = RecoveryDuration;
	bAttackActive = false;
	ActiveAbilitySystem.Reset();
	ActiveOwnerComponent.Reset();
}
