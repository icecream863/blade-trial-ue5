#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystem/Abilities/SCLDodgeAbility.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/SCLPlayerAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyStates/ANS_SCLInvincible.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Demo/SCLDemoMap.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Targeting/SCLTargetingComponent.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

// 真实游戏玩家上的回归：查实际播放动作，不以“配置表里存在八项”代替验证。
// 使用 Montage 播放位置抽样窗口，避免启动着色器导致墙钟时间与动画时间脱节。
class FSCLDirectionalDodgeCommand final : public IAutomationLatentCommand
{
public:
	explicit FSCLDirectionalDodgeCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 120) { Test->AddError(TEXT("Directional dodge timed out")); return true; }
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game) World = Context.World();
		auto* Demo = World ? World->GetSubsystem<USCLDemoSubsystem>() : nullptr;
		if (!Demo || !Demo->IsActive()) return false;
		if (!Player.IsValid())
		{
			Demo->RestartRun();
			Player = Demo->GetPlayer();
			Player->SetActorLocation(ASCLDemoMap::Checkpoint(ESCLDemoStage::Training));
			Demo->CheckRegion(); Demo->PrimaryAction();
			Player->SetActorLocation(ASCLDemoMap::Checkpoint(ESCLDemoStage::Sword));
			Demo->CheckRegion();
			AActor* Target = Demo->GetOpponent();
			if (!Target || !Player->GetMesh()->GetAnimInstance()) { Test->AddError(TEXT("Dodge fixture missing")); return true; }
			if (auto* Pawn = Cast<APawn>(Target))
				if (auto* AI = Cast<AAIController>(Pawn->GetController()))
				{ AI->StopMovement(); if (AI->BrainComponent) AI->BrainComponent->StopLogic(TEXT("Dodge fixture")); }
			Anchor = Player->GetActorLocation();
			Target->SetActorLocation(Anchor + FVector{700,0,0});
			Player->SetActorRotation(FRotator::ZeroRotator);
			Player->GetController()->SetControlRotation(FRotator::ZeroRotator);
			Player->GetTargetingComponent()->ToggleLock();
			Test->TestTrue(TEXT("Dodge fixture has a locked enemy"), Player->GetTargetingComponent()->IsLockedOn());
			MeshTransform = Player->GetMesh()->GetRelativeTransform();
			RootMode = Player->GetMesh()->GetAnimInstance()->RootMotionMode;
			return false;
		}
		auto* ASC = Player->GetSCLAbilitySystemComponent();
		auto* Anim = Player->GetMesh()->GetAnimInstance();
		static const FVector2D Inputs[] = {{0,1},{1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1}};
		static const TCHAR* Names[] = {TEXT("F"),TEXT("FR"),TEXT("R"),TEXT("BR"),TEXT("B"),TEXT("BL"),TEXT("L"),TEXT("FL")};
		const int32 Direction = Index % 8; // 第九次在无敌窗口内主动中断。
		if (Step == 0)
		{
			Player->SetActorLocation(Anchor);
			Player->SetActorRotation(FRotator::ZeroRotator);
			Player->GetController()->SetControlRotation(FRotator::ZeroRotator);
			Player->GetCharacterMovement()->StopMovementImmediately();
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100);
			Player->MoveInViewDirection(Inputs[Direction]);
			Desired = Player->GetDesiredDodgeDirection();
			Player->RequestDodge(); Player->ClearMovementInput();
			Montage = Anim->GetCurrentActiveMontage();
			Test->TestNotNull(TEXT("Dodge actually starts a montage"), Montage.Get());
			if (!Montage.IsValid()) return true;
			Test->TestEqual(TEXT("Actual direction montage"), Montage->GetName(), FString(TEXT("AM_PlayerDodge_")) + Names[Direction]);
			Test->TestEqual(TEXT("Dodge costs stamina once"), ASC->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()), 88.0F);
			Test->TestFalse(TEXT("No iframe at frame zero"), ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Invincible));
			int32 Windows = 0;
			for (const FAnimNotifyEvent& Event : Montage->Notifies)
				if (Cast<UANS_SCLInvincible>(Event.NotifyStateClass))
				{
					++Windows;
					Test->TestTrue(TEXT("Iframe asset starts at .10 and ends at .60"),
						FMath::IsNearlyEqual(Event.GetTime(),0.10F,0.005F) && FMath::IsNearlyEqual(Event.GetDuration(),0.50F,0.005F));
				}
			Test->TestEqual(TEXT("Exactly one iframe notify"), Windows, 1);
			FootStart = Player->GetMesh()->GetBoneLocation(TEXT("foot_l"), EBoneSpaces::ComponentSpace);
			Step = 1; Middle = Late = false; RecoveryFrame = 0;
			return false;
		}
		if (Step == 2)
		{
			if (World->GetTimeSeconds() < Next) return false;
			Test->TestFalse(TEXT("Cancel releases iframe"), ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Invincible));
			Test->TestFalse(TEXT("Cancel releases dodging state"), ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Dodging));
			Test->TestTrue(TEXT("Cancel restores root policy"), Anim->RootMotionMode == RootMode);
			Test->TestTrue(TEXT("Cancel stops capsule displacement"), FVector::Dist(CancelLocation,Player->GetActorLocation()) < 5);
			Test->TestTrue(TEXT("Cancel retains lock"), Player->GetTargetingComponent()->IsLockedOn());
			const float Health = ASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			Player->TakeDamage(10, FDamageEvent(), nullptr, Demo->GetOpponent());
			Test->TestTrue(TEXT("Damage works again after cancel"), ASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()) < Health);
			return true;
		}
		const float Position = Anim->Montage_GetPosition(Montage.Get());
		if (Index == 0 && Position > 0.70F && HeldInputProbe == 0)
		{
			Player->MoveInViewDirection(FVector2D(1, 0));
			HeldInputProbe = 1;
			return false;
		}
		if (Index == 0 && HeldInputProbe == 1)
		{
			const auto* InputsAnim = Cast<USCLPlayerAnimInstance>(Anim);
			const FVector Intent = Player->GetWorldMovementIntent();
			Test->TestTrue(TEXT("Held movement prepares matching base pose during dodge"),
				FMath::IsNearlyEqual(InputsAnim->LocalForwardSpeed, FVector::DotProduct(Intent, Player->GetActorForwardVector()), 0.02F) &&
				FMath::IsNearlyEqual(InputsAnim->LocalRightSpeed, FVector::DotProduct(Intent, Player->GetActorRightVector()), 0.02F));
			Player->ClearMovementInput();
			HeldInputProbe = 2;
		}
		if (!EndSample && Position > Montage->GetPlayLength() - 0.18F && ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Dodging))
		{
			EndSample = true;
			const auto* InputsAnim = Cast<USCLPlayerAnimInstance>(Anim);
			Test->TestTrue(TEXT("Released input blends dodge back to idle instead of forced-motion run"),
				FMath::Abs(InputsAnim->LocalForwardSpeed) < 0.01F && FMath::Abs(InputsAnim->LocalRightSpeed) < 0.01F);
			// 故意与胶囊速度不同：动作仍移动，基础姿势已经准备好结束后的待机。
			Test->AddInfo(FString::Printf(TEXT("Dodge end sample %s: velocity=%.2f base=(%.3f,%.3f)"), Names[Direction],
				Player->GetVelocity().Size2D(), InputsAnim->LocalForwardSpeed, InputsAnim->LocalRightSpeed));
		}
		if (!Middle && Position >= 0.30F && Position < 0.55F)
		{
			Middle = true;
			const float Health = ASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			Player->TakeDamage(10, FDamageEvent(), nullptr, Demo->GetOpponent());
			Test->TestEqual(TEXT("Actual damage is rejected during iframe"), ASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()), Health);
			Test->TestTrue(TEXT("Middle of dodge is invincible"), ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Invincible));
			Test->TestTrue(TEXT("Dodge bones animate"), FVector::Dist(FootStart,Player->GetMesh()->GetBoneLocation(TEXT("foot_l"), EBoneSpaces::ComponentSpace)) > 2);
			Test->TestTrue(TEXT("Mesh transform is not artificially rotated"), MeshTransform.Equals(Player->GetMesh()->GetRelativeTransform(),0.01F));
			Test->TestTrue(TEXT("Lock does not rotate character during side/back dodge"), FMath::Abs(Player->GetActorRotation().Yaw) < 0.5F);
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Automation/Dodge-%s.png"),Names[Direction]),false,false);
			if (Index == 8)
			{
				FGameplayTagContainer Tags(SCLGameplayTags::Ability_Dodge);
				ASC->CancelAbilities(&Tags);
				CancelLocation = Player->GetActorLocation(); Next = World->GetTimeSeconds() + 0.15F; Step = 2;
				return false;
			}
		}
		// Montage 位置在动画更新时前进，Notify 分发可以在同帧稍后完成。
		// 首次截图可能让时间一步越过窗口；等待下一帧再检查已处理完的结束通知。
		if (!RecoveryFrame && Position >= 0.65F) RecoveryFrame = GFrameCounter;
		if (!Late && RecoveryFrame && GFrameCounter > RecoveryFrame)
		{ Late = true; Test->TestFalse(FString::Printf(TEXT("%s recovery is vulnerable at %.3f"),Names[Direction],Position), ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Invincible)); }
		if (ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Dodging)) return false;
		Test->TestTrue(TEXT("Runtime sampled iframe middle and recovery; startup checked at activation"), Middle && Late);
		const FVector Travel = Player->GetActorLocation() - Anchor;
		Test->TestTrue(TEXT("Capsule travels toward requested direction"), FVector::DotProduct(Travel,Desired) > 400 && Travel.Size2D() < 500);
		Test->TestTrue(TEXT("Capsule has no doubled root displacement"), (Travel - Desired * FVector::DotProduct(Travel,Desired)).Size2D() < 10);
		Test->TestTrue(TEXT("Normal end restores root policy"), Anim->RootMotionMode == RootMode);
		Test->TestFalse(TEXT("Normal end releases iframe"), ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Invincible));
		Test->AddInfo(FString::Printf(TEXT("Dodge %s travel %.1f cm"),Names[Direction],Travel.Size2D()));
		++Index; Step = 0; EndSample = false;
		return false;
	}
private:
	FAutomationTestBase* Test;
	double Started{FPlatformTime::Seconds()};
	TWeakObjectPtr<ASCLPlayerCharacter> Player;
	TWeakObjectPtr<UAnimMontage> Montage;
	FVector Anchor, Desired, FootStart, CancelLocation;
	FTransform MeshTransform;
	ERootMotionMode::Type RootMode{ERootMotionMode::RootMotionFromMontagesOnly};
	float Next{0};
	int32 Index{0}, Step{0};
	uint64 RecoveryFrame{0};
	bool Middle{false}, Late{false};
	bool EndSample{false};
	int32 HeldInputProbe{0};
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLDirectionalDodgeTest,"SoulCombatLabCombat.DirectionalDodge",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSCLDirectionalDodgeTest::RunTest(const FString& Parameters)
{
	for (int32 Index=0;Index<8;++Index)
	{
		const FVector Direction = FRotator(0,Index*45.0F,0).Vector();
		TestTrue(TEXT("Eight direction selection"), static_cast<int32>(USCLDodgeAbility::SelectDodgeDirection(Direction,FRotator::ZeroRotator)) == Index);
		TestTrue(TEXT("Selection uses character facing"), static_cast<int32>(USCLDodgeAbility::SelectDodgeDirection(FRotator(0,90,0).RotateVector(Direction),FRotator(0,90,0))) == Index);
	}
	ADD_LATENT_AUTOMATION_COMMAND(FSCLDirectionalDodgeCommand(this));
	return true;
}
#endif
