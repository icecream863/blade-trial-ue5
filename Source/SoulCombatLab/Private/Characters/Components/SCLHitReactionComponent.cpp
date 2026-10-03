#include "Characters/Components/SCLHitReactionComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "SoulCombatLab.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const TCHAR* LexToString(const ESCLHitDirection HitDirection)
{
	switch (HitDirection)
	{
	case ESCLHitDirection::Front:
		return TEXT("Front");
	case ESCLHitDirection::Back:
		return TEXT("Back");
	case ESCLHitDirection::Left:
		return TEXT("Left");
	case ESCLHitDirection::Right:
		return TEXT("Right");
	default:
		return TEXT("Unknown");
	}
}
}

USCLHitReactionComponent::USCLHitReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> FrontReactionAsset(
		TEXT("/Game/SoulCombatLab/Animations/HitReact/AM_HitReact_Front.AM_HitReact_Front"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> BackReactionAsset(
		TEXT("/Game/SoulCombatLab/Animations/HitReact/AM_HitReact_Back.AM_HitReact_Back"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> LeftReactionAsset(
		TEXT("/Game/SoulCombatLab/Animations/HitReact/AM_HitReact_Left.AM_HitReact_Left"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> RightReactionAsset(
		TEXT("/Game/SoulCombatLab/Animations/HitReact/AM_HitReact_Right.AM_HitReact_Right"));

	FrontReactionMontage = FrontReactionAsset.Object;
	BackReactionMontage = BackReactionAsset.Object;
	LeftReactionMontage = LeftReactionAsset.Object;
	RightReactionMontage = RightReactionAsset.Object;
}

bool USCLHitReactionComponent::ReactToPointDamage(
	const FPointDamageEvent& PointDamageEvent,
	const AActor* const DamageCauser)
{
	const AActor* const ComponentOwner = GetOwner();
	if (ComponentOwner == nullptr)
	{
		return false;
	}

	FVector DirectionToSource = -FVector{PointDamageEvent.ShotDirection};
	if (IsValid(DamageCauser) && DamageCauser != ComponentOwner)
	{
		DirectionToSource = DamageCauser->GetActorLocation() - ComponentOwner->GetActorLocation();
	}

	DirectionToSource.Z = 0.0F;
	if (!DirectionToSource.Normalize())
	{
		return false;
	}

	LastHitDirection = ClassifyHitDirection(DirectionToSource);
	const bool bMontagePlayed = PlayReactionMontage(LastHitDirection);
	OnHitReaction.Broadcast(LastHitDirection, bMontagePlayed);
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Hit reaction: Victim=%s Direction=%s MontagePlayed=%s"),
		*GetNameSafe(ComponentOwner),
		LexToString(LastHitDirection),
		bMontagePlayed ? TEXT("true") : TEXT("false"));
	return bMontagePlayed;
}

ESCLHitDirection USCLHitReactionComponent::ClassifyHitDirection(const FVector& DirectionToSource) const
{
	const AActor* const ComponentOwner = GetOwner();
	const FVector HorizontalDirection = DirectionToSource.GetSafeNormal2D();
	if (ComponentOwner == nullptr || HorizontalDirection.IsNearlyZero())
	{
		return ESCLHitDirection::Front;
	}

	const float ForwardDot = FVector::DotProduct(
		ComponentOwner->GetActorForwardVector().GetSafeNormal2D(),
		HorizontalDirection);
	const float RightDot = FVector::DotProduct(
		ComponentOwner->GetActorRightVector().GetSafeNormal2D(),
		HorizontalDirection);

	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		return ForwardDot >= 0.0F ? ESCLHitDirection::Front : ESCLHitDirection::Back;
	}

	return RightDot >= 0.0F ? ESCLHitDirection::Right : ESCLHitDirection::Left;
}

bool USCLHitReactionComponent::PlayReactionMontage(const ESCLHitDirection HitDirection) const
{
	UAnimMontage* ReactionMontage = nullptr;
	switch (HitDirection)
	{
	case ESCLHitDirection::Front:
		ReactionMontage = FrontReactionMontage;
		break;
	case ESCLHitDirection::Back:
		ReactionMontage = BackReactionMontage;
		break;
	case ESCLHitDirection::Left:
		ReactionMontage = LeftReactionMontage;
		break;
	case ESCLHitDirection::Right:
		ReactionMontage = RightReactionMontage;
		break;
	default:
		break;
	}

	const AActor* const ComponentOwner = GetOwner();
	USkeletalMeshComponent* const OwnerMesh =
		ComponentOwner != nullptr ? ComponentOwner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	UAnimInstance* const AnimInstance = OwnerMesh != nullptr ? OwnerMesh->GetAnimInstance() : nullptr;
	if (AnimInstance == nullptr || ReactionMontage == nullptr)
	{
		return false;
	}

	if (AnimInstance->Montage_IsPlaying(ReactionMontage))
	{
		AnimInstance->Montage_SetPosition(ReactionMontage, 0.0F);
		return true;
	}

	return AnimInstance->Montage_Play(ReactionMontage) > 0.0F;
}
