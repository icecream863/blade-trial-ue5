#pragma once

#include "CoreMinimal.h"

#include "SCLBossTypes.generated.h"

UENUM(BlueprintType)
enum class ESCLBossPhase : uint8
{
	PhaseOne,
	PhaseTwo
};

UENUM(BlueprintType)
enum class ESCLBossAttack : uint8
{
	None,
	LightSlash,
	HeavySlash,
	Combo,
	DashSlash,
	AreaAttack,
	DelayedAttack
};

// Boss 选招输入：当前距离、阶段和近期出招历史。
USTRUCT(BlueprintType)
struct SOULCOMBATLAB_API FSCLBossUtilityContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Utility", meta = (ClampMin = "0.0", Units = "cm"))
	float DistanceToTarget{0.0F};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Utility")
	ESCLBossPhase Phase{ESCLBossPhase::PhaseOne};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Utility")
	ESCLBossAttack PreviousAttack{ESCLBossAttack::None};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Utility")
	ESCLBossAttack AttackBeforePrevious{ESCLBossAttack::None};
};

// Boss 选招结果：选中的招式及其评分。
USTRUCT(BlueprintType)
struct SOULCOMBATLAB_API FSCLBossAttackDecision
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Utility")
	ESCLBossAttack Attack{ESCLBossAttack::None};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Utility")
	float Score{0.0F};
};

// Boss 招式的执行参数，供战斗组件配置此次攻击。
USTRUCT(BlueprintType)
struct SOULCOMBATLAB_API FSCLBossAttackExecutionProfile
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack")
	ESCLBossAttack Attack{ESCLBossAttack::None};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack")
	float DamageMultiplier{1.0F};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack")
	float PoiseDamageMultiplier{1.0F};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack")
	float MontagePlayRate{1.0F};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack")
	int32 MontageStepIndex{0};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack")
	int32 MontageStepCount{1};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack", meta = (Units = "s"))
	float AttackStateDuration{1.0F};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack", meta = (Units = "s"))
	float RecoveryDuration{0.75F};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack")
	bool bParryable{true};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack", meta = (Units = "cm/s"))
	float DashSpeed{0.0F};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack", meta = (Units = "s"))
	float DashDuration{0.0F};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack", meta = (Units = "cm"))
	float AreaRadius{0.0F};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack", meta = (Units = "s"))
	float AreaImpactDelay{0.0F};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Attack")
	bool bWeaponTraceEnabled{true};
};
