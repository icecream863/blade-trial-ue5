#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimEnums.h"
#include "SCLWeaponPresentationComponent.generated.h"
class UAnimMontage;
class UAnimInstance;
class USCLAbilitySystemComponent;
struct FAnimNotifyEventReference;

// 玩家武器表现：空闲收刀、攻击前拔刀、动画通知换挂点；招式与费用仍由 GAS/Combo 负责。
UCLASS(ClassGroup="SoulCombatLab", meta=(BlueprintSpawnableComponent))
class SOULCOMBATLAB_API USCLWeaponPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USCLWeaponPresentationComponent();
	// 闪避、格挡、受击等需要立刻反应的动作会取消收/拔刀并恢复握刀。
	void PrepareForAction();
	// 已入鞘时记住一次轻/重输入，拔刀动画结束后交回角色原有 GAS 请求。
	// 返回 true 表示输入已由拔刀流程接管；攻击费用到实际起播时才检查。
	bool QueueAttackWhileDrawing(bool bHeavy);
	// 仅接受当前收刀 Montage 实例的通知，旧混出通知不能把新攻击的刀挂回腰间。
	void CommitSheath(const FAnimNotifyEventReference& EventReference);
	void CommitDraw(const FAnimNotifyEventReference& EventReference);
	bool IsSheathed() const { return bSheathed; }
	bool IsSheathDrawEnabled() const { return bEnableSheathDraw; }
	bool IsSheathing() const { return ActiveSheathInstanceId != INDEX_NONE; }
	bool IsDrawing() const { return ActiveDrawInstanceId != INDEX_NONE; }
	UAnimMontage* GetSheathMontage() const { return LoadedSheathMontage; }
	UAnimMontage* GetDrawMontage() const { return LoadedDrawMontage; }
	float GetIdleDelay() const { return IdleSheathDelay; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	/** 暂时关闭自动收刀与攻击前拔刀；保留实现，之后可在玩家蓝图默认值中重新启用。 */
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Sheath", meta=(DisplayName="启用收刀与拔刀动作"))
	bool bEnableSheathDraw{false};
	// 没有战斗动作后等待的秒数；普通移动不阻止自动收刀。
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Sheath", meta=(ClampMin="0.1", Units="s", DisplayName="空闲收刀等待"))
	float IdleSheathDelay{3.0F};
	// 项目自己的动画副本，原资源保持不变；必须包含入鞘 Notify。
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Sheath", meta=(DisplayName="收刀动画"))
	TSoftObjectPtr<UAnimMontage> SheathMontage;
	// 与收刀相同骨架的项目动画副本；通知在手真正握刀时切换挂点。
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Sheath", meta=(DisplayName="拔刀动画"))
	TSoftObjectPtr<UAnimMontage> DrawMontage;
	// 资源中的实际名字带 Targer 拼写；不擅自重命名原骨架挂点。
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Sheath", meta=(DisplayName="刀入鞘挂点"))
	FName SheathedSocket{TEXT("katana_Targer01Socket")};
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> LoadedSheathMontage;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> LoadedDrawMontage;
	TWeakObjectPtr<USCLAbilitySystemComponent> ObservedASC;
	// 收刀和拔刀都提取但丢弃根位移，结束/中断后恢复原根运动设置。
	void RestoreRootMotion();
	FDelegateHandle TagChangedHandle;
	FTimerHandle IdleTimer;
	int32 ActiveSheathInstanceId{INDEX_NONE};
	int32 ActiveDrawInstanceId{INDEX_NONE};
	enum class EQueuedDrawAttack : uint8 { None, Light, Heavy };
	EQueuedDrawAttack QueuedDrawAttack{EQueuedDrawAttack::None};
	bool bDrawNotifyCommitted{false};
	bool bSheathed{false};
	void ArmIdleTimer(float Delay);
	void TryAutoSheath();
	bool IsCombatBusy() const;
	bool AttachWeapon(bool bToSheath);
	void CancelSheath(bool bReturnToHand = true);
	void CancelDraw();
	void HandleTagChanged(FGameplayTag Tag, int32 Count);
	void HandleMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 InstanceId);
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 InstanceId);
	void HandleDrawBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 InstanceId);
	void HandleDrawEnded(UAnimMontage* Montage, bool bInterrupted, int32 InstanceId);
};
