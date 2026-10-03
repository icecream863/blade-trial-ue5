#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SCLPlayerMovesetData.h"

#include "SCLPlayerComboComponent.generated.h"

// 选招结果，不是攻击执行结果：有 StepIndex 表示可以尝试播放，bBuffered 表示只收下输入。
// 两者都没有表示拒绝。这里不能返回 Executed，因为本组件根本不播放动画。
struct FSCLPlayerComboSelection
{
	int32 StepIndex{INDEX_NONE};
	bool bBuffered{false};
};

// 玩家连招状态机：只读取 Moveset、选择节点并保存一格输入，不反向调用 Combat。
// Combat 先向本组件取选招结果，再自行播放和扣费；成功后才确认当前节点。
// Notify 也先进入 Combat：本组件只交出待接节点，由 Combat 执行同一个起播流程。
UCLASS(ClassGroup = "SoulCombatLab", meta = (BlueprintSpawnableComponent))
class SOULCOMBATLAB_API USCLPlayerComboComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USCLPlayerComboComponent();

	// GAS 在激活前检查“下一招”成本，不是当前正播放招式的成本。
	float GetNextStaminaCost(ESCLPlayerAttackInput Input) const;
	// 返回只读节点指针；资产不存在或索引越界返回 nullptr。
	const FSCLPlayerAttackStep* GetStep(int32 StepIndex) const;
	// 起手索引、每招动画/名称和所有非终止后继都合法才返回 true。
	bool HasValidMoveset() const;
	// “活动”表示存在当前节点，不代表输入窗口已打开；窗口由 bComboWindowOpen 独立管理。
	bool IsComboActive() const { return ActiveStepIndex != INDEX_NONE; }
	int32 GetActiveStepIndex() const { return ActiveStepIndex; }
	// 由 Combat 在 Montage 和费用提交成功后确认；单纯选中或缓存时不能改变当前节点。
	void ConfirmAttackStarted(int32 StepIndex) { ActiveStepIndex = StepIndex; }
	// 无当前招式会直接起手；窗口未开则缓存输入；缓存占用或无后继则拒绝。
	FSCLPlayerComboSelection SelectAttack(ESCLPlayerAttackInput Input);
	// 打开窗口并取出未超时的待接节点；INDEX_NONE 表示没有可消费输入，不代表播放失败。
	int32 OpenComboWindow();
	void CloseComboWindow();
	// 攻击结束、取消或切换招式时重置，下一次请求重新走起手。
	void ResetCombo();

	// 玩家蓝图指定招式图：起手索引、各招动画/成本和轻重后继都从此资产读取。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Moveset", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USCLPlayerMovesetData> PlayerMoveset;

	// 在玩家蓝图的 PlayerComboComponent 上调整；窗口消费时超过此秒数就丢弃缓存。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Combo", meta = (ClampMin = "0.0", Units = "s", DisplayName = "连招输入缓存有效期"))
	float InputBufferLifetime{0.7F};

private:
	// 只查图，不消费缓存，不播放动画；INDEX_NONE 表示没有对应后继。
	int32 ResolveNextStep(ESCLPlayerAttackInput Input) const;
	// 消费只表示把输入转为节点索引；不操作动画、不扣费，也不调用其他组件。
	int32 ConsumeBufferedStep();

	// 真正的连招缓存：保存等待动画窗口打开的输入，不承担技能间的临时传参。
	ESCLPlayerAttackInput BufferedInput{ESCLPlayerAttackInput::Light};
	// INDEX_NONE 表示没有当前招式；此时下一次输入会选轻/重起手。
	int32 ActiveStepIndex{INDEX_NONE};
	// 记录世界时间（秒），消费时与当前时间做差，判断缓存是否过期。
	float BufferedAttackTime{0.0F};
	// 只保留一格待接招输入，第二次缓存请求会被拒绝；缓存不等于已经播放或扣费。
	bool bHasBufferedInput{false};
	// 当前 Montage 是否处于接招区间；它与“已有缓存输入”是两个独立条件。
	bool bComboWindowOpen{false};
};
