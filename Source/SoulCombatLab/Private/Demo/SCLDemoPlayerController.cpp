#include "Demo/SCLDemoPlayerController.h"
#include "SoulCombatLab.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "UObject/ConstructorHelpers.h"
#include "Targeting/SCLTargetingComponent.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"

/** 输入配置集中在 Controller；不依赖 Character 的私有字段。 */
ASCLDemoPlayerController::ASCLDemoPlayerController()
{
	// 1. 创建按下/松开类型的 Boolean 动作，并在本 Controller 的战斗映射中指定键位。
	// 动作只表达输入值，短按/长按解释与 BindAction 回调在 Controller 中。
	AttackHoldAction = CreateDefaultSubobject<UInputAction>(TEXT("AttackHoldAction"));
	AttackHoldAction->ValueType = EInputActionValueType::Boolean;
	BlockAction = CreateDefaultSubobject<UInputAction>(TEXT("BlockAction"));
	BlockAction->ValueType = EInputActionValueType::Boolean;
	ParryAction = CreateDefaultSubobject<UInputAction>(TEXT("ParryAction"));
	ParryAction->ValueType = EInputActionValueType::Boolean;
	ExecutionAction = CreateDefaultSubobject<UInputAction>(TEXT("ExecutionAction"));
	ExecutionAction->ValueType = EInputActionValueType::Boolean;
	LockOnAction = CreateDefaultSubobject<UInputAction>(TEXT("LockOnAction"));
	LockOnAction->ValueType = EInputActionValueType::Boolean;
	SwitchTargetLeftAction = CreateDefaultSubobject<UInputAction>(TEXT("SwitchTargetLeftAction"));
	SwitchTargetLeftAction->ValueType = EInputActionValueType::Boolean;
	SwitchTargetRightAction = CreateDefaultSubobject<UInputAction>(TEXT("SwitchTargetRightAction"));
	SwitchTargetRightAction->ValueType = EInputActionValueType::Boolean;
	DeveloperDebugHUDAction = CreateDefaultSubobject<UInputAction>(TEXT("DeveloperDebugHUDAction"));
	DeveloperDebugHUDAction->ValueType = EInputActionValueType::Boolean;
	// 2. 复用现有移动、视角、跳跃和躲闪输入资产；蓝图子类可覆盖这些引用。
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContext(
		TEXT("/Game/Input/IMC_Default.IMC_Default"));
	DefaultMappingContext = DefaultContext.Object;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MouseContext(
		TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	MouseLookMappingContext = MouseContext.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> JumpInput(
		TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
	JumpAction = JumpInput.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveInput(
		TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	MoveAction = MoveInput.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> LookInput(
		TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
	LookAction = LookInput.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> MouseLookInput(
		TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
	MouseLookAction = MouseLookInput.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> DodgeInput(
		TEXT("/Game/Input/Actions/IA_Dodge.IA_Dodge"));
	DodgeAction = DodgeInput.Object;
}

/**
 * 动作定义可以来自默认对象，但映射必须使用本实例实际绑定的动作指针。
 * 蓝图派生类的默认子对象复制可能保留 CDO 内部引用，不能直接安装构造函数中的映射图。
 */
void ASCLDemoPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	RuntimeCombatMappingContext = NewObject<UInputMappingContext>(this, TEXT("ActiveCombatMappingContext"));
	RuntimeCombatMappingContext->MapKey(AttackHoldAction, EKeys::LeftMouseButton);
	RuntimeCombatMappingContext->MapKey(BlockAction, EKeys::RightMouseButton);
	RuntimeCombatMappingContext->MapKey(ParryAction, EKeys::Q);
	RuntimeCombatMappingContext->MapKey(ExecutionAction, EKeys::E);
	RuntimeCombatMappingContext->MapKey(LockOnAction, EKeys::MiddleMouseButton);
	RuntimeCombatMappingContext->MapKey(SwitchTargetLeftAction, EKeys::MouseScrollDown);
	RuntimeCombatMappingContext->MapKey(SwitchTargetRightAction, EKeys::MouseScrollUp);
	RuntimeCombatMappingContext->MapKey(DeveloperDebugHUDAction, EKeys::F1);
}

/**
 * 绑定 Enter、Escape、R 菜单热键，并允许这些热键在暂停时执行。
 * 角色动作也绑定到本 Controller 的 Enhanced Input 组件；换 Pawn 无需重复绑定。
 */
void ASCLDemoPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (auto* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		BindPlayerActions(EnhancedInput);
	}
	else
	{
		UE_LOG(LogSoulCombatLab, Error, TEXT("Player Controller requires UEnhancedInputComponent."));
	}
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ASCLDemoPlayerController::Primary).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASCLDemoPlayerController::PauseDemo).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ASCLDemoPlayerController::Retry).bExecuteWhenPaused = true;
}
/**
 * 把 Enter 转成 Demo 主操作；标题开始、失败重试或胜利重开由 Demo 状态决定。
 */
void ASCLDemoPlayerController::Primary()
{
	if (auto* Demo = GetWorld()->GetSubsystem<USCLDemoSubsystem>()) Demo->PrimaryAction();
}
/**
 * 把 Escape 转成流程暂停切换，Controller 不在此清理或重建关卡对象。
 */
void ASCLDemoPlayerController::PauseDemo()
{
	if (auto* Demo = GetWorld()->GetSubsystem<USCLDemoSubsystem>()) Demo->TogglePause();
}
/**
 * R 仅在失败状态触发主操作，避免正常战斗中误重建玩家。
 */
void ASCLDemoPlayerController::Retry()
{
	if (auto* Demo = GetWorld()->GetSubsystem<USCLDemoSubsystem>(); Demo != nullptr && Demo->GetState() == ESCLDemoState::Defeat)
		Demo->PrimaryAction();
}

/**
 * 左键 Started 回调：重置旧长按状态并启动计时。
 * 此时不发轻击；短按是在松开时确认，重击是在计时到点时确认。
 */
void ASCLDemoPlayerController::HandleAttackPressed()
{
	// 新一次按下先清掉旧状态，避免暂停、失焦后残留的计时器影响这次输入。
	ResetAttackHold();
	bAttackButtonDown = true;
	if (const ASCLPlayerCharacter* const PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		GetWorldTimerManager().SetTimer(HeavyHoldTimer, this,
			&ASCLDemoPlayerController::HandleHeavyHoldThreshold, HeavyAttackHoldThreshold, false);
	}
}

/**
 * 左键 Completed 回调：停计时；尚未触发重击时请求一次轻击。
 * 已经触发重击的松开不会再追加轻击。
 */
void ASCLDemoPlayerController::HandleAttackReleased()
{
	// 阈值未到就松开算轻击；重击已经在计时器到点时发出，不再补轻击。
	GetWorldTimerManager().ClearTimer(HeavyHoldTimer);
	if (bAttackButtonDown && !bHeavyHoldTriggered)
	{
		if (ASCLPlayerCharacter* const PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
		{
			PlayerCharacter->RequestLightAttack();
		}
	}
	bAttackButtonDown = false;
}

/**
 * 输入动作被取消时只清理长按状态，不把取消当作短按松开。
 */
void ASCLDemoPlayerController::HandleAttackCanceled()
{
	ResetAttackHold();
}

/**
 * 长按计时器回调；到点只发一次重击。
 * 回调查询当前 Pawn，实际能否出招仍由 GAS 和连招组件判断。
 */
void ASCLDemoPlayerController::HandleHeavyHoldThreshold()
{
	// 这里只解释本地按键，是否真的能出招仍交给 Character、GAS 和 Combo 判断。
	if (bAttackButtonDown && !bHeavyHoldTriggered)
	{
		bHeavyHoldTriggered = true;
		if (ASCLPlayerCharacter* const PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
		{
			PlayerCharacter->RequestHeavyAttack();
		}
	}
}

/**
 * 同时清理计时器、按下标记和已触发标记，保持三者一致。
 */
void ASCLDemoPlayerController::ResetAttackHold()
{
	GetWorldTimerManager().ClearTimer(HeavyHoldTimer);
	bAttackButtonDown = false;
	bHeavyHoldTriggered = false;
}

/**
 * 失去 Pawn 时废弃旧长按计时，避免重生后旧输入落到新角色上。
 */
void ASCLDemoPlayerController::OnUnPossess()
{
	// 失败重生时 Controller 会换 Pawn；旧的长按计时器不能把攻击发给新角色。
	ResetAttackHold();
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->ClearMovementInput();
		PlayerCharacter->StopJumping();
		PlayerCharacter->StopBlock();
	}
	RemoveLocalInputMappings();
	Super::OnUnPossess();
}


// ===== 当前玩家的输入绑定和按键解释 =====
/**
 * 绑定 Controller 自己的动作资产；每次回调通过 GetPawn 查询当前角色。
 * Started 用于按下，Completed 用于正常松开，Canceled 用于输入取消。
 * 移动、攻击等需要收尾的动作同时绑定取消回调。
 */
void ASCLDemoPlayerController::BindPlayerActions(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (JumpAction != nullptr)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ASCLDemoPlayerController::HandleJumpStarted);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ASCLDemoPlayerController::HandleJumpReleased);
	}

	if (MoveAction != nullptr)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASCLDemoPlayerController::Move);
		EnhancedInputComponent->BindAction(
			MoveAction,
			ETriggerEvent::Completed,
			this,
			&ASCLDemoPlayerController::ClearMovementInput);
		EnhancedInputComponent->BindAction(
			MoveAction,
			ETriggerEvent::Canceled,
			this,
			&ASCLDemoPlayerController::ClearMovementInput);
	}

	if (LookAction != nullptr)
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASCLDemoPlayerController::Look);
	}

	if (MouseLookAction != nullptr)
	{
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ASCLDemoPlayerController::Look);
	}

	if (AttackHoldAction != nullptr)
	{
		EnhancedInputComponent->BindAction(AttackHoldAction, ETriggerEvent::Started, this, &ASCLDemoPlayerController::HandleAttackPressed);
		EnhancedInputComponent->BindAction(AttackHoldAction, ETriggerEvent::Completed, this, &ASCLDemoPlayerController::HandleAttackReleased);
		EnhancedInputComponent->BindAction(AttackHoldAction, ETriggerEvent::Canceled, this, &ASCLDemoPlayerController::HandleAttackCanceled);
	}

	if (DodgeAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			DodgeAction,
			ETriggerEvent::Started,
			this,
			&ASCLDemoPlayerController::HandleDodge);
	}

	if (BlockAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			BlockAction,
			ETriggerEvent::Started,
			this,
			&ASCLDemoPlayerController::HandleBlockStarted);
		EnhancedInputComponent->BindAction(
			BlockAction,
			ETriggerEvent::Completed,
			this,
			&ASCLDemoPlayerController::HandleBlockReleased);
		EnhancedInputComponent->BindAction(
			BlockAction,
			ETriggerEvent::Canceled,
			this,
			&ASCLDemoPlayerController::HandleBlockReleased);
	}

	if (ParryAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			ParryAction,
			ETriggerEvent::Started,
			this,
			&ASCLDemoPlayerController::HandleParry);
	}

	if (ExecutionAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			ExecutionAction,
			ETriggerEvent::Started,
			this,
			&ASCLDemoPlayerController::HandleExecution);
	}

	if (LockOnAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			LockOnAction,
			ETriggerEvent::Started,
			this,
			&ASCLDemoPlayerController::HandleLockOn);
	}

	if (SwitchTargetLeftAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			SwitchTargetLeftAction,
			ETriggerEvent::Started,
			this,
			&ASCLDemoPlayerController::HandleSwitchTargetLeft);
	}

	if (SwitchTargetRightAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			SwitchTargetRightAction,
			ETriggerEvent::Started,
			this,
			&ASCLDemoPlayerController::HandleSwitchTargetRight);
	}

	if (DeveloperDebugHUDAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			DeveloperDebugHUDAction,
			ETriggerEvent::Started,
			this,
			&ASCLDemoPlayerController::DebugToggleDeveloperHUD);
	}
}

// 按键回调只解释当前输入；执行角色动作时获取当前 Pawn，避免保留重生前的角色引用。
/**
 * 按下跳跃时调用当前 Pawn 的 Jump，由 CharacterMovement 执行跳跃。
 */
void ASCLDemoPlayerController::HandleJumpStarted()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->Jump();
	}
}

/**
 * 松开跳跃时调用 StopJumping，结束持续的跳跃按住意图。
 */
void ASCLDemoPlayerController::HandleJumpReleased()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StopJumping();
	}
}

/**
 * 将 Enhanced Input 的二维数值原样传给角色；角色再按视角转换成世界方向。
 */
void ASCLDemoPlayerController::Move(const FInputActionValue& InputValue)
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->MoveInViewDirection(InputValue.Get<FVector2D>());
	}
}

/**
 * Completed 或 Canceled 都清除角色缓存的移动意图。
 */
void ASCLDemoPlayerController::ClearMovementInput()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->ClearMovementInput();
	}
}

/**
 * 按下躲闪键时请求当前角色的 GAS 躲闪技能。
 */
void ASCLDemoPlayerController::HandleDodge()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->RequestDodge();
	}
}

/**
 * 格挡键按下时请求格挡技能，与松开回调成对。
 */
void ASCLDemoPlayerController::HandleBlockStarted()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StartBlock();
	}
}

/**
 * 格挡键松开或取消时结束当前格挡技能。
 */
void ASCLDemoPlayerController::HandleBlockReleased()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StopBlock();
	}
}

/**
 * 按下弹反键时向角色发起弹反请求。
 */
void ASCLDemoPlayerController::HandleParry()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->RequestParry();
	}
}

/**
 * 按下处决键时向角色发起处决请求，技能负责核对目标条件。
 */
void ASCLDemoPlayerController::HandleExecution()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->RequestExecution();
	}
}

/**
 * 切换当前角色的锁定状态；目标筛选和相机恢复归 TargetingComponent。
 */
void ASCLDemoPlayerController::HandleLockOn()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		if (auto* Targeting = PlayerCharacter->GetTargetingComponent())
		{
			Targeting->ToggleLock();
		}
	}
}

/**
 * 把滚轮动作交给锁定组件，尝试切换到左侧候选目标。
 */
void ASCLDemoPlayerController::HandleSwitchTargetLeft()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		if (auto* Targeting = PlayerCharacter->GetTargetingComponent())
		{
			Targeting->SwitchTargetLeft();
		}
	}
}

/**
 * 把滚轮动作交给锁定组件，尝试切换到右侧候选目标。
 */
void ASCLDemoPlayerController::HandleSwitchTargetRight()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		if (auto* Targeting = PlayerCharacter->GetTargetingComponent())
		{
			Targeting->SwitchTargetRight();
		}
	}
}

/**
 * F1 通过角色转发到开发组件，切换开发 HUD 的显示。
 */
void ASCLDemoPlayerController::DebugToggleDeveloperHUD()
{
	if (auto* PlayerCharacter = Cast<ASCLPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->DebugToggleDeveloperHUD();
	}
}

/**
 * 直接更新 Controller 的 Yaw/Pitch；相机组件读取控制旋转形成视角。
 */
void ASCLDemoPlayerController::Look(const FInputActionValue& InputValue)
{
	const FVector2D LookInput = InputValue.Get<FVector2D>();
	AddYawInput(LookInput.X);
	AddPitchInput(LookInput.Y);
}

/** 服务端占有（含单机）接入映射；本地客户端由 AcknowledgePossession 接入。 */
void ASCLDemoPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	InstallLocalInputMappings();
}

/** 客户端确认新的 Pawn 时刷新映射，不依赖仅在服务端调用的 OnPossess。 */
void ASCLDemoPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	InstallLocalInputMappings();
}

/** 安装本地映射前移除旧安装；切换 Pawn 时输入资产本身保持不变。 */
void ASCLDemoPlayerController::InstallLocalInputMappings()
{
	RemoveLocalInputMappings();
	if (!IsLocalController() || !Cast<ASCLPlayerCharacter>(GetPawn()) || !GetLocalPlayer()) return;
	auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem) return;
	InstalledInputSubsystem = Subsystem;
	const auto Add = [this, Subsystem](UInputMappingContext* Context, const int32 Priority)
	{
		if (!Context) return;
		Subsystem->AddMappingContext(Context, Priority);
		InstalledMappingContexts.AddUnique(Context);
	};
	Add(DefaultMappingContext, 0);
	Add(MouseLookMappingContext, 0);
	Add(RuntimeCombatMappingContext, 1);
}

/** 只移除本 Controller 安装的映射；保存的弱引用在失去 Pawn 后仍可用于卸载。 */
void ASCLDemoPlayerController::RemoveLocalInputMappings()
{
	if (auto* Subsystem = InstalledInputSubsystem.Get())
	{
		for (UInputMappingContext* Context : InstalledMappingContexts) Subsystem->RemoveMappingContext(Context);
	}
	InstalledMappingContexts.Reset();
	InstalledInputSubsystem.Reset();
}

/** Controller 退出世界时收尾计时器与本地映射。 */
void ASCLDemoPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetAttackHold();
	RemoveLocalInputMappings();
	Super::EndPlay(EndPlayReason);
}
