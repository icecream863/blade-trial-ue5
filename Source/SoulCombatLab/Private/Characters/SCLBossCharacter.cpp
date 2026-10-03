#include "Characters/SCLBossCharacter.h"
#include "GameFramework/RootMotionSource.h"

#include "AI/Boss/SCLBossUtilityPolicy.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Combat/SCLCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/SCLEnemyArchetypeData.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "SoulCombatLab.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ASCLBossCharacter::ASCLBossCharacter()
{
	ArchetypeDataClass = USCLBossArchetypeData::StaticClass();
	bShowOverheadHealthBar = false;
	AreaTelegraph = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AreaTelegraph"));
	AreaTelegraph->SetupAttachment(GetRootComponent());
	AreaTelegraph->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AreaTelegraph->SetGenerateOverlapEvents(false);
	AreaTelegraph->SetCastShadow(false);
	AreaTelegraph->SetHiddenInGame(true);
	AreaTelegraph->SetRelativeLocation(FVector{0.0F, 0.0F, -88.0F});
	AreaTelegraph->SetRelativeScale3D(FVector{1.0F, 1.0F, 0.03F});

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TelegraphMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (TelegraphMesh.Succeeded())
	{
		AreaTelegraph->SetStaticMesh(TelegraphMesh.Object);
	}
}

void ASCLBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	if (AbilitySystem == nullptr)
	{
		return;
	}

	HealthChangedDelegateHandle = AbilitySystem
		->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ASCLBossCharacter::HandleHealthChanged);
	AttackingTagDelegateHandle = AbilitySystem->RegisterGameplayTagEvent(
		SCLGameplayTags::State_Attacking,
		EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&ASCLBossCharacter::HandleAttackingTagChanged);
	const float CurrentHealth = AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
	RefreshBossPhase(CurrentHealth);
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Boss initialized: Boss=%s Phase=%s Health=%.0f/%.0f"),
		*GetNameSafe(this),
		SCLBossUtilityPolicy::GetPhaseName(BossPhase),
		CurrentHealth,
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetMaxHealthAttribute()));
}

void ASCLBossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDashMovement();
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DashStopTimerHandle);
		World->GetTimerManager().ClearTimer(AreaTelegraphTimerHandle);
	}
	HideAreaTelegraph();

	USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	if (AbilitySystem != nullptr && HealthChangedDelegateHandle.IsValid())
	{
		AbilitySystem
			->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
		HealthChangedDelegateHandle.Reset();
	}
	if (AbilitySystem != nullptr && AttackingTagDelegateHandle.IsValid())
	{
		AbilitySystem->RegisterGameplayTagEvent(
			SCLGameplayTags::State_Attacking,
			EGameplayTagEventType::NewOrRemoved).Remove(AttackingTagDelegateHandle);
		AttackingTagDelegateHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

FSCLBossAttackDecision ASCLBossCharacter::SelectNextAttack(const float DistanceToTarget)
{
	return SelectAttack(DistanceToTarget, false);
}

FSCLBossAttackDecision ASCLBossCharacter::SelectNextExecutableAttack(const float DistanceToTarget)
{
	return SelectAttack(DistanceToTarget, true);
}

bool ASCLBossCharacter::PrepareCurrentAttack()
{
	if (CurrentAttack == ESCLBossAttack::None)
	{
		return false;
	}

	USCLCombatComponent* const Combat = GetCombatComponent_Implementation();
	if (Combat == nullptr)
	{
		return false;
	}

	CurrentExecutionProfile = SCLBossUtilityPolicy::ResolveExecutionProfile(CurrentAttack, BossPhase);
	FSCLRuntimeAttackProfile RuntimeProfile;
	RuntimeProfile.AttackName = FName{SCLBossUtilityPolicy::GetAttackName(CurrentAttack)};
	RuntimeProfile.DamageMultiplier = CurrentExecutionProfile.DamageMultiplier;
	RuntimeProfile.PoiseDamageMultiplier = CurrentExecutionProfile.PoiseDamageMultiplier;
	RuntimeProfile.MontagePlayRate = CurrentExecutionProfile.MontagePlayRate;
	RuntimeProfile.MontageStepIndex = CurrentExecutionProfile.MontageStepIndex;
	RuntimeProfile.MontageStepCount = CurrentExecutionProfile.MontageStepCount;
	RuntimeProfile.AttackStateDuration = CurrentExecutionProfile.AttackStateDuration;
	RuntimeProfile.AreaRadius = CurrentExecutionProfile.AreaRadius;
	RuntimeProfile.AreaImpactDelay = CurrentExecutionProfile.AreaImpactDelay;
	RuntimeProfile.bParryable = CurrentExecutionProfile.bParryable;
	RuntimeProfile.bWeaponTraceEnabled = CurrentExecutionProfile.bWeaponTraceEnabled;
	Combat->ConfigureNextAttackProfile(RuntimeProfile);
	return true;
}

void ASCLBossCharacter::StartCurrentAttackMovement(AActor* const TargetActor)
{
	UWorld* const World = GetWorld();
	if (CurrentExecutionProfile.AreaRadius > UE_SMALL_NUMBER && AreaTelegraph != nullptr)
	{
		constexpr float TelegraphMeshRadiusCentimeters{50.0F};
		const float RadiusScale = CurrentExecutionProfile.AreaRadius / TelegraphMeshRadiusCentimeters;
		AreaTelegraph->SetRelativeScale3D(FVector{RadiusScale, RadiusScale, 0.03F});
		AreaTelegraph->SetHiddenInGame(false);
		if (World != nullptr)
		{
			World->GetTimerManager().SetTimer(
				AreaTelegraphTimerHandle,
				this,
				&ASCLBossCharacter::HideAreaTelegraph,
				FMath::Max(CurrentExecutionProfile.AreaImpactDelay, 0.01F),
				false);
		}
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("Boss area telegraph shown: Boss=%s Radius=%.0f ImpactDelay=%.2f"),
			*GetNameSafe(this),
			CurrentExecutionProfile.AreaRadius,
			CurrentExecutionProfile.AreaImpactDelay);
	}

	if (!IsValid(TargetActor) || CurrentExecutionProfile.DashSpeed <= UE_SMALL_NUMBER ||
		CurrentExecutionProfile.DashDuration <= UE_SMALL_NUMBER)
	{
		return;
	}

	FVector DashDirection = TargetActor->GetActorLocation() - GetActorLocation();
	DashDirection.Z = 0.0F;
	if (!DashDirection.Normalize())
	{
		return;
	}

	UCharacterMovementComponent* const Movement = GetCharacterMovement();
	if (World == nullptr || Movement == nullptr)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(DashStopTimerHandle);
	const float Distance = FVector::Dist2D(TargetActor->GetActorLocation(), GetActorLocation());
	const float DashDistance = FMath::Clamp(Distance - 155.0F, 0.0F,
		CurrentExecutionProfile.DashSpeed * CurrentExecutionProfile.DashDuration);
	if (DashDistance <= UE_SMALL_NUMBER) return;
	// LaunchCharacter loses most of its horizontal travel when it lands and brakes.
	// A bounded movement source owns the whole dash and still uses character collision.
	StopDashMovement();
	const TSharedPtr<FRootMotionSource_ConstantForce> Dash = MakeShared<FRootMotionSource_ConstantForce>();
	Dash->InstanceName = TEXT("SCLBossDash");
	Dash->Priority = 500;
	Dash->AccumulateMode = ERootMotionAccumulateMode::Override;
	Dash->Force = DashDirection * (DashDistance / CurrentExecutionProfile.DashDuration);
	Dash->Duration = CurrentExecutionProfile.DashDuration;
	Dash->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Dash->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	DashRootMotionId = Movement->ApplyRootMotionSource(Dash);
	World->GetTimerManager().SetTimer(
		DashStopTimerHandle,
		this,
		&ASCLBossCharacter::StopDashMovement,
		CurrentExecutionProfile.DashDuration,
		false);
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Boss dash started: Boss=%s Target=%s Speed=%.0f Duration=%.2f"),
		*GetNameSafe(this),
		*GetNameSafe(TargetActor),
		DashDistance / CurrentExecutionProfile.DashDuration,
		CurrentExecutionProfile.DashDuration);
}

float ASCLBossCharacter::GetCurrentAttackRecoveryDuration() const
{
	return CurrentExecutionProfile.Attack == CurrentAttack
		? CurrentExecutionProfile.RecoveryDuration
		: GetAttackRecoveryDuration();
}

float ASCLBossCharacter::GetCombatEnterDistance() const
{
	return 350.0F;
}

float ASCLBossCharacter::GetCombatExitDistance() const
{
	return 380.0F;
}

FSCLBossAttackDecision ASCLBossCharacter::SelectAttack(
	const float DistanceToTarget,
	const bool bExecutableOnly)
{
	FSCLBossUtilityContext Context;
	Context.DistanceToTarget = FMath::Max(DistanceToTarget, 0.0F);
	Context.Phase = BossPhase;
	Context.PreviousAttack = CurrentAttack;
	Context.AttackBeforePrevious = AttackBeforePrevious;
	const FSCLBossAttackDecision Decision = bExecutableOnly
		? SCLBossUtilityPolicy::SelectExecutableAttack(Context)
		: SCLBossUtilityPolicy::SelectAttack(Context);
	AttackBeforePrevious = CurrentAttack;
	CurrentAttack = Decision.Attack;

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Boss utility selected: Boss=%s Phase=%s Distance=%.1f Previous=%s BeforePrevious=%s Attack=%s Score=%.1f ExecutableOnly=%s"),
		*GetNameSafe(this),
		SCLBossUtilityPolicy::GetPhaseName(BossPhase),
		Context.DistanceToTarget,
		SCLBossUtilityPolicy::GetAttackName(Context.PreviousAttack),
		SCLBossUtilityPolicy::GetAttackName(Context.AttackBeforePrevious),
		SCLBossUtilityPolicy::GetAttackName(Decision.Attack),
		Decision.Score,
		bExecutableOnly ? TEXT("true") : TEXT("false"));
	return Decision;
}

void ASCLBossCharacter::StopDashMovement()
{
	DashStopTimerHandle.Invalidate();
	if (UCharacterMovementComponent* const Movement = GetCharacterMovement())
	{
		if (DashRootMotionId != 0) Movement->RemoveRootMotionSourceByID(DashRootMotionId);
		DashRootMotionId = 0;
		Movement->StopMovementImmediately();
	}
}

void ASCLBossCharacter::HideAreaTelegraph()
{
	AreaTelegraphTimerHandle.Invalidate();
	if (AreaTelegraph != nullptr)
	{
		AreaTelegraph->SetHiddenInGame(true);
	}
}

void ASCLBossCharacter::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshBossPhase(ChangeData.NewValue);
}

void ASCLBossCharacter::HandleAttackingTagChanged(
	const FGameplayTag Tag,
	const int32 NewCount)
{
	if (NewCount > 0)
	{
		return;
	}

	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DashStopTimerHandle);
		World->GetTimerManager().ClearTimer(AreaTelegraphTimerHandle);
	}
	StopDashMovement();
	HideAreaTelegraph();
}

void ASCLBossCharacter::RefreshBossPhase(const float CurrentHealth)
{
	USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	if (AbilitySystem == nullptr)
	{
		return;
	}

	const float MaxHealth = AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetMaxHealthAttribute());
	const ESCLBossPhase NewPhase = SCLBossUtilityPolicy::ResolvePhase(CurrentHealth, MaxHealth);
	if (NewPhase == BossPhase)
	{
		return;
	}

	const ESCLBossPhase PreviousPhase = BossPhase;
	BossPhase = NewPhase;
	OnBossPhaseChanged.Broadcast(PreviousPhase, BossPhase);
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Boss phase changed: Boss=%s Previous=%s Current=%s Health=%.0f/%.0f Fraction=%.2f"),
		*GetNameSafe(this),
		SCLBossUtilityPolicy::GetPhaseName(PreviousPhase),
		SCLBossUtilityPolicy::GetPhaseName(BossPhase),
		CurrentHealth,
		MaxHealth,
		GetHealthFraction());
}

float ASCLBossCharacter::GetHealthFraction() const
{
	const USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	if (AbilitySystem == nullptr)
	{
		return 0.0F;
	}

	const float MaxHealth = AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetMaxHealthAttribute());
	return MaxHealth > UE_SMALL_NUMBER
		? FMath::Clamp(
			AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()) / MaxHealth,
			0.0F,
			1.0F)
		: 0.0F;
}
