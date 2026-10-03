#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SCLPlayerDeveloperComponent.generated.h"
class ASCLPlayerCharacter;
class ASCLBossCharacter;
class ASCLTrainingDummy;

// 玩家开发调试组件：创建测试敌人、摆位和定时伤害采样；不参与正常输入或连招选择。
UCLASS()
class SOULCOMBATLAB_API USCLPlayerDeveloperComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USCLPlayerDeveloperComponent();
	// 防御与处决实验：准备条件，按原时间采样伤害和状态。
	/** 请求一次躲闪，在 0.35/0.80 秒采样伤害；以日志的实际伤害判断窗口内外表现。 */
	void DebugTestDodgeIFrame();
	/** 恢复生命/体力后进入格挡，依次安排正面、背面和连续伤害采样。 Phase 名称只是日志标签，实际格挡和破防结果要看输出属性与状态。 */
	void DebugTestBlock();
	/** 要求世界中已有训练靶，恢复生命、朝向伤害来源并请求弹反。 在 0.16/0.40 秒用弱引用安排点伤害采样，比较窗口时序与攻击者失衡。 */
	void DebugTestParry();
	/** 准备训练靶的可处决状态与玩家摆位，立即请求处决，再延迟记录目标生命和死亡状态。 */
	void DebugTestExecution();
	/** 只准备训练靶和摆位，不自动发起技能；之后可手动按 E 观察完整处决。 */
	void DebugPrepareExecution();
	// 场景实验：创建临时目标与敌人，调整摆位。
	/** 准备多个候选目标与玩家朝向，供手动检查锁定和滚轮切换。 */
	void DebugPrepareLockOnSwitch();
	/** 在玩家视角前方生成感知测试敌人，供检查目标发现与追击。 */
	void DebugSpawnPerceptionEnemy();
	/** 在玩家视角前方生成重兵，便于比较其攻击与防御表现。 */
	void DebugSpawnHeavyEnemy();
	/** 在玩家前方分开摆放剑兵和重兵，观察同一套 AI 下的配置差异。 */
	void DebugSpawnEnemyVariants();
	// Boss 实验：临时 Boss 的引用和测试参数只属于开发组件。
	/** 按视角方向生成临时 Boss；DistanceToTarget 单位为厘米，最小限制为 150。 弱引用只记录最近生成的 Boss，不等同于自动销毁先前的 Boss。 */
	void DebugSpawnBossPrototype(float DistanceToTarget = 800.0F);
	/** 把临时 Boss 当前生命设为最大生命的一半，利用已有属性监听驱动阶段变化。 */
	void DebugSetBossPhaseTwo();
	/** 以给定距离调用临时 Boss 选招；该参数用于策略评估，不会移动玩家或 Boss。 */
	void DebugEvaluateBossUtility(float DistanceToTarget);
	/** 组合生成、距离选招和半血阶段操作，并打印选招结果；不代替真实挥刀验证。 */
	void DebugTestBossFoundation();
	/** 提高玩家生命/韧性以便观察，生成 Boss；可选半血开场，供手动战斗检查。 */
	void DebugPrepareBossCombatTest(bool bStartInPhaseTwo = false);
	/** 对当前锁定目标施加等于剩余生命的伤害；仍经过伤害入口，不直接写死亡状态。 */
	void DebugKillLockedTarget();
	// 调试显示开关：不修改玩家战斗规则。
	/** 切换当前 World 的战斗调试绘制开关，不影响其他 World。 */
	void DebugToggleCombatDraw();
	/** 通过当前 Controller 的 HUD 切换开发面板；HUD 类型不匹配时不执行。 */
	void DebugToggleDeveloperHUD();
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	ASCLPlayerCharacter* GetPlayer() const;
	/** 生成指定来向的点伤害并打印生命、体力、格挡与失衡状态；bFromFront 选择前后方向。 */
	void ApplyDebugBlockDamage(FName Phase, bool bFromFront, float DamageAmount);
	/** 对玩家施加来自指定 Actor 的点伤害，输出实际伤害、弹反标签和攻击者失衡。 */
	void ApplyDebugParryDamage(FName Phase, AActor* DamageCauser, float DamageAmount);
	/** 查找已有训练靶、恢复其生命并施加可处决状态，把玩家放到靶后 150 cm。 没有目标或 ASC 时返回 nullptr，调用者不能继续执行处决。 */
	ASCLTrainingDummy* PrepareDebugExecutionTarget();
	// 最近生成的实验 Boss；弱引用不延长 Actor 寿命，正式 Demo Boss 由流程系统管理。
	TWeakObjectPtr<ASCLBossCharacter> DebugBossPrototype;
};
