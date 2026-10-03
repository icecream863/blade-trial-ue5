#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "SCLPlayerMovesetData.generated.h"

class UAnimMontage;

// 输入请求只有轻/重两种；最终招式还取决于当前节点与后继配置。
UENUM(BlueprintType)
enum class ESCLPlayerAttackInput : uint8
{
	Light,
	Heavy
};

// 玩家连招图中的一个节点：配置动画、成本、伤害倍率以及轻重输入的下一节点。
// Steps 数组下标就是招式索引；后继字段存的是下标，不是动画名。
USTRUCT(BlueprintType)
struct FSCLPlayerAttackStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	// 用于日志和观察当前招式，不用它查后继边。
	FName AttackName{NAME_None};

	// 用于描述这招的输入类型；实际选后继时读取的是 NextLight/NextHeavyStepIndex。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	ESCLPlayerAttackInput InputType{ESCLPlayerAttackInput::Light};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	// 本招实际播放的动画；ComboWindow 和 WeaponTrace 时间区间配置在该 Montage 上。
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	// 仅在本招实际起播时提交；输入缓存和缓存超时丢弃都不会按本招扣费。
	float StaminaCost{0.0F};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	// 本招生命伤害倍率，会与共享基础数据、武器伤害及执行配置一起参与结算。
	float DamageMultiplier{1.0F};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	// 本招韧性伤害倍率；当前结算仅在实际生命伤害大于零时施加韧性伤害。
	float PoiseDamageMultiplier{1.0F};

	// 当前招播放期间再次输入轻/重时要接到的 Steps 下标；-1 表示没有这条接招路线。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (ClampMin = "-1"))
	int32 NextLightStepIndex{INDEX_NONE};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (ClampMin = "-1"))
	int32 NextHeavyStepIndex{INDEX_NONE};
};

// 玩家招式数据资产：保存所有攻击节点与轻重起手索引。
UCLASS(BlueprintType)
class SOULCOMBATLAB_API USCLPlayerMovesetData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 只读安全查询，越界返回 nullptr；数组顺序决定所有索引的意义。
	const FSCLPlayerAttackStep* FindStep(int32 StepIndex) const;

	UFUNCTION(BlueprintPure, Category = "Player Combat")
	// 没有当前节点时查询；这里只选起手下标，不控制播放时机。
	int32 GetOpenerIndex(ESCLPlayerAttackInput Input) const;

	UFUNCTION(BlueprintPure, Category = "Player Combat")
	// 沿当前节点的一条轻/重边查询；返回 INDEX_NONE 表示此输入没有后继。
	int32 GetNextStepIndex(int32 CurrentStepIndex, ESCLPlayerAttackInput Input) const;

	// 没有正在播放的招式时，轻击/重击分别从哪个 Steps 下标开始。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moveset")
	int32 LightOpenerIndex{0};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moveset")
	int32 HeavyOpenerIndex{0};

	// 编辑数组顺序会改变下标含义；移动招式后要同步检查所有起手和后继索引。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moveset")
	TArray<FSCLPlayerAttackStep> Steps;
};
