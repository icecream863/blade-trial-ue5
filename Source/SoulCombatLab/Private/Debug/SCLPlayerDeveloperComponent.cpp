#include "Debug/SCLPlayerDeveloperComponent.h"
#include "Characters/Player/SCLPlayerCharacter.h"

#include "AI/Boss/SCLBossUtilityPolicy.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Combat/SCLCombatComponent.h"
#include "Combat/SCLWeapon.h"
#include "Characters/SCLBossCharacter.h"
#include "Characters/SCLEnemyCharacter.h"
#include "Characters/SCLTrainingDummy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/SCLCombatDebugSubsystem.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "SoulCombatLab.h"
#include "Targeting/SCLTargetingComponent.h"
#include "TimerManager.h"
#include "UI/SCLHUD.h"
/**
 * 请求一次躲闪，在 0.35/0.80 秒采样伤害；以日志的实际伤害判断窗口内外表现。
 */
void USCLPlayerDeveloperComponent::DebugTestDodgeIFrame()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	Player->RequestDodge();
	FTimerDelegate InsideIFrameDamage = FTimerDelegate::CreateWeakLambda(this, [this, Player]()
	{
		const float AppliedDamage =
			UGameplayStatics::ApplyDamage(Player, 5.0F, Player->GetController(), Player, UDamageType::StaticClass());
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("Dodge IFrame test sample: Phase=Inside IncomingDamage=5.00 AppliedDamage=%.2f"),
			AppliedDamage);
	});
	FTimerHandle InsideIFrameHandle;
	World->GetTimerManager().SetTimer(InsideIFrameHandle, InsideIFrameDamage, 0.35F, false);

	FTimerDelegate AfterIFrameDamage = FTimerDelegate::CreateWeakLambda(this, [this, Player]()
	{
		const float AppliedDamage =
			UGameplayStatics::ApplyDamage(Player, 5.0F, Player->GetController(), Player, UDamageType::StaticClass());
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("Dodge IFrame test sample: Phase=After IncomingDamage=5.00 AppliedDamage=%.2f"),
			AppliedDamage);
	});
	FTimerHandle AfterIFrameHandle;
	World->GetTimerManager().SetTimer(AfterIFrameHandle, AfterIFrameDamage, 0.80F, false);
}

/**
 * 恢复生命/体力后进入格挡，依次安排正面、背面和连续伤害采样。
 * Phase 名称只是日志标签，实际格挡和破防结果要看输出属性与状态。
 */
void USCLPlayerDeveloperComponent::DebugTestBlock()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	UWorld* const World = GetWorld();
	USCLAbilitySystemComponent* const AbilitySystem = Player->GetSCLAbilitySystemComponent();
	if (World == nullptr || AbilitySystem == nullptr)
	{
		return;
	}

	Player->StopBlock();
	AbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetHealthAttribute(),
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetMaxHealthAttribute()));
	AbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetStaminaAttribute(),
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetMaxStaminaAttribute()));
	Player->StartBlock();

	const auto ScheduleSample = [this, Player, World](
		const float Delay,
		const FName Phase,
		const bool bFromFront)
	{
		FTimerDelegate DamageSample = FTimerDelegate::CreateWeakLambda(
			this,
			[this, Phase, bFromFront]()
			{
				ApplyDebugBlockDamage(Phase, bFromFront, 20.0F);
			});
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(TimerHandle, DamageSample, Delay, false);
	};

	ScheduleSample(0.10F, TEXT("FrontBlocked1"), true);
	ScheduleSample(0.25F, TEXT("RearBypass"), false);
	ScheduleSample(0.40F, TEXT("FrontBlocked2"), true);
	ScheduleSample(0.55F, TEXT("FrontBlocked3"), true);
	ScheduleSample(0.70F, TEXT("GuardBreak"), true);
	ScheduleSample(0.85F, TEXT("AfterGuardBreak"), true);
}

/**
 * 生成指定来向的点伤害并打印生命、体力、格挡与失衡状态；bFromFront 选择前后方向。
 */
void USCLPlayerDeveloperComponent::ApplyDebugBlockDamage(
	const FName Phase,
	const bool bFromFront,
	const float DamageAmount)
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	USCLAbilitySystemComponent* const AbilitySystem = Player->GetSCLAbilitySystemComponent();
	if (AbilitySystem == nullptr)
	{
		return;
	}

	const FVector ShotDirection = bFromFront ? -Player->GetActorForwardVector() : Player->GetActorForwardVector();
	const FPointDamageEvent PointDamageEvent{
		DamageAmount,
		FHitResult{},
		ShotDirection,
		UDamageType::StaticClass()};
	const float AppliedDamage = Player->TakeDamage(DamageAmount, PointDamageEvent, Player->GetController(), nullptr);
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Block test sample: Phase=%s AppliedDamage=%.2f Health=%.2f Stamina=%.2f Blocking=%s Staggered=%s"),
		*Phase.ToString(),
		AppliedDamage,
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()),
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()),
		AbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Blocking) ? TEXT("true") : TEXT("false"),
		AbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Staggered) ? TEXT("true") : TEXT("false"));
}

/**
 * 要求世界中已有训练靶，恢复生命、朝向伤害来源并请求弹反。
 * 在 0.16/0.40 秒用弱引用安排点伤害采样，比较窗口时序与攻击者失衡。
 */
void USCLPlayerDeveloperComponent::DebugTestParry()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	UWorld* const World = GetWorld();
	USCLAbilitySystemComponent* const AbilitySystem = Player->GetSCLAbilitySystemComponent();
	if (World == nullptr || AbilitySystem == nullptr)
	{
		return;
	}

	ASCLTrainingDummy* DamageSource = nullptr;
	for (TActorIterator<ASCLTrainingDummy> DummyIterator{World}; DummyIterator; ++DummyIterator)
	{
		DamageSource = *DummyIterator;
		break;
	}
	if (DamageSource == nullptr)
	{
		UE_LOG(LogSoulCombatLab, Warning, TEXT("Parry test requires an SCLTrainingDummy in the world."));
		return;
	}

	Player->StopBlock();
	AbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetHealthAttribute(),
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetMaxHealthAttribute()));
	const FVector DirectionToSource =
		(DamageSource->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
	Player->SetActorRotation(FRotator{0.0F, DirectionToSource.Rotation().Yaw, 0.0F});
	Player->RequestParry();

	const TWeakObjectPtr<AActor> WeakDamageSource{DamageSource};
	const auto ScheduleSample = [this, Player, World, WeakDamageSource](
		const float Delay,
		const FName Phase)
	{
		FTimerDelegate DamageSample = FTimerDelegate::CreateWeakLambda(
			this,
			[this, WeakDamageSource, Phase]()
			{
				ApplyDebugParryDamage(Phase, WeakDamageSource.Get(), 20.0F);
			});
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(TimerHandle, DamageSample, Delay, false);
	};

	ScheduleSample(0.16F, TEXT("InsideWindow"));
	ScheduleSample(0.40F, TEXT("AfterWindow"));
}

/**
 * 对玩家施加来自指定 Actor 的点伤害，输出实际伤害、弹反标签和攻击者失衡。
 */
void USCLPlayerDeveloperComponent::ApplyDebugParryDamage(
	const FName Phase,
	AActor* const DamageCauser,
	const float DamageAmount)
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	USCLAbilitySystemComponent* const AbilitySystem = Player->GetSCLAbilitySystemComponent();
	if (AbilitySystem == nullptr || DamageCauser == nullptr)
	{
		return;
	}

	const FVector ShotDirection =
		(Player->GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal();
	const FPointDamageEvent PointDamageEvent{
		DamageAmount,
		FHitResult{},
		ShotDirection,
		UDamageType::StaticClass()};
	const float AppliedDamage = Player->TakeDamage(
		DamageAmount,
		PointDamageEvent,
		Player->GetController(),
		DamageCauser);
	const UAbilitySystemComponent* const AttackerAbilitySystem =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(DamageCauser);
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Parry test sample: Phase=%s AppliedDamage=%.2f Health=%.2f Parrying=%s AttackerStaggered=%s"),
		*Phase.ToString(),
		AppliedDamage,
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()),
		AbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Parrying) ? TEXT("true") : TEXT("false"),
		AttackerAbilitySystem != nullptr &&
			AttackerAbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Staggered)
			? TEXT("true")
			: TEXT("false"));
}

/**
 * 准备训练靶的可处决状态与玩家摆位，立即请求处决，再延迟记录目标生命和死亡状态。
 */
void USCLPlayerDeveloperComponent::DebugTestExecution()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	UWorld* const World = GetWorld();
	ASCLTrainingDummy* const Target = PrepareDebugExecutionTarget();
	USCLAbilitySystemComponent* const TargetAbilitySystem = Target != nullptr
		? Target->GetSCLAbilitySystemComponent()
		: nullptr;
	if (World == nullptr || TargetAbilitySystem == nullptr)
	{
		return;
	}
	Player->RequestExecution();

	const TWeakObjectPtr<USCLAbilitySystemComponent> WeakTargetAbilitySystem{TargetAbilitySystem};
	FTimerDelegate ResultSample = FTimerDelegate::CreateWeakLambda(
		this,
		[WeakTargetAbilitySystem]()
		{
			const USCLAbilitySystemComponent* const TargetSystem = WeakTargetAbilitySystem.Get();
			if (TargetSystem == nullptr)
			{
				return;
			}
			UE_LOG(
				LogSoulCombatLab,
				Log,
				TEXT("Execution test result: Health=%.2f Executable=%s Dead=%s"),
				TargetSystem->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()),
				TargetSystem->HasMatchingGameplayTag(SCLGameplayTags::State_Executable)
					? TEXT("true")
					: TEXT("false"),
				TargetSystem->HasMatchingGameplayTag(SCLGameplayTags::State_Dead)
					? TEXT("true")
					: TEXT("false"));
		});
	FTimerHandle ResultTimerHandle;
	World->GetTimerManager().SetTimer(ResultTimerHandle, ResultSample, 0.65F, false);
}

/**
 * 只准备训练靶和摆位，不自动发起技能；之后可手动按 E 观察完整处决。
 */
void USCLPlayerDeveloperComponent::DebugPrepareExecution()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	if (ASCLTrainingDummy* const Target = PrepareDebugExecutionTarget())
	{
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("Execution test prepared: Target=%s Press=E"),
			*GetNameSafe(Target));
	}
}

/**
 * 准备多个候选目标与玩家朝向，供手动检查锁定和滚轮切换。
 */
void USCLPlayerDeveloperComponent::DebugPrepareLockOnSwitch()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (Player->GetTargetingComponent() != nullptr)
	{
		Player->GetTargetingComponent()->ClearTarget();
	}

	const FRotator ViewYaw{
		0.0F,
		Player->GetController() != nullptr ? Player->GetController()->GetControlRotation().Yaw : Player->GetActorRotation().Yaw,
		0.0F};
	const FVector ViewForward = ViewYaw.Vector();
	const FVector ViewRight = FRotationMatrix{ViewYaw}.GetUnitAxis(EAxis::Y);
	int32 ExistingTargetIndex{0};
	for (TActorIterator<ASCLTrainingDummy> DummyIterator{World}; DummyIterator; ++DummyIterator)
	{
		DummyIterator->SetActorLocation(
			Player->GetActorLocation() - ViewForward * 3000.0F + ViewRight * ExistingTargetIndex * 150.0F,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		++ExistingTargetIndex;
	}

	constexpr float TargetDistance{700.0F};
	const TArray<float> HorizontalOffsets{-350.0F, 0.0F, 350.0F};
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Player;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	int32 SpawnedCount{0};
	for (const float HorizontalOffset : HorizontalOffsets)
	{
		const FVector SpawnLocation = Player->GetActorLocation() + ViewForward * TargetDistance +
			ViewRight * HorizontalOffset;
		ASCLTrainingDummy* const SpawnedTarget = World->SpawnActor<ASCLTrainingDummy>(
			ASCLTrainingDummy::StaticClass(),
			SpawnLocation,
			(-ViewForward).Rotation(),
			SpawnParameters);
		SpawnedCount += SpawnedTarget != nullptr ? 1 : 0;
	}

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Lock-On switch test prepared: Targets=%d MiddleMouse=Lock WheelUp=Right WheelDown=Left"),
		SpawnedCount);
}

/**
 * 在玩家视角前方生成感知测试敌人，供检查目标发现与追击。
 */
void USCLPlayerDeveloperComponent::DebugSpawnPerceptionEnemy()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FRotator ViewYaw{
		0.0F,
		Player->GetController() != nullptr ? Player->GetController()->GetControlRotation().Yaw : Player->GetActorRotation().Yaw,
		0.0F};
	const FVector SpawnLocation = Player->GetActorLocation() + ViewYaw.Vector() * 600.0F;
	const FRotator SpawnRotation = (Player->GetActorLocation() - SpawnLocation).Rotation();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Player;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASCLEnemyCharacter* const SpawnedEnemy = World->SpawnActor<ASCLEnemyCharacter>(
		ASCLEnemyCharacter::StaticClass(),
		SpawnLocation,
		SpawnRotation,
		SpawnParameters);

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("AI perception test spawned: Enemy=%s Distance=%.1f FacingPlayer=%s"),
		*GetNameSafe(SpawnedEnemy),
		SpawnedEnemy != nullptr
			? FVector::Dist(SpawnedEnemy->GetActorLocation(), Player->GetActorLocation())
			: 0.0F,
		SpawnedEnemy != nullptr ? TEXT("true") : TEXT("false"));
}

/**
 * 在玩家视角前方生成重兵，便于比较其攻击与防御表现。
 */
void USCLPlayerDeveloperComponent::DebugSpawnHeavyEnemy()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FRotator ViewYaw{
		0.0F,
		Player->GetController() != nullptr ? Player->GetController()->GetControlRotation().Yaw : Player->GetActorRotation().Yaw,
		0.0F};
	const FVector SpawnLocation = Player->GetActorLocation() + ViewYaw.Vector() * 600.0F;
	const FRotator SpawnRotation = (Player->GetActorLocation() - SpawnLocation).Rotation();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Player;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASCLHeavyEnemyCharacter* const SpawnedEnemy = World->SpawnActor<ASCLHeavyEnemyCharacter>(
		ASCLHeavyEnemyCharacter::StaticClass(),
		SpawnLocation,
		SpawnRotation,
		SpawnParameters);

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Heavy enemy test spawned: Enemy=%s Distance=%.1f FacingPlayer=%s"),
		*GetNameSafe(SpawnedEnemy),
		SpawnedEnemy != nullptr
			? FVector::Dist(SpawnedEnemy->GetActorLocation(), Player->GetActorLocation())
			: 0.0F,
		SpawnedEnemy != nullptr ? TEXT("true") : TEXT("false"));
}

/**
 * 在玩家前方分开摆放剑兵和重兵，观察同一套 AI 下的配置差异。
 */
void USCLPlayerDeveloperComponent::DebugSpawnEnemyVariants()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FRotator ViewYaw{
		0.0F,
		Player->GetController() != nullptr ? Player->GetController()->GetControlRotation().Yaw : Player->GetActorRotation().Yaw,
		0.0F};
	const FVector ViewForward = ViewYaw.Vector();
	const FVector ViewRight = FRotationMatrix{ViewYaw}.GetUnitAxis(EAxis::Y);
	const FVector Center = Player->GetActorLocation() + ViewForward * 700.0F;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Player;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASCLSwordEnemyCharacter* const SwordEnemy = World->SpawnActor<ASCLSwordEnemyCharacter>(
		ASCLSwordEnemyCharacter::StaticClass(),
		Center - ViewRight * 260.0F,
		(Player->GetActorLocation() - Center).Rotation(),
		SpawnParameters);
	ASCLHeavyEnemyCharacter* const HeavyEnemy = World->SpawnActor<ASCLHeavyEnemyCharacter>(
		ASCLHeavyEnemyCharacter::StaticClass(),
		Center + ViewRight * 260.0F,
		(Player->GetActorLocation() - Center).Rotation(),
		SpawnParameters);

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Enemy variant test spawned: Sword=%s Heavy=%s"),
		*GetNameSafe(SwordEnemy),
		*GetNameSafe(HeavyEnemy));
}

/**
 * 按视角方向生成临时 Boss；DistanceToTarget 单位为厘米，最小限制为 150。
 * 弱引用只记录最近生成的 Boss，不等同于自动销毁先前的 Boss。
 */
void USCLPlayerDeveloperComponent::DebugSpawnBossPrototype(const float DistanceToTarget)
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FRotator ViewYaw{
		0.0F,
		Player->GetController() != nullptr ? Player->GetController()->GetControlRotation().Yaw : Player->GetActorRotation().Yaw,
		0.0F};
	const float SafeDistance = FMath::Max(DistanceToTarget, 150.0F);
	const FVector SpawnLocation = Player->GetActorLocation() + ViewYaw.Vector() * SafeDistance;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Player;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	DebugBossPrototype = World->SpawnActor<ASCLBossCharacter>(
		ASCLBossCharacter::StaticClass(),
		SpawnLocation,
		(Player->GetActorLocation() - SpawnLocation).Rotation(),
		SpawnParameters);

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Boss prototype test spawned: Boss=%s Distance=%.1f"),
		*GetNameSafe(DebugBossPrototype.Get()),
		DebugBossPrototype.IsValid()
			? FVector::Dist(DebugBossPrototype->GetActorLocation(), Player->GetActorLocation())
			: 0.0F);
}

/**
 * 把临时 Boss 当前生命设为最大生命的一半，利用已有属性监听驱动阶段变化。
 */
void USCLPlayerDeveloperComponent::DebugSetBossPhaseTwo()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	ASCLBossCharacter* const Boss = DebugBossPrototype.Get();
	USCLAbilitySystemComponent* const BossAbilitySystem =
		Boss != nullptr ? Boss->GetSCLAbilitySystemComponent() : nullptr;
	if (BossAbilitySystem == nullptr)
	{
		UE_LOG(LogSoulCombatLab, Warning, TEXT("Boss Phase Two test requires DebugSpawnBossPrototype first."));
		return;
	}

	const float MaxHealth = BossAbilitySystem->GetNumericAttribute(USCLAttributeSet::GetMaxHealthAttribute());
	BossAbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetHealthAttribute(),
		MaxHealth * 0.50F);
}

/**
 * 以给定距离调用临时 Boss 选招；该参数用于策略评估，不会移动玩家或 Boss。
 */
void USCLPlayerDeveloperComponent::DebugEvaluateBossUtility(const float DistanceToTarget)
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	ASCLBossCharacter* const Boss = DebugBossPrototype.Get();
	if (Boss == nullptr)
	{
		UE_LOG(LogSoulCombatLab, Warning, TEXT("Boss Utility test requires DebugSpawnBossPrototype first."));
		return;
	}

	Boss->SelectNextAttack(DistanceToTarget);
}

/**
 * 组合生成、距离选招和半血阶段操作，并打印选招结果；不代替真实挥刀验证。
 */
void USCLPlayerDeveloperComponent::DebugTestBossFoundation()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	DebugSpawnBossPrototype();
	if (!DebugBossPrototype.IsValid())
	{
		return;
	}

	DebugEvaluateBossUtility(150.0F);
	DebugEvaluateBossUtility(400.0F);
	DebugSetBossPhaseTwo();
	DebugEvaluateBossUtility(150.0F);
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Boss foundation test completed: Boss=%s Phase=%s CurrentAttack=%s"),
		*GetNameSafe(DebugBossPrototype.Get()),
		SCLBossUtilityPolicy::GetPhaseName(DebugBossPrototype->GetBossPhase()),
		SCLBossUtilityPolicy::GetAttackName(DebugBossPrototype->GetCurrentAttack()));
}

/**
 * 提高玩家生命/韧性以便观察，生成 Boss；可选半血开场，供手动战斗检查。
 */
void USCLPlayerDeveloperComponent::DebugPrepareBossCombatTest(const bool bStartInPhaseTwo)
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	USCLAbilitySystemComponent* const PlayerAbilitySystem = Player->GetSCLAbilitySystemComponent();
	if (PlayerAbilitySystem == nullptr)
	{
		return;
	}

	constexpr float DebugSurvivabilityValue{1000.0F};
	FGameplayTagContainer DeadTags;
	DeadTags.AddTag(SCLGameplayTags::State_Dead);
	PlayerAbilitySystem->RemoveActiveEffectsWithGrantedTags(DeadTags);
	PlayerAbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetMaxHealthAttribute(),
		DebugSurvivabilityValue);
	PlayerAbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetHealthAttribute(),
		DebugSurvivabilityValue);
	PlayerAbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetMaxPoiseAttribute(),
		DebugSurvivabilityValue);
	PlayerAbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetPoiseAttribute(),
		DebugSurvivabilityValue);

	DebugSpawnBossPrototype(600.0F);
	if (bStartInPhaseTwo)
	{
		DebugSetBossPhaseTwo();
	}

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Boss combat visual test prepared: Player=%s Health=%.0f Poise=%.0f StartPhase=%s Boss=%s"),
		*GetNameSafe(Player),
		DebugSurvivabilityValue,
		DebugSurvivabilityValue,
		bStartInPhaseTwo ? TEXT("PhaseTwo") : TEXT("PhaseOne"),
		*GetNameSafe(DebugBossPrototype.Get()));
}

/**
 * 对当前锁定目标施加等于剩余生命的伤害；仍经过伤害入口，不直接写死亡状态。
 */
void USCLPlayerDeveloperComponent::DebugKillLockedTarget()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	ASCLCharacterBase* const Target = Player->GetTargetingComponent() != nullptr
		? Player->GetTargetingComponent()->GetCurrentTarget()
		: nullptr;
	USCLAbilitySystemComponent* const TargetAbilitySystem = Target != nullptr
		? Target->GetSCLAbilitySystemComponent()
		: nullptr;
	if (Target == nullptr || TargetAbilitySystem == nullptr)
	{
		UE_LOG(LogSoulCombatLab, Warning, TEXT("Lock-On kill test requires a current target."));
		return;
	}

	const float RemainingHealth = TargetAbilitySystem->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
	UGameplayStatics::ApplyDamage(Target, RemainingHealth, Player->GetController(), Player, UDamageType::StaticClass());
}

/**
 * 查找已有训练靶、恢复其生命并施加可处决状态，把玩家放到靶后 150 cm。
 * 没有目标或 ASC 时返回 nullptr，调用者不能继续执行处决。
 */
ASCLTrainingDummy* USCLPlayerDeveloperComponent::PrepareDebugExecutionTarget()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return nullptr; }
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	ASCLTrainingDummy* Target = nullptr;
	for (TActorIterator<ASCLTrainingDummy> DummyIterator{World}; DummyIterator; ++DummyIterator)
	{
		Target = *DummyIterator;
		break;
	}
	USCLAbilitySystemComponent* const TargetAbilitySystem = Target != nullptr
		? Target->GetSCLAbilitySystemComponent()
		: nullptr;
	if (Target == nullptr || TargetAbilitySystem == nullptr)
	{
		UE_LOG(LogSoulCombatLab, Warning, TEXT("Execution test requires an SCLTrainingDummy in the world."));
		return nullptr;
	}

	TargetAbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetHealthAttribute(),
		TargetAbilitySystem->GetNumericAttribute(USCLAttributeSet::GetMaxHealthAttribute()));
	TargetAbilitySystem->ApplyExecutableState();
	const FVector TargetForward = Target->GetActorForwardVector().GetSafeNormal2D();
	if (UCharacterMovementComponent* const MovementComponent = Player->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
	Player->SetActorLocation(
		Target->GetActorLocation() - TargetForward * 150.0F,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	const FVector DirectionToTarget =
		(Target->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
	Player->SetActorRotation(FRotator{0.0F, DirectionToTarget.Rotation().Yaw, 0.0F});
	return Target;
}


/**
 * 切换当前 World 的战斗调试绘制开关，不影响其他 World。
 */
void USCLPlayerDeveloperComponent::DebugToggleCombatDraw()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	if (UWorld* const World = GetWorld())
	{
		if (USCLCombatDebugSubsystem* const Debug = World->GetSubsystem<USCLCombatDebugSubsystem>())
		{
			Debug->SetEnabled(!Debug->IsEnabled());
		}
	}
}

/**
 * 通过当前 Controller 的 HUD 切换开发面板；HUD 类型不匹配时不执行。
 */
void USCLPlayerDeveloperComponent::DebugToggleDeveloperHUD()
{
	ASCLPlayerCharacter* const Player = GetPlayer();
	if (!IsValid(Player)) { return; }
	const APlayerController* const PlayerController = Cast<APlayerController>(Player->GetController());
	ASCLHUD* const HUD = PlayerController != nullptr
		? Cast<ASCLHUD>(PlayerController->GetHUD())
		: nullptr;
	if (HUD != nullptr)
	{
		HUD->ToggleDeveloperDebugHUD();
	}
}


/**
 * 默认没有 Tick；实验仅在控制台命令触发后安排定时采样。
 */
USCLPlayerDeveloperComponent::USCLPlayerDeveloperComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/**
 * 从组件 Owner 取得玩家角色；开发方法都先核对这个返回值。
 */
ASCLPlayerCharacter* USCLPlayerDeveloperComponent::GetPlayer() const
{
	return Cast<ASCLPlayerCharacter>(GetOwner());
}

/**
 * 旧 Pawn 销毁时清理绑定到本组件的定时器，防止实验在重生后继续采样。
 */
void USCLPlayerDeveloperComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 重生销毁旧 Pawn 时，旧实验不能继续对角色或新场景施加定时伤害。
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}
