#include "Targeting/SCLTargetingComponent.h"
#include "Characters/Components/SCLActionMovementComponent.h"

#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "Characters/SCLCharacterBase.h"
#include "CollisionQueryParams.h"
#include "Debug/SCLCombatDebugSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "SoulCombatLab.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
constexpr float TargetAimHeight{60.0F};
constexpr float MinimumViewportExtent{1.0F};
}

USCLTargetingComponent::USCLTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USCLTargetingComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* const ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TRACE_CPUPROFILER_EVENT_SCOPE(SCL_LockOnTick);

	ASCLCharacterBase* const Target = CurrentTarget.Get();
	const AActor* const Owner = GetOwner();
	if (Target == nullptr || Owner == nullptr)
	{
		LoseTarget(TEXT("Invalid"));
		return;
	}

	const USCLAbilitySystemComponent* const TargetAbilitySystem = Target->GetSCLAbilitySystemComponent();
	if (TargetAbilitySystem == nullptr ||
		TargetAbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
	{
		LoseTarget(TEXT("Dead"));
		return;
	}

	if (FVector::DistSquared2D(Owner->GetActorLocation(), Target->GetActorLocation()) >
		FMath::Square(TargetLossDistance))
	{
		LoseTarget(TEXT("Distance"));
		return;
	}

	const APawn* const OwnerPawn = Cast<APawn>(Owner);
	const APlayerController* const PlayerController = OwnerPawn != nullptr
		? Cast<APlayerController>(OwnerPawn->GetController())
		: nullptr;
	FVector ViewLocation{Owner->GetActorLocation()};
	FRotator ViewRotation{FRotator::ZeroRotator};
	if (PlayerController != nullptr)
	{
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	OccludedDuration = HasLineOfSightTo(*Target, ViewLocation)
		? 0.0F
		: OccludedDuration + DeltaTime;
	if (OccludedDuration >= OcclusionGracePeriod)
	{
		LoseTarget(TEXT("Occluded"));
		return;
	}

	UpdateLockedFacing(DeltaTime, *Target);
}

void USCLTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetCurrentTarget(nullptr);
	Super::EndPlay(EndPlayReason);
}

void USCLTargetingComponent::ToggleLock()
{
	if (CurrentTarget.IsValid())
	{
		ClearTarget();
		return;
	}

	const TOptional<FTargetScore> BestTarget = FindBestTarget();
	if (!BestTarget.IsSet())
	{
		UE_LOG(LogSoulCombatLab, Log, TEXT("Lock-On failed: no valid target."));
		return;
	}

	SetCurrentTarget(BestTarget->Target.Get());
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Lock-On acquired: Target=%s Score=%.3f Distance=%.3f Angle=%.3f ScreenCenter=%.3f"),
		*GetNameSafe(BestTarget->Target.Get()),
		BestTarget->Total,
		BestTarget->Distance,
		BestTarget->Angle,
		BestTarget->ScreenCenter);
}

void USCLTargetingComponent::ClearTarget()
{
	ASCLCharacterBase* const PreviousTarget = CurrentTarget.Get();
	SetCurrentTarget(nullptr);
	if (PreviousTarget != nullptr)
	{
		UE_LOG(LogSoulCombatLab, Log, TEXT("Lock-On released: Target=%s"), *GetNameSafe(PreviousTarget));
	}
}

void USCLTargetingComponent::SwitchTargetLeft()
{
	SwitchTarget(false);
}

void USCLTargetingComponent::SwitchTargetRight()
{
	SwitchTarget(true);
}

TOptional<USCLTargetingComponent::FTargetScore> USCLTargetingComponent::FindBestTarget() const
{
	const TArray<FTargetScore> Candidates = FindCandidates();
	TOptional<FTargetScore> BestTarget;
	for (const FTargetScore& Candidate : Candidates)
	{
		if (!BestTarget.IsSet() || Candidate.Total > BestTarget->Total)
		{
			BestTarget = Candidate;
		}
	}
	return BestTarget;
}

TArray<USCLTargetingComponent::FTargetScore> USCLTargetingComponent::FindCandidates() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SCL_TargetSearch);
	const AActor* const Owner = GetOwner();
	UWorld* const World = GetWorld();
	if (Owner == nullptr || World == nullptr)
	{
		return TArray<FTargetScore>{};
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams{SCENE_QUERY_STAT(SCLLockOnSearch), false, Owner};
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Owner->GetActorLocation(),
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(SearchRadius),
		QueryParams);

	TArray<FTargetScore> Candidates;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ASCLCharacterBase* const Candidate = Cast<ASCLCharacterBase>(Overlap.GetActor());
		if (Candidate == nullptr || Candidate == Owner)
		{
			continue;
		}

		const TOptional<FTargetScore> CandidateScore = ScoreCandidate(*Candidate);
		if (USCLCombatDebugSubsystem::IsEnabledForWorld(World))
		{
			const FColor Color = CandidateScore.IsSet() ? FColor::Green : FColor::Red;
			const FVector Location = Candidate->GetActorLocation() + FVector::UpVector * TargetAimHeight;
			DrawDebugSphere(World, Location, 25.0F, 12, Color, false, USCLCombatDebugSubsystem::EventLifetime);
			const FString Label = CandidateScore.IsSet()
				? FString::Printf(TEXT("Candidate %.3f"), CandidateScore->Total) : TEXT("Rejected");
			DrawDebugString(World, Location + FVector{0.0F, 0.0F, 35.0F}, Label, nullptr,
				Color, USCLCombatDebugSubsystem::EventLifetime, true);
		}
		if (CandidateScore.IsSet())
		{
			Candidates.Add(CandidateScore.GetValue());
		}
	}
	return Candidates;
}

TOptional<USCLTargetingComponent::FTargetScore> USCLTargetingComponent::ScoreCandidate(
	ASCLCharacterBase& Candidate) const
{
	const AActor* const Owner = GetOwner();
	const APawn* const OwnerPawn = Cast<APawn>(Owner);
	APlayerController* const PlayerController = OwnerPawn != nullptr
		? Cast<APlayerController>(OwnerPawn->GetController())
		: nullptr;
	const USCLAbilitySystemComponent* const CandidateAbilitySystem =
		Candidate.GetSCLAbilitySystemComponent();
	if (Owner == nullptr || PlayerController == nullptr || CandidateAbilitySystem == nullptr ||
		CandidateAbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
	{
		return {};
	}

	FVector ViewLocation{FVector::ZeroVector};
	FRotator ViewRotation{FRotator::ZeroRotator};
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TargetLocation = Candidate.GetActorLocation() + FVector::UpVector * TargetAimHeight;
	const FVector ViewOffset = TargetLocation - ViewLocation;
	const float Distance = FVector::Distance(Owner->GetActorLocation(), Candidate.GetActorLocation());
	const float ViewDot = FVector::DotProduct(ViewRotation.Vector(), ViewOffset.GetSafeNormal());
	if (Distance > SearchRadius || ViewDot < MinimumViewDot ||
		!HasLineOfSightTo(Candidate, ViewLocation))
	{
		return {};
	}

	FVector2D ScreenPosition{FVector2D::ZeroVector};
	int32 ViewportWidth{0};
	int32 ViewportHeight{0};
	if (!PlayerController->ProjectWorldLocationToScreen(TargetLocation, ScreenPosition, true))
	{
		return {};
	}
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	const FVector2D ScreenCenter{
		static_cast<float>(ViewportWidth) * 0.5F,
		static_cast<float>(ViewportHeight) * 0.5F};
	const float MaximumScreenDistance = FMath::Max(ScreenCenter.Size(), MinimumViewportExtent);
	const float DistanceScore = 1.0F - FMath::Clamp(Distance / SearchRadius, 0.0F, 1.0F);
	const float AngleScore = FMath::GetMappedRangeValueClamped(
		FVector2D{MinimumViewDot, 1.0F},
		FVector2D{0.0F, 1.0F},
		ViewDot);
	const float CenterScore = 1.0F - FMath::Clamp(
		FVector2D::Distance(ScreenPosition, ScreenCenter) / MaximumScreenDistance,
		0.0F,
		1.0F);

	FTargetScore Score;
	Score.Target = &Candidate;
	Score.Distance = DistanceScore;
	Score.Angle = AngleScore;
	Score.ScreenCenter = CenterScore;
	Score.Total = DistanceScore * DistanceWeight + AngleScore * AngleWeight +
		CenterScore * ScreenCenterWeight;
	return Score;
}

void USCLTargetingComponent::DrawDebugState(const float Lifetime) const
{
	if (GetOwner() == nullptr || !USCLCombatDebugSubsystem::IsEnabledForWorld(GetWorld()))
	{
		return;
	}
	const FVector Origin = GetOwner()->GetActorLocation();
	DrawDebugSphere(GetWorld(), Origin, SearchRadius, 32, FColor::Blue, false, Lifetime);
	if (const ASCLCharacterBase* const Target = CurrentTarget.Get())
	{
		DrawDebugDirectionalArrow(GetWorld(), Origin,
			Target->GetActorLocation() + FVector::UpVector * TargetAimHeight,
			35.0F, FColor::Yellow, false, Lifetime, 0, 2.0F);
	}
}

bool USCLTargetingComponent::HasLineOfSightTo(
	const ASCLCharacterBase& Candidate,
	const FVector& ViewLocation) const
{
	UWorld* const World = GetWorld();
	const AActor* const Owner = GetOwner();
	if (World == nullptr || Owner == nullptr)
	{
		return false;
	}

	FCollisionQueryParams QueryParams{SCENE_QUERY_STAT(SCLLockOnVisibility), true, Owner};
	FHitResult Hit;
	const FVector TargetLocation = Candidate.GetActorLocation() + FVector::UpVector * TargetAimHeight;
	const bool bBlocked = World->LineTraceSingleByChannel(
		Hit,
		ViewLocation,
		TargetLocation,
		ECC_Visibility,
		QueryParams);
	return !bBlocked || Hit.GetActor() == &Candidate;
}

void USCLTargetingComponent::SwitchTarget(const bool bSwitchRight)
{
	ASCLCharacterBase* const PreviousTarget = CurrentTarget.Get();
	if (PreviousTarget == nullptr)
	{
		ToggleLock();
		return;
	}

	const APawn* const OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* const PlayerController = OwnerPawn != nullptr
		? Cast<APlayerController>(OwnerPawn->GetController())
		: nullptr;
	if (PlayerController == nullptr)
	{
		return;
	}

	FVector ViewLocation{FVector::ZeroVector};
	FRotator ViewRotation{FRotator::ZeroRotator};
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewRight = FRotationMatrix{ViewRotation}.GetUnitAxis(EAxis::Y);
	const FVector PreviousDirection =
		(PreviousTarget->GetActorLocation() - ViewLocation).GetSafeNormal();
	const float PreviousHorizontal = FVector::DotProduct(ViewRight, PreviousDirection);
	const float DesiredSign = bSwitchRight ? 1.0F : -1.0F;

	TOptional<FTargetScore> BestTarget;
	float BestSwitchScore{-TNumericLimits<float>::Max()};
	for (const FTargetScore& Candidate : FindCandidates())
	{
		ASCLCharacterBase* const CandidateActor = Candidate.Target.Get();
		if (CandidateActor == nullptr || CandidateActor == PreviousTarget)
		{
			continue;
		}

		const FVector CandidateDirection =
			(CandidateActor->GetActorLocation() - ViewLocation).GetSafeNormal();
		const float HorizontalDelta =
			FVector::DotProduct(ViewRight, CandidateDirection) - PreviousHorizontal;
		if (HorizontalDelta * DesiredSign <= 0.0F)
		{
			continue;
		}

		const float SwitchScore = Candidate.Total - FMath::Abs(HorizontalDelta) * SwitchSeparationPenalty;
		if (SwitchScore > BestSwitchScore)
		{
			BestSwitchScore = SwitchScore;
			BestTarget = Candidate;
		}
	}

	if (!BestTarget.IsSet())
	{
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("Lock-On switch failed: Direction=%s Reason=NoCandidate"),
			bSwitchRight ? TEXT("Right") : TEXT("Left"));
		return;
	}

	SetCurrentTarget(BestTarget->Target.Get());
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Lock-On switched: Direction=%s Previous=%s Target=%s Score=%.3f"),
		bSwitchRight ? TEXT("Right") : TEXT("Left"),
		*GetNameSafe(PreviousTarget),
		*GetNameSafe(BestTarget->Target.Get()),
		BestTarget->Total);
}

void USCLTargetingComponent::LoseTarget(const TCHAR* const Reason)
{
	ASCLCharacterBase* const LostTarget = CurrentTarget.Get();
	SetCurrentTarget(nullptr);
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Lock-On lost: Target=%s Reason=%s"),
		*GetNameSafe(LostTarget),
		Reason);
}

void USCLTargetingComponent::UpdateLockedFacing(
	const float DeltaTime,
	ASCLCharacterBase& Target)
{
	ACharacter* const OwnerCharacter = Cast<ACharacter>(GetOwner());
	APlayerController* const PlayerController = OwnerCharacter != nullptr
		? Cast<APlayerController>(OwnerCharacter->GetController())
		: nullptr;
	if (OwnerCharacter == nullptr || PlayerController == nullptr)
	{
		return;
	}

	const FVector DirectionToTarget =
		(Target.GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
	const FRotator DesiredCharacterRotation{0.0F, DirectionToTarget.Rotation().Yaw, 0.0F};
	const ASCLCharacterBase* Character = Cast<ASCLCharacterBase>(OwnerCharacter);
	const bool bDodging = Character && Character->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Dodging);
	// 八向闪避保持起招朝向，镜头仍可跟随锁定目标。
	if (!bDodging) OwnerCharacter->SetActorRotation(FMath::RInterpTo(
		OwnerCharacter->GetActorRotation(),
		DesiredCharacterRotation,
		DeltaTime,
		CharacterRotationSpeed));

	// The orbit pivot is stable when the camera rotates or its arm hits a wall.
	// Feeding the last camera position back into the aim caused close-range low angles.
	const USpringArmComponent* const CameraBoom = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
	const FVector CameraPivot = CameraBoom != nullptr
		? CameraBoom->GetComponentLocation() + CameraBoom->TargetOffset
		: OwnerCharacter->GetActorLocation();
	constexpr float CameraAimHeight{20.0F};
	constexpr float MinimumFramingDistance{200.0F};
	const FVector AimOffset = Target.GetActorLocation() + FVector::UpVector * CameraAimHeight - CameraPivot;
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(
		AimOffset.Z, FMath::Max(AimOffset.Size2D(), static_cast<double>(MinimumFramingDistance))));
	const FRotator DesiredViewRotation{
		FMath::Clamp(Pitch - 4.0F, -25.0F, -16.0F), DesiredCharacterRotation.Yaw, 0.0F};
	PlayerController->SetControlRotation(FMath::RInterpTo(
		PlayerController->GetControlRotation(),
		DesiredViewRotation,
		DeltaTime,
		CameraRotationSpeed));
}

void USCLTargetingComponent::SetCurrentTarget(ASCLCharacterBase* const NewTarget)
{
	if (ASCLCharacterBase* const PreviousTarget = CurrentTarget.Get())
	{
		PreviousTarget->SetLockOnMarkerActive(false);
	}

	CurrentTarget = NewTarget;
	OccludedDuration = 0.0F;
	if (NewTarget != nullptr)
	{
		NewTarget->SetLockOnMarkerActive(true);
	}

	const bool bIsLocked = CurrentTarget.IsValid();
	ASCLCharacterBase* const OwnerCharacter = Cast<ASCLCharacterBase>(GetOwner());
	if (bIsLocked && !LockedCameraBoom.IsValid() && OwnerCharacter != nullptr)
	{
		if (USpringArmComponent* const CameraBoom = OwnerCharacter->FindComponentByClass<USpringArmComponent>())
		{
			LockedCameraBoom = CameraBoom;
			SavedArmLength = CameraBoom->TargetArmLength;
			SavedTargetOffset = CameraBoom->TargetOffset;
			SavedSocketOffset = CameraBoom->SocketOffset;
			CameraBoom->TargetArmLength = 460.0F;
			CameraBoom->TargetOffset = FVector{0.0F, 0.0F, 70.0F};
			CameraBoom->SocketOffset = FVector{0.0F, 120.0F, 0.0F};
		}
	}
	else if (!bIsLocked)
	{
		if (USpringArmComponent* const CameraBoom = LockedCameraBoom.Get())
		{
			CameraBoom->TargetArmLength = SavedArmLength;
			CameraBoom->TargetOffset = SavedTargetOffset;
			CameraBoom->SocketOffset = SavedSocketOffset;
		}
		LockedCameraBoom.Reset();
	}
	// 锁定只管理 LockOn 申请；格挡倍率由移动组件自动组合，无需等待格挡补做恢复。
	if (OwnerCharacter)
	{
		auto* Limits = OwnerCharacter->GetActionMovementComponent();
		if (bIsLocked) Limits->SetWalkSpeedLimit(TEXT("LockOn"), LockedWalkSpeed);
		else Limits->ClearWalkSpeedLimit(TEXT("LockOn"));
		Limits->SetFacingLocked(TEXT("LockOn"), bIsLocked);
	}
	SetComponentTickEnabled(bIsLocked);
}
