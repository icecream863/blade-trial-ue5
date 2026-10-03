#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "SCLDemoPlayerController.generated.h"
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedInputComponent;
struct FInputActionValue;

// 玩家控制器：持有输入资产和键位映射、统一绑定玩家输入，解释移动、视角、攻击与防御按键，并管理菜单热键。
// Content 中的 BP_SCLPlayerController 继承此类，由玩家 GameMode 选择。
UCLASS(Blueprintable)
class SOULCOMBATLAB_API ASCLDemoPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ASCLDemoPlayerController();
	// Started 开始计时，Completed 判短按，Canceled 只清理，计时器到点判长按。
	void HandleAttackPressed();
	void HandleAttackReleased();
	void HandleAttackCanceled();
	// 秒；这是短按/长按阈值，与 ComboComponent 的输入缓存有效期无关。
	float GetHeavyAttackHoldThreshold() const { return HeavyAttackHoldThreshold; }
	// 供调试与流程验证读取；映射对象由 Controller 持有，重生不重新创建。
	UInputMappingContext* GetRuntimeCombatMappingContext() const { return RuntimeCombatMappingContext; }
	UInputAction* GetDeveloperDebugHUDAction() const { return DeveloperDebugHUDAction; }
protected:
	virtual void PostInitializeComponents() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* InPawn) override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnUnPossess() override;
private:
	// 配置、绑定与安装由同一个类负责；只向当前 Pawn 发送动作请求。
	void BindPlayerActions(UEnhancedInputComponent* Input);
	void InstallLocalInputMappings();
	void RemoveLocalInputMappings();
	// 记录实际安装对象，卸载时移除相同对象；不调用 ClearAllMappings 影响其他系统。
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InstalledInputSubsystem;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputMappingContext>> InstalledMappingContexts;
	// 基础映射引用现有资产；战斗映射由本 Controller 实例初始化，按键解释也在这里。
	// 移动、跳跃与躲闪的基础映射，可在 Controller 蓝图替换。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// 鼠标视角映射，与通用 Look 输入分开。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MouseLookMappingContext;

	// 运行实例创建的战斗键位表，引用与 BindAction 相同的动作对象；不保存到蓝图。
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> RuntimeCombatMappingContext;

	// 跳跃：开始 Jump，松开 StopJumping。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	// 二维移动：触发时传角色，完成/取消时清理移动意图。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	// 通用二维视角输入。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// 鼠标二维视角输入。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MouseLookAction;

	// 左键：按下计时，短按松开发轻击，到阈值发重击。
	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackHoldAction;

	// 躲闪请求；方向由当前角色的移动意图决定。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> DodgeAction;

	// 格挡按下与松开配对，取消也结束格挡。
	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> BlockAction;

	// 弹反请求。
	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ParryAction;

	// 处决请求。
	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ExecutionAction;

	// 切换锁定。
	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LockOnAction;

	// 切换左侧目标。
	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SwitchTargetLeftAction;

	// 切换右侧目标。
	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SwitchTargetRightAction;

	// F1 开发 HUD；回调转给当前角色的开发组件。
	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> DeveloperDebugHUDAction;

	// 短按与长按的判定配置属于 Controller，在 BP_SCLPlayerController 的 Class Defaults 中调整。
	UPROPERTY(EditDefaultsOnly, Category="Input|Attack", meta=(ClampMin="0.05", Units="s", DisplayName="重击长按阈值"))
	float HeavyAttackHoldThreshold{0.25F};
	// 每次回调读取当前 Pawn，不长期保存角色引用，确保重生后控制新角色。
	void HandleJumpStarted();
	void HandleJumpReleased();
	void Move(const FInputActionValue& InputValue);
	void ClearMovementInput();
	void HandleDodge();
	void HandleBlockStarted();
	void HandleBlockReleased();
	void HandleParry();
	void HandleExecution();
	void HandleLockOn();
	void HandleSwitchTargetLeft();
	void HandleSwitchTargetRight();
	void DebugToggleDeveloperHUD();
	void Look(const FInputActionValue& InputValue);
	void HandleHeavyHoldThreshold();
	// Pawn 被卸载或输入取消时清理状态，避免旧计时器攻击新 Pawn。
	void ResetAttackHold();
	// 菜单流程由 DemoSubsystem 决定，Controller 只把热键转为请求。
	void Primary();
	void PauseDemo();
	void Retry();
	// 三者一起由 ResetAttackHold 清理，避免已松开或换 Pawn 后仍触发重击。
	FTimerHandle HeavyHoldTimer;
	bool bAttackButtonDown{false};
	// 为 true 时松开不能再补轻击；一次按住最多触发一次重击。
	bool bHeavyHoldTriggered{false};
};
