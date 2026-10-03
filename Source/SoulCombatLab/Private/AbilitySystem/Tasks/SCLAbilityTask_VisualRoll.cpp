#include "AbilitySystem/Tasks/SCLAbilityTask_VisualRoll.h"

#include "Components/SkeletalMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
constexpr float MinimumRollDurationSeconds{0.1F};
constexpr float FullRollDegrees{360.0F};
constexpr float RollEaseExponent{1.5F};
}

USCLAbilityTask_VisualRoll::USCLAbilityTask_VisualRoll()
{
	bTickingTask = true;
}

USCLAbilityTask_VisualRoll* USCLAbilityTask_VisualRoll::CreateVisualRoll(
	UGameplayAbility* const OwningAbility,
	const FName TaskInstanceName,
	USkeletalMeshComponent* const InMeshComponent,
	const float Duration)
{
	USCLAbilityTask_VisualRoll* const Task =
		NewAbilityTask<USCLAbilityTask_VisualRoll>(OwningAbility, TaskInstanceName);
	Task->MeshComponent = InMeshComponent;
	Task->RollDuration = FMath::Max(Duration, MinimumRollDurationSeconds);
	return Task;
}

void USCLAbilityTask_VisualRoll::Activate()
{
	Super::Activate();
	if (MeshComponent == nullptr)
	{
		EndTask();
		return;
	}

	InitialRelativeTransform = MeshComponent->GetRelativeTransform();
	bInitialTransformCaptured = true;
}

void USCLAbilityTask_VisualRoll::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);
	TRACE_CPUPROFILER_EVENT_SCOPE(SCL_VisualRoll);
	if (MeshComponent == nullptr || !bInitialTransformCaptured)
	{
		EndTask();
		return;
	}

	ElapsedTime += DeltaTime;
	const float LinearAlpha = FMath::Clamp(ElapsedTime / RollDuration, 0.0F, 1.0F);
	const float RollAlpha = FMath::InterpEaseInOut(0.0F, 1.0F, LinearAlpha, RollEaseExponent);
	const FQuat RollRotation{
		FVector::RightVector,
		FMath::DegreesToRadians(FullRollDegrees * RollAlpha)};
	const FVector InitialLocation = InitialRelativeTransform.GetLocation();
	const FVector CapsuleCenteredOrbit = -RollRotation.RotateVector(-InitialLocation);
	const FVector RolledLocation{
		InitialLocation.X,
		InitialLocation.Y,
		CapsuleCenteredOrbit.Z};
	const FQuat RolledRotation = RollRotation * InitialRelativeTransform.GetRotation();
	MeshComponent->SetRelativeLocationAndRotation(RolledLocation, RolledRotation);

	if (LinearAlpha >= 1.0F)
	{
		EndTask();
	}
}

void USCLAbilityTask_VisualRoll::OnDestroy(const bool bAbilityEnded)
{
	if (MeshComponent != nullptr && bInitialTransformCaptured)
	{
		MeshComponent->SetRelativeTransform(InitialRelativeTransform);
	}
	bInitialTransformCaptured = false;
	Super::OnDestroy(bAbilityEnded);
}
