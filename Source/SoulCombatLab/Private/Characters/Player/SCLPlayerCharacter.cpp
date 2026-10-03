#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/Player/SCLWeaponPresentationComponent.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "Debug/SCLPlayerDeveloperComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "SoulCombatLab.h"
#include "AbilitySystem/Abilities/SCLBlockAbility.h"
#include "AbilitySystem/Abilities/SCLDodgeAbility.h"
#include "AbilitySystem/Abilities/SCLExecutionAbility.h"
#include "AbilitySystem/Abilities/SCLHeavyAttackAbility.h"
#include "AbilitySystem/Abilities/SCLLightAttackAbility.h"
#include "AbilitySystem/Abilities/SCLParryAbility.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Combat/SCLCombatComponent.h"
#include "Combat/SCLWeapon.h"
#include "Characters/Player/SCLPlayerComboComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/SpringArmComponent.h"
#include "Targeting/SCLTargetingComponent.h"
#include "MotionWarpingComponent.h"
#include "UObject/ConstructorHelpers.h"

// 角色装配入口：创建组件、设置默认资产，并应用玩家蓝图的外观与装备配置。
// Controller 解释按键；ComboComponent 选招；DeveloperComponent 提供控制台测试场景。

/**
 * 构造默认组件与资产引用，蓝图子类可以覆盖公开的配置。
 * 先读装备与 StartupAbilities，再读模型和镜头；这里没有运行时按键判定。
 */
ASCLPlayerCharacter::ASCLPlayerCharacter()
{
	// 1. 装配开发组件与默认武器；武器 Actor 稍后由 Combat 生成。
	WeaponPresentationComponent = CreateDefaultSubobject<USCLWeaponPresentationComponent>(TEXT("WeaponPresentationComponent"));
	DeveloperComponent = CreateDefaultSubobject<USCLPlayerDeveloperComponent>(TEXT("DeveloperComponent"));
	PlayerWeaponClass = ASCLGhostKatanaWeapon::StaticClass();
	GetCombatComponent_Implementation()->SetDefaultWeaponClass(PlayerWeaponClass);
	GetCombatComponent_Implementation()->SetWeaponAttachSocket(PlayerWeaponAttachSocket);
	// 2. 列出需要授予玩家的技能类；父类 BeginPlay 在权威端授予，并非在构造函数中立即出招。
	StartupAbilities.Add(USCLLightAttackAbility::StaticClass());
	StartupAbilities.Add(USCLHeavyAttackAbility::StaticClass());
	StartupAbilities.Add(USCLDodgeAbility::StaticClass());
	StartupAbilities.Add(USCLBlockAbility::StaticClass());
	StartupAbilities.Add(USCLParryAbility::StaticClass());
	StartupAbilities.Add(USCLExecutionAbility::StaticClass());

	// 3. 装配锁定、连招、刀鞘与镜头；组件各自管理自己的运行状态。
	TargetingComponent = CreateDefaultSubobject<USCLTargetingComponent>(TEXT("TargetingComponent"));
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
	PlayerComboComponent = CreateDefaultSubobject<USCLPlayerComboComponent>(TEXT("PlayerComboComponent"));
	ScabbardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScabbardMesh"));
	ScabbardMesh->SetupAttachment(GetMesh(), ScabbardAttachSocket);
	ScabbardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ScabbardAssetFinder(
		TEXT("/Game/GhostSamurai_Bundle/GhostSamurai/Weapon/Mesh/Katana/SM_Scabbard01.SM_Scabbard01"));
	if (ScabbardAssetFinder.Succeeded())
	{
		ScabbardAsset = ScabbardAssetFinder.Object;
		ScabbardMesh->SetStaticMesh(ScabbardAsset);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0F;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> PlayerMesh(
		TEXT("/Game/GhostSamurai_Bundle/Demo/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"));
	if (PlayerMesh.Succeeded())
	{
		PlayerSkeletalMesh = PlayerMesh.Object;
		GetMesh()->SetSkeletalMeshAsset(PlayerSkeletalMesh);
		GetMesh()->SetRelativeLocation(FVector{0.0F, 0.0F, -90.0F});
		GetMesh()->SetRelativeRotation(FRotator{0.0F, -90.0F, 0.0F});
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> PlayerAnimation(
		TEXT("/Game/SoulCombatLab/Characters/Player/Combat/ABP_SCLPlayerCombat"));
	if (PlayerAnimation.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(PlayerAnimation.Class);
	}


}

/**
 * 编辑器构造或重建角色时，将蓝图配置应用到 Mesh、刀鞘和 Combat。
 * 武器这里只设置生成类型与挂点，实际 Actor 在 Combat::BeginPlay 中生成。
 */
void ASCLPlayerCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (PlayerSkeletalMesh != nullptr)
	{
		GetMesh()->SetSkeletalMeshAsset(PlayerSkeletalMesh);
	}
	if (ScabbardMesh != nullptr)
	{
		ScabbardMesh->AttachToComponent(
			GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, ScabbardAttachSocket);
		ScabbardMesh->SetStaticMesh(ScabbardAsset);
	}
	if (USCLCombatComponent* const Combat = GetCombatComponent_Implementation())
	{
		Combat->SetDefaultWeaponClass(PlayerWeaponClass);
		Combat->SetWeaponAttachSocket(PlayerWeaponAttachSocket);
	}
}


// ===== 角色动作：接收请求，执行移动或交给 GAS =====
/**
 * Controller 将二维移动输入交给这里，X 表示左右，Y 表示前后。
 * 先缓存意图供躲闪读取；攻击、失衡或死亡时不再添加普通移动输入。
 * 方向只取镜头控制旋转的 Yaw，向上看不会让角色向空中移动。
 */
void ASCLPlayerCharacter::MoveInViewDirection(const FVector2D MovementInput)
{
	if (Controller == nullptr)
	{
		return;
	}

	// 先记住玩家意图，再检查是否允许普通移动；攻击中输入仍能决定随后躲闪的方向。
	CachedMovementInput = MovementInput;
	const USCLAbilitySystemComponent* const ASC = GetSCLAbilitySystemComponent();
	const USCLCombatComponent* const Combat = GetCombatComponent_Implementation();
	if ((Combat != nullptr && Combat->IsAttackActive()) ||
		(ASC != nullptr && (ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Attacking) ||
			ASC->HasMatchingGameplayTag(SCLGameplayTags::State_ParryAction) ||
			ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Dodging) ||
			ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Staggered) ||
			ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Dead)))) return;
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation{0.0F, ControlRotation.Yaw, 0.0F};
	const FVector ForwardDirection = FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementInput.Y);
	AddMovementInput(RightDirection, MovementInput.X);
}

/**
 * 移动按键松开或取消时清空意图；否则无输入躲闪仍可能沿旧方向出发。
 */
void ASCLPlayerCharacter::ClearMovementInput()
{
	CachedMovementInput = FVector2D::ZeroVector;
}

/**
 * 通过 Ability_Dodge 标签请求 GAS 技能，体力、状态和实际移动由躲闪技能处理。
 */
void ASCLPlayerCharacter::RequestDodge()
{
	USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	if (AbilitySystem == nullptr)
	{
		return;
	}

	FGameplayTagContainer DodgeAbilityTags;
	DodgeAbilityTags.AddTag(SCLGameplayTags::Ability_Dodge);
	AbilitySystem->TryActivateAbilitiesByTag(DodgeAbilityTags);
}

/**
 * 右键按下时请求格挡技能；持续时间由技能生命周期和右键松开共同控制。
 */
void ASCLPlayerCharacter::StartBlock()
{
	USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	if (AbilitySystem == nullptr)
	{
		return;
	}

	FGameplayTagContainer BlockAbilityTags;
	BlockAbilityTags.AddTag(SCLGameplayTags::Ability_Block);
	AbilitySystem->TryActivateAbilitiesByTag(BlockAbilityTags);
}

/**
 * 右键松开时正常结束当前格挡实例，包括配置蓝图子类；技能结束撤销状态并播放收势。
 */
void ASCLPlayerCharacter::StopBlock()
{
	USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	if (AbilitySystem == nullptr)
	{
		return;
	}

	if (FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecByBaseClass(USCLBlockAbility::StaticClass()))
		if (USCLBlockAbility* Block = Cast<USCLBlockAbility>(Spec->GetPrimaryInstance()))
			Block->ReleaseBlock();
}

/**
 * 请求弹反 Ability，弹反窗口的开启与关闭由技能负责。
 */
void ASCLPlayerCharacter::RequestParry()
{
	USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	if (AbilitySystem == nullptr)
	{
		return;
	}

	FGameplayTagContainer ParryAbilityTags;
	ParryAbilityTags.AddTag(SCLGameplayTags::Ability_Parry);
	AbilitySystem->TryActivateAbilitiesByTag(ParryAbilityTags);
}

/**
 * 请求处决 Ability；这里不选目标、不对齐位置，也不直接造成处决伤害。
 */
void ASCLPlayerCharacter::RequestExecution()
{
	USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	if (AbilitySystem == nullptr)
	{
		return;
	}

	FGameplayTagContainer ExecutionAbilityTags;
	ExecutionAbilityTags.AddTag(SCLGameplayTags::Ability_Execution);
	AbilitySystem->TryActivateAbilitiesByTag(ExecutionAbilityTags);
}

/**
 * 有移动意图时返回视角平面的单位方向；无输入或无 Controller 时使用角色正前方。
 */
FVector ASCLPlayerCharacter::GetDesiredDodgeDirection() const
{
	const FVector Intent = GetWorldMovementIntent();
	return Intent.IsNearlyZero() ? GetActorForwardVector() : Intent.GetSafeNormal();
}

FVector ASCLPlayerCharacter::GetWorldMovementIntent() const
{
	if (!Controller) return FVector::ZeroVector;

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation{0.0F, ControlRotation.Yaw, 0.0F};
	const FVector ForwardDirection = FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::Y);
	return (ForwardDirection * CachedMovementInput.Y + RightDirection * CachedMovementInput.X).GetClampedToMaxSize(1.0F);
}

/**
 * Controller 短按或蓝图调用的轻击入口。
 * 仅激活带轻击标签的 GAS 技能；技能提供 Light 参数，连招组件决定招式节点。
 */
void ASCLPlayerCharacter::RequestLightAttack()
{
	USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	USCLCombatComponent* const PlayerCombat = GetCombatComponent_Implementation();
	if (AbilitySystem == nullptr || PlayerCombat == nullptr || PlayerComboComponent == nullptr)
	{
		return;
	}
	// 刀已入鞘时先播放拔刀；组件只记住这次 Light，请求在拔刀结束后重新进到本入口。
	if (WeaponPresentationComponent->QueueAttackWhileDrawing(false)) return;

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(SCLGameplayTags::Ability_Attack_Light);
	// 标签选择轻击技能；技能内部明确传递 Light 输入，不依赖组件的临时字段。
	AbilitySystem->TryActivateAbilitiesByTag(AbilityTags);
}

/**
 * Controller 长按达到阈值或蓝图调用的重击入口。
 * 选择重击标签对应的技能；技能提供 Heavy 参数，角色不直接挑 Montage。
 */
void ASCLPlayerCharacter::RequestHeavyAttack()
{
	USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	USCLCombatComponent* const PlayerCombat = GetCombatComponent_Implementation();
	if (AbilitySystem == nullptr || PlayerCombat == nullptr || PlayerComboComponent == nullptr)
	{
		return;
	}
	// 重击同理，不提前扣费；拔刀完成才按当时体力和状态激活技能。
	if (WeaponPresentationComponent->QueueAttackWhileDrawing(true)) return;

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(SCLGameplayTags::Ability_Attack_Heavy);
	// 重击技能明确传递 Heavy 输入；与轻击共用检查和执行流程。
	AbilitySystem->TryActivateAbilitiesByTag(AbilityTags);
}

// ===== 控制台命令转发：场景实现见 Debug/SCLPlayerDeveloperComponent.cpp =====
void ASCLPlayerCharacter::DebugTestDodgeIFrame()
{
	if (DeveloperComponent) DeveloperComponent->DebugTestDodgeIFrame();
}

void ASCLPlayerCharacter::DebugTestBlock()
{
	if (DeveloperComponent) DeveloperComponent->DebugTestBlock();
}

void ASCLPlayerCharacter::DebugTestParry()
{
	if (DeveloperComponent) DeveloperComponent->DebugTestParry();
}

void ASCLPlayerCharacter::DebugTestExecution()
{
	if (DeveloperComponent) DeveloperComponent->DebugTestExecution();
}

void ASCLPlayerCharacter::DebugPrepareExecution()
{
	if (DeveloperComponent) DeveloperComponent->DebugPrepareExecution();
}

void ASCLPlayerCharacter::DebugPrepareLockOnSwitch()
{
	if (DeveloperComponent) DeveloperComponent->DebugPrepareLockOnSwitch();
}

void ASCLPlayerCharacter::DebugSpawnPerceptionEnemy()
{
	if (DeveloperComponent) DeveloperComponent->DebugSpawnPerceptionEnemy();
}

void ASCLPlayerCharacter::DebugSpawnHeavyEnemy()
{
	if (DeveloperComponent) DeveloperComponent->DebugSpawnHeavyEnemy();
}

void ASCLPlayerCharacter::DebugSpawnEnemyVariants()
{
	if (DeveloperComponent) DeveloperComponent->DebugSpawnEnemyVariants();
}

void ASCLPlayerCharacter::DebugSpawnBossPrototype(float DistanceToTarget)
{
	if (DeveloperComponent) DeveloperComponent->DebugSpawnBossPrototype(DistanceToTarget);
}

void ASCLPlayerCharacter::DebugSetBossPhaseTwo()
{
	if (DeveloperComponent) DeveloperComponent->DebugSetBossPhaseTwo();
}

void ASCLPlayerCharacter::DebugEvaluateBossUtility(float DistanceToTarget)
{
	if (DeveloperComponent) DeveloperComponent->DebugEvaluateBossUtility(DistanceToTarget);
}

void ASCLPlayerCharacter::DebugTestBossFoundation()
{
	if (DeveloperComponent) DeveloperComponent->DebugTestBossFoundation();
}

void ASCLPlayerCharacter::DebugPrepareBossCombatTest(bool bStartInPhaseTwo)
{
	if (DeveloperComponent) DeveloperComponent->DebugPrepareBossCombatTest(bStartInPhaseTwo);
}

void ASCLPlayerCharacter::DebugKillLockedTarget()
{
	if (DeveloperComponent) DeveloperComponent->DebugKillLockedTarget();
}

void ASCLPlayerCharacter::DebugToggleCombatDraw()
{
	if (DeveloperComponent) DeveloperComponent->DebugToggleCombatDraw();
}

void ASCLPlayerCharacter::DebugToggleDeveloperHUD()
{
	if (DeveloperComponent) DeveloperComponent->DebugToggleDeveloperHUD();
}
