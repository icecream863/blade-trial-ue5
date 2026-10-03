#include "Animation/SCLPlayerAnimInstance.h"

#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/Player/SCLWeaponPresentationComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Targeting/SCLTargetingComponent.h"

void USCLPlayerAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const ASCLPlayerCharacter* const Player = Cast<ASCLPlayerCharacter>(TryGetPawnOwner());
	if (!Player) return;
	// 物理先决定角色是否离地，动画只表现结果，不在这里重新执行 Jump 或移动胶囊。
	bIsFalling = Player->GetCharacterMovement()->IsFalling();
	VerticalSpeed = Player->GetVelocity().Z;
	GroundSpeed = Player->GetVelocity().Size2D();

	const USCLAbilitySystemComponent* const ASC = Player->GetSCLAbilitySystemComponent();
	const bool bDodging = ASC && ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Dodging);
	const float MaxSpeed = FMath::Max(Player->GetCharacterMovement()->GetMaxSpeed(), 1.0F);
	// Slot 下的基础姿势会参与 Montage 混出。闪避的强制位移不是普通跑步速度：
	// 松开按键应混回待机，按住方向应预备对应步伐，避免先混回跑步再突然停下。
	// 结束后的第一帧也沿用意图，隔离 RootMotionSource 清理时的残余速度。
	const FVector Velocity = (bDodging || bWasDodging)
		? Player->GetWorldMovementIntent() * MaxSpeed : Player->GetVelocity();
	bWasDodging = bDodging;
	// 使用角色自身坐标，锁定后虽然始终面向敌人，后退仍得到负的前向速度。
	LocalForwardSpeed = FMath::Clamp(FVector::DotProduct(Velocity, Player->GetActorForwardVector()) / MaxSpeed, -1.0F, 1.0F);
	LocalRightSpeed = FMath::Clamp(FVector::DotProduct(Velocity, Player->GetActorRightVector()) / MaxSpeed, -1.0F, 1.0F);
	bLockedOn = Player->GetTargetingComponent() && Player->GetTargetingComponent()->IsLockedOn();
	bWeaponSheathed = Player->GetWeaponPresentationComponent() && Player->GetWeaponPresentationComponent()->IsSheathed();
	bBlocking = ASC && ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Blocking);
}
