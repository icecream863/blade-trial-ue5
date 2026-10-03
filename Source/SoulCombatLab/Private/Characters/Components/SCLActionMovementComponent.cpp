#include "Characters/Components/SCLActionMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

USCLActionMovementComponent::USCLActionMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USCLActionMovementComponent::SetBaseWalkSpeed(const float Speed)
{
	if (!FMath::IsFinite(Speed)) return;
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (bSpeedBaselineSaved) { BaseWalkSpeed = FMath::Max(Speed, 0.0F); ApplyWalkSpeed(); }
		else Character->GetCharacterMovement()->MaxWalkSpeed = FMath::Max(Speed, 0.0F);
	}
}

void USCLActionMovementComponent::CaptureSpeedBaseline()
{
	if (!bSpeedBaselineSaved)
	{
		if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			BaseWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
			bSpeedBaselineSaved = true;
		}
	}
}

void USCLActionMovementComponent::SetWalkSpeedLimit(const FName Requester, const float Limit)
{
	if (Requester.IsNone() || !FMath::IsFinite(Limit)) return;
	CaptureSpeedBaseline();
	SpeedLimits.Add(Requester, FMath::Max(Limit, 0.0F));
	ApplyWalkSpeed();
}

void USCLActionMovementComponent::ClearWalkSpeedLimit(const FName Requester)
{
	if (SpeedLimits.Remove(Requester) > 0) ApplyWalkSpeed();
}

void USCLActionMovementComponent::SetWalkSpeedMultiplier(const FName Requester, const float Multiplier)
{
	if (Requester.IsNone() || !FMath::IsFinite(Multiplier)) return;
	CaptureSpeedBaseline();
	SpeedMultipliers.Add(Requester, FMath::Max(Multiplier, 0.0F));
	ApplyWalkSpeed();
}

void USCLActionMovementComponent::ClearWalkSpeedMultiplier(const FName Requester)
{
	if (SpeedMultipliers.Remove(Requester) > 0) ApplyWalkSpeed();
}

// 先取所有速度上限中的最小值，再乘动作倍率。结果只由当前申请决定，与释放顺序无关。
void USCLActionMovementComponent::ApplyWalkSpeed()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !bSpeedBaselineSaved) return;
	float Speed = BaseWalkSpeed;
	for (const auto& Limit : SpeedLimits) Speed = FMath::Min(Speed, Limit.Value);
	for (const auto& Multiplier : SpeedMultipliers) Speed *= Multiplier.Value;
	Character->GetCharacterMovement()->MaxWalkSpeed = Speed;
	if (SpeedLimits.IsEmpty() && SpeedMultipliers.IsEmpty()) bSpeedBaselineSaved = false;
}

void USCLActionMovementComponent::SetFacingLocked(const FName Requester, const bool bLocked)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || Requester.IsNone()) return;
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (bLocked)
	{
		if (FacingLocks.IsEmpty()) bOriginalOrientToMovement = Movement->bOrientRotationToMovement;
		FacingLocks.Add(Requester);
		Movement->bOrientRotationToMovement = false;
	}
	else if (FacingLocks.Remove(Requester) > 0 && FacingLocks.IsEmpty())
	{
		Movement->bOrientRotationToMovement = bOriginalOrientToMovement;
	}
}

bool USCLActionMovementComponent::RequestRootMotionMode(
	const FName Requester, const ERootMotionMode::Type Mode, const int32 Priority)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (Requester.IsNone() || !Anim) return false;
	// Mesh/AnimInstance 被更换时，先恢复旧实例，再在新实例建立自己的基线。
	if (RootMotionRequests.IsEmpty() || RootMotionInstance.Get() != Anim)
	{
		if (UAnimInstance* Old = RootMotionInstance.Get()) Old->SetRootMotionMode(OriginalRootMotionMode);
		RootMotionInstance = Anim;
		OriginalRootMotionMode = Anim->RootMotionMode;
	}
	RootMotionRequests.Add(Requester, {Mode, Priority, ++RequestOrder});
	ApplyRootMotionMode();
	return true;
}

void USCLActionMovementComponent::ReleaseRootMotionMode(const FName Requester)
{
	if (RootMotionRequests.Remove(Requester) > 0) ApplyRootMotionMode();
}

void USCLActionMovementComponent::ApplyRootMotionMode()
{
	UAnimInstance* Anim = RootMotionInstance.Get();
	if (!Anim) return;
	const FRootMotionRequest* Selected = nullptr;
	for (const auto& Request : RootMotionRequests)
	{
		if (!Selected || Request.Value.Priority > Selected->Priority ||
			(Request.Value.Priority == Selected->Priority && Request.Value.Order > Selected->Order))
			Selected = &Request.Value;
	}
	Anim->SetRootMotionMode(Selected ? Selected->Mode : OriginalRootMotionMode);
	if (!Selected) RootMotionInstance.Reset();
}

void USCLActionMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 组件退出时恢复一次基线，正常游戏中的每个动作仍必须成对撤销自己的申请。
	SpeedLimits.Empty();
	SpeedMultipliers.Empty();
	ApplyWalkSpeed();
	if (!FacingLocks.IsEmpty())
		if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
			Character->GetCharacterMovement()->bOrientRotationToMovement = bOriginalOrientToMovement;
	FacingLocks.Empty();
	RootMotionRequests.Empty();
	ApplyRootMotionMode();
	Super::EndPlay(EndPlayReason);
}
