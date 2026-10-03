#include "Characters/SCLCharacterBase.h"
#include "Characters/Components/SCLActionMovementComponent.h"
#include "Characters/SCLEnemyCharacter.h"

#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/SCLParryAbility.h"
#include "Characters/Components/SCLHitReactionComponent.h"
#include "Combat/SCLCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "SoulCombatLab.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FVector GuardVisualBaseScale{0.04F, 0.55F, 0.75F};
const FVector GuardVisualImpactScale{0.07F, 0.70F, 0.90F};
constexpr float GuardImpactDurationSeconds{0.12F};
}

ASCLCharacterBase::ASCLCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	AbilitySystemComponent = CreateDefaultSubobject<USCLAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilityAttributes = CreateDefaultSubobject<USCLAttributeSet>(TEXT("AbilityAttributes"));
	ActionMovementComponent = CreateDefaultSubobject<USCLActionMovementComponent>(TEXT("ActionMovementComponent"));
	CombatComponent = CreateDefaultSubobject<USCLCombatComponent>(TEXT("CombatComponent"));
	HitReactionComponent = CreateDefaultSubobject<USCLHitReactionComponent>(TEXT("HitReactionComponent"));
	GuardVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GuardVisual"));
	GuardVisual->SetupAttachment(RootComponent);
	GuardVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GuardVisual->SetGenerateOverlapEvents(false);
	GuardVisual->SetCastShadow(false);
	GuardVisual->SetRelativeLocation(FVector{48.0F, 0.0F, 20.0F});
	GuardVisual->SetRelativeScale3D(GuardVisualBaseScale);
	GuardVisual->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> GuardVisualMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (GuardVisualMesh.Succeeded())
	{
		GuardVisual->SetStaticMesh(GuardVisualMesh.Object);
	}

	ExecutionMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExecutionMarker"));
	ExecutionMarker->SetupAttachment(RootComponent);
	ExecutionMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ExecutionMarker->SetGenerateOverlapEvents(false);
	ExecutionMarker->SetCastShadow(false);
	ExecutionMarker->SetRelativeLocation(FVector{0.0F, 0.0F, 135.0F});
	ExecutionMarker->SetRelativeScale3D(FVector{0.18F});
	ExecutionMarker->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ExecutionMarkerMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (ExecutionMarkerMesh.Succeeded())
	{
		ExecutionMarker->SetStaticMesh(ExecutionMarkerMesh.Object);
	}

	LockOnMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LockOnMarker"));
	LockOnMarker->SetupAttachment(RootComponent);
	LockOnMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LockOnMarker->SetGenerateOverlapEvents(false);
	LockOnMarker->SetCastShadow(false);
	LockOnMarker->SetRelativeLocation(FVector{0.0F, 0.0F, 115.0F});
	LockOnMarker->SetRelativeScale3D(FVector{0.12F});
	LockOnMarker->SetHiddenInGame(true);
	if (ExecutionMarkerMesh.Succeeded())
	{
		LockOnMarker->SetStaticMesh(ExecutionMarkerMesh.Object);
	}

	GetCapsuleComponent()->InitCapsuleSize(42.0F, 96.0F);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* const MovementComponent = GetCharacterMovement();
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->RotationRate = FRotator{0.0F, 500.0F, 0.0F};
	MovementComponent->JumpZVelocity = 500.0F;
	MovementComponent->AirControl = 0.35F;
	ActionMovementComponent->SetBaseWalkSpeed(500.0F);
	MovementComponent->MinAnalogWalkSpeed = 20.0F;
	MovementComponent->BrakingDecelerationWalking = 2000.0F;
	MovementComponent->BrakingDecelerationFalling = 1500.0F;
}

UAbilitySystemComponent* ASCLCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

bool ASCLCharacterBase::HasStartupAbility(const TSubclassOf<UGameplayAbility> AbilityClass) const
{
	if (AbilityClass == nullptr) return false;
	return StartupAbilities.ContainsByPredicate([AbilityClass](const TSubclassOf<UGameplayAbility> ConfiguredClass)
	{
		return ConfiguredClass != nullptr && ConfiguredClass->IsChildOf(AbilityClass);
	});
}

void ASCLCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->InitializeAbilityActorInfo(this, this);
		GrantStartupAbilities();
	}
}

void ASCLCharacterBase::GrantStartupAbilities()
{
	if (!HasAuthority() || AbilitySystemComponent == nullptr)
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (AbilityClass != nullptr)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec{AbilityClass, 1});
		}
	}
}

void ASCLCharacterBase::SetGuardVisualActive(const bool bActive)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GuardImpactFeedbackTimerHandle);
	}

	if (GuardVisual == nullptr)
	{
		return;
	}

	GuardVisual->SetRelativeScale3D(GuardVisualBaseScale);
	GuardVisual->SetHiddenInGame(!bActive);
}

void ASCLCharacterBase::PlayGuardImpactFeedback()
{
	if (GuardVisual == nullptr || GuardVisual->bHiddenInGame)
	{
		return;
	}

	GuardVisual->SetRelativeScale3D(GuardVisualImpactScale);
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			GuardImpactFeedbackTimerHandle,
			this,
			&ASCLCharacterBase::ResetGuardImpactFeedback,
			GuardImpactDurationSeconds,
			false);
	}
}

void ASCLCharacterBase::SetMovementFacingLocked(const FName FacingOwner, const bool bLocked)
{
	ActionMovementComponent->SetFacingLocked(FacingOwner, bLocked);
}

void ASCLCharacterBase::SetExecutionMarkerActive(const bool bActive)
{
	if (ExecutionMarker != nullptr)
	{
		ExecutionMarker->SetHiddenInGame(!bActive);
	}
}

void ASCLCharacterBase::SetLockOnMarkerActive(const bool bActive)
{
	if (LockOnMarker != nullptr)
	{
		LockOnMarker->SetHiddenInGame(!bActive);
	}
}

void ASCLCharacterBase::ResetGuardImpactFeedback()
{
	if (GuardVisual != nullptr)
	{
		GuardVisual->SetRelativeScale3D(GuardVisualBaseScale);
	}
}

float ASCLCharacterBase::TakeDamage(
	const float DamageAmount,
	FDamageEvent const& DamageEvent,
	AController* const EventInstigator,
	AActor* const DamageCauser)
{
	// 顺序很关键：无敌与成功弹反都返回 0；格挡只消耗防御体力；最后才扣生命。
	if (AbilitySystemComponent != nullptr &&
		AbilitySystemComponent->HasMatchingGameplayTag(SCLGameplayTags::State_Invincible))
	{
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("Damage missed: Target=%s Reason=State.Invincible IncomingDamage=%.2f"),
			*GetNameSafe(this),
			DamageAmount);
		return 0.0F;
	}

	const float ValidatedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (ValidatedDamage <= 0.0F)
	{
		return 0.0F;
	}

	if (IsDamageParried(DamageEvent, DamageCauser))
	{
		HandleSuccessfulParry(DamageEvent, DamageCauser);
		return 0.0F;
	}

	if (AbilitySystemComponent != nullptr && IsDamageBlocked(DamageEvent, DamageCauser))
	{
		PlayGuardImpactFeedback();
		const float RequestedGuardDamage = ValidatedDamage * GuardStaminaDamageMultiplier;
		const float AppliedGuardDamage =
			AbilitySystemComponent->ApplyGuardDamageToSelf(RequestedGuardDamage, DamageCauser);
		AbilitySystemComponent->RestartStaminaRegenerationDelay();
		const bool bGuardBroken =
			AbilitySystemComponent->HasMatchingGameplayTag(SCLGameplayTags::State_Staggered);
		if (bGuardBroken &&
			HitReactionComponent != nullptr &&
			DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			const FPointDamageEvent& PointDamageEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
			HitReactionComponent->ReactToPointDamage(PointDamageEvent, DamageCauser);
		}
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("Damage blocked: Target=%s IncomingDamage=%.2f GuardDamage=%.2f AppliedGuardDamage=%.2f RemainingStamina=%.2f GuardBroken=%s"),
			*GetNameSafe(this),
			ValidatedDamage,
			RequestedGuardDamage,
			AppliedGuardDamage,
			AbilitySystemComponent->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()),
			bGuardBroken ? TEXT("true") : TEXT("false"));
		return 0.0F;
	}

	const float AppliedDamage = AbilitySystemComponent != nullptr
		? AbilitySystemComponent->ApplyDamageToSelf(ValidatedDamage, DamageCauser)
		: 0.0F;
	// 普通敌人有韧性，不因每次生命伤害都播放全身受击；玩家仍可立即反馈点伤害。
	if (AppliedDamage > 0.0F && !IsA<ASCLEnemyCharacter>() &&
		HitReactionComponent != nullptr &&
		DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDamageEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		HitReactionComponent->ReactToPointDamage(PointDamageEvent, DamageCauser);
	}

	return AppliedDamage;
}

bool ASCLCharacterBase::IsDamageBlocked(
	const FDamageEvent& DamageEvent,
	const AActor* const DamageCauser) const
{
	if (AbilitySystemComponent == nullptr ||
		!AbilitySystemComponent->HasMatchingGameplayTag(SCLGameplayTags::State_Blocking))
	{
		return false;
	}
	return IsIncomingPointDamageWithinArc(DamageEvent, DamageCauser, BlockArcDegrees);
}

bool ASCLCharacterBase::IsDamageParried(
	const FDamageEvent& DamageEvent,
	AActor* const DamageCauser) const
{
	if (AbilitySystemComponent == nullptr ||
		!AbilitySystemComponent->HasMatchingGameplayTag(SCLGameplayTags::State_Parrying))
	{
		return false;
	}
	if (DamageCauser != nullptr &&
		DamageCauser->GetClass()->ImplementsInterface(USCLCombatInterface::StaticClass()))
	{
		const USCLCombatComponent* const AttackerCombat =
			ISCLCombatInterface::Execute_GetCombatComponent(DamageCauser);
		if (AttackerCombat != nullptr && !AttackerCombat->IsActiveAttackParryable())
		{
			return false;
		}
	}
	return IsIncomingPointDamageWithinArc(DamageEvent, DamageCauser, ParryArcDegrees);
}

bool ASCLCharacterBase::IsIncomingPointDamageWithinArc(
	const FDamageEvent& DamageEvent,
	const AActor* const DamageCauser,
	const float ArcDegrees) const
{
	if (!DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		return false;
	}

	const FPointDamageEvent& PointDamageEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
	FVector DirectionToSource = DamageCauser != nullptr
		? DamageCauser->GetActorLocation() - GetActorLocation()
		: -PointDamageEvent.ShotDirection;
	DirectionToSource.Z = 0.0F;
	DirectionToSource = DirectionToSource.GetSafeNormal();
	if (DirectionToSource.IsNearlyZero())
	{
		return false;
	}

	const float MinimumFacingDot = FMath::Cos(FMath::DegreesToRadians(ArcDegrees * 0.5F));
	return FVector::DotProduct(GetActorForwardVector(), DirectionToSource) >= MinimumFacingDot;
}

void ASCLCharacterBase::HandleSuccessfulParry(
	const FDamageEvent& DamageEvent,
	AActor* const DamageCauser)
{
	PlayGuardImpactFeedback();

	USCLAbilitySystemComponent* const AttackerAbilitySystem = Cast<USCLAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(DamageCauser));
	if (AttackerAbilitySystem != nullptr)
	{
		AttackerAbilitySystem->ApplyStaggeredState(TEXT("Parried"));
		AttackerAbilitySystem->ApplyExecutableState();
	}

	if (DamageCauser != nullptr &&
		DamageCauser->GetClass()->ImplementsInterface(USCLCombatInterface::StaticClass()))
	{
		if (USCLCombatComponent* const AttackerCombat =
			ISCLCombatInterface::Execute_GetCombatComponent(DamageCauser))
		{
			AttackerCombat->CancelActiveAttack();
		}
	}

	if (ASCLCharacterBase* const AttackerCharacter = Cast<ASCLCharacterBase>(DamageCauser))
	{
		// 伤害参数和“一次动作结算一次”状态属于实际激活的弹反技能，支持蓝图子类覆盖。
		FGameplayAbilitySpec* const ParrySpec = AbilitySystemComponent
			? AbilitySystemComponent->FindAbilitySpecByBaseClass(USCLParryAbility::StaticClass()) : nullptr;
		if (ParrySpec && ParrySpec->IsActive())
			if (USCLParryAbility* const Parry = Cast<USCLParryAbility>(ParrySpec->GetPrimaryInstance()))
			{
				Parry->NotifySuccessfulParry();
				const float CounterApplied = Parry->ApplyCounterDamage(AttackerCharacter);
				UE_LOG(LogSoulCombatLab, Log, TEXT("Parry counter damage: Defender=%s Attacker=%s AppliedDamage=%.2f"),
					*GetNameSafe(this), *GetNameSafe(AttackerCharacter), CounterApplied);
			}
		// 反击可直接击杀低血量目标；死亡后的角色不能再次播放失衡受击动作。
		if (AttackerAbilitySystem && AttackerAbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Dead)) return;
		if (USCLHitReactionComponent* const AttackerReaction =
			AttackerCharacter->GetHitReactionComponent();
			AttackerReaction != nullptr && DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			FPointDamageEvent ParryReactionEvent =
				static_cast<const FPointDamageEvent&>(DamageEvent);
			ParryReactionEvent.ShotDirection *= -1.0F;
			AttackerReaction->ReactToPointDamage(ParryReactionEvent, this);
		}
	}

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Damage parried: Defender=%s Attacker=%s State=State.Parrying AppliedDamage=0.00"),
		*GetNameSafe(this),
		*GetNameSafe(DamageCauser));
}
