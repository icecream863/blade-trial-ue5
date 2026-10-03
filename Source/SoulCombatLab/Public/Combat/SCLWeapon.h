#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SCLWeapon.generated.h"

class USceneComponent;
class USCLHitTraceComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
struct FHitResult;

// 武器 Actor：提供伤害数据、命中采样组件以及挥刀视觉效果。
UCLASS()
class SOULCOMBATLAB_API ASCLWeapon : public AActor
{
	GENERATED_BODY()

public:
	ASCLWeapon();

	UFUNCTION(BlueprintPure, Category = "Combat|Weapon")
	USCLHitTraceComponent* GetHitTraceComponent() const { return HitTraceComponent; }

	UFUNCTION(BlueprintPure, Category = "Combat|Weapon")
	float GetBaseDamage() const { return BaseDamage; }

	virtual void BeginSwingEffects() {}
	virtual void EndSwingEffects() {}
	virtual void PlayHitEffects(const FHitResult& HitResult) {}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> TraceStart;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> TraceEnd;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USCLHitTraceComponent> HitTraceComponent;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Weapon", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float BaseDamage{20.0F};
};

// GhostSamurai 玩家刀的武器预设，复用通用武器命中流程。
UCLASS()
class SOULCOMBATLAB_API ASCLGhostKatanaWeapon final : public ASCLWeapon
{
	GENERATED_BODY()

public:
	ASCLGhostKatanaWeapon();

	virtual void PlayHitEffects(const FHitResult& HitResult) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Effects")
	TObjectPtr<UNiagaraSystem> HitEffect;
};
