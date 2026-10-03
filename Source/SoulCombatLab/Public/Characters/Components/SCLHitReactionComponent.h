#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "SCLHitReactionComponent.generated.h"

class UAnimMontage;
struct FPointDamageEvent;

UENUM(BlueprintType)
enum class ESCLHitDirection : uint8
{
	Front,
	Back,
	Left,
	Right
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSCLHitReactionSignature,
	ESCLHitDirection,
	HitDirection,
	bool,
	bMontagePlayed);

// 角色受击表现组件：按伤害来源方向选择 Montage，并广播本次反馈是否播放。
// 伤害结算和攻击中断不在这里处理。
UCLASS(ClassGroup = "SoulCombatLab", meta = (BlueprintSpawnableComponent))
class SOULCOMBATLAB_API USCLHitReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	bool PlayStaggerReaction() { return PlayReactionMontage(ESCLHitDirection::Front); }
	USCLHitReactionComponent();

	bool ReactToPointDamage(const FPointDamageEvent& PointDamageEvent, const AActor* DamageCauser);

	UFUNCTION(BlueprintPure, Category = "Combat|Hit Reaction")
	ESCLHitDirection ClassifyHitDirection(const FVector& DirectionToSource) const;

	UFUNCTION(BlueprintPure, Category = "Combat|Hit Reaction")
	ESCLHitDirection GetLastHitDirection() const { return LastHitDirection; }

	UPROPERTY(BlueprintAssignable, Category = "Combat|Hit Reaction")
	FSCLHitReactionSignature OnHitReaction;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hit Reaction")
	TObjectPtr<UAnimMontage> FrontReactionMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hit Reaction")
	TObjectPtr<UAnimMontage> BackReactionMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hit Reaction")
	TObjectPtr<UAnimMontage> LeftReactionMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hit Reaction")
	TObjectPtr<UAnimMontage> RightReactionMontage;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Hit Reaction", meta = (AllowPrivateAccess = "true"))
	ESCLHitDirection LastHitDirection{ESCLHitDirection::Front};

	bool PlayReactionMontage(ESCLHitDirection HitDirection) const;
};
