#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "SCLTargetingComponent.generated.h"

class ASCLCharacterBase;
class USpringArmComponent;

// 玩家锁定组件：选择和切换目标，维护锁定镜头与朝向。
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SOULCOMBATLAB_API USCLTargetingComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	USCLTargetingComponent();
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void ToggleLock();
	void ClearTarget();
	void SwitchTargetLeft();
	void SwitchTargetRight();
	void DrawDebugState(float Lifetime) const;

	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	ASCLCharacterBase* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	bool IsLockedOn() const { return CurrentTarget.IsValid(); }


private:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	struct FTargetScore
	{
		TWeakObjectPtr<ASCLCharacterBase> Target;
		float Total{0.0F};
		float Distance{0.0F};
		float Angle{0.0F};
		float ScreenCenter{0.0F};
	};

	TOptional<FTargetScore> FindBestTarget() const;
	TArray<FTargetScore> FindCandidates() const;
	TOptional<FTargetScore> ScoreCandidate(ASCLCharacterBase& Candidate) const;
	bool HasLineOfSightTo(const ASCLCharacterBase& Candidate, const FVector& ViewLocation) const;
	void SwitchTarget(bool bSwitchRight);
	void LoseTarget(const TCHAR* Reason);
	void UpdateLockedFacing(float DeltaTime, ASCLCharacterBase& Target);
	void SetCurrentTarget(ASCLCharacterBase* NewTarget);

	UPROPERTY(EditDefaultsOnly, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float SearchRadius{2000.0F};

	UPROPERTY(EditDefaultsOnly, Category = "Targeting", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float MinimumViewDot{0.10F};

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Score", meta = (ClampMin = "0.0"))
	float DistanceWeight{0.30F};

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Score", meta = (ClampMin = "0.0"))
	float AngleWeight{0.25F};

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Score", meta = (ClampMin = "0.0"))
	float ScreenCenterWeight{0.45F};

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Switch", meta = (ClampMin = "0.0"))
	float SwitchSeparationPenalty{0.35F};

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Lock", meta = (ClampMin = "0.0", Units = "cm"))
	float TargetLossDistance{2300.0F};

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Lock", meta = (ClampMin = "0.0", Units = "s"))
	float OcclusionGracePeriod{0.60F};

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Lock", meta = (ClampMin = "0.0"))
	float CameraRotationSpeed{7.0F};

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Lock", meta = (ClampMin = "0.0"))
	float CharacterRotationSpeed{12.0F};

	/** 原生回退使用 Walk 步速；当前 BP_SCLPlayerSamurai 选 Run，覆盖为 437.5 cm/s。
	 * 换步伐时在玩家蓝图 TargetingComponent 调此上限，并配套调 CharacterMovement 的普通速度。 */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Lock", meta = (ClampMin = "0.0", Units = "cm/s"))
	float LockedWalkSpeed{180.0F};

	UPROPERTY()
	TWeakObjectPtr<ASCLCharacterBase> CurrentTarget;
	TWeakObjectPtr<USpringArmComponent> LockedCameraBoom;
	float SavedArmLength{400.0F};
	FVector SavedTargetOffset{ForceInit};
	FVector SavedSocketOffset{ForceInit};
	float OccludedDuration{0.0F};
};
