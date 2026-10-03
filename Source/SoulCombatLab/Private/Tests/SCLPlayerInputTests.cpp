#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Demo/SCLDemoPlayerController.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Combat/SCLCombatComponent.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputKeyEventArgs.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"

// 验证职责迁移后 Controller 的短按、长按及取消，避免只验证直接调用角色攻击。
class FSCLPlayerInputCommand final : public IAutomationLatentCommand
{
public:
	explicit FSCLPlayerInputCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 20.0) { Test->AddError(TEXT("Player input test timed out")); return true; }
		if (Now < Next) return false;
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game) World = Context.World();
		if (!World) return false;
		auto* Demo = World->GetSubsystem<USCLDemoSubsystem>();
		if (!Demo || !Demo->IsActive()) return false;
		if (Demo->GetState() == ESCLDemoState::Title) Demo->PrimaryAction();
		auto* Controller = Cast<ASCLDemoPlayerController>(World->GetFirstPlayerController());
		auto* Character = Controller ? Cast<ASCLPlayerCharacter>(Controller->GetPawn()) : nullptr;
		if (!Character) return false;
		auto* Combat = Character->GetCombatComponent_Implementation();
		auto* ASC = Character->GetSCLAbilitySystemComponent();
		if (!Combat || !ASC) { Test->AddError(TEXT("Missing player combat components")); return true; }
		if (Step == 0)
		{
			Combat->CancelActiveAttack();
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Controller->HandleAttackPressed();
			Controller->HandleAttackReleased();
			Test->TestEqual(TEXT("Controller short press starts light opener"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_1")));
			Combat->CancelActiveAttack();
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Controller->HandleAttackPressed();
			Next = Now + Controller->GetHeavyAttackHoldThreshold() + 0.08;
			Step = 1;
			return false;
		}
		if (Step == 1)
		{
			Test->TestEqual(TEXT("Controller hold starts heavy opener"), Combat->GetActiveAttackName(), FName(TEXT("JumpAttack04")));
			Controller->HandleAttackReleased();
			Test->TestEqual(TEXT("Release after heavy does not add light attack"), Combat->GetActiveAttackName(), FName(TEXT("JumpAttack04")));
			Combat->CancelActiveAttack();
			Controller->HandleAttackPressed();
			Controller->HandleAttackCanceled();
			Next = Now + Controller->GetHeavyAttackHoldThreshold() + 0.08;
			Step = 2;
			return false;
		}
		if (Step == 2)
		{
			Test->TestFalse(TEXT("Canceled hold leaves no delayed attack"), Combat->IsAttackActive());
			auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
			if (!Input) { Test->AddError(TEXT("Missing local input subsystem")); return true; }
			const auto* Mapping = Controller->GetRuntimeCombatMappingContext();
			Test->TestTrue(TEXT("Controller mapping installed"), Input->HasMappingContext(Mapping));
			Controller->UnPossess();
			Test->TestFalse(TEXT("UnPossess removes Controller mapping"), Input->HasMappingContext(Mapping));
			Controller->Possess(Character);
			Test->TestTrue(TEXT("Possess restores same Controller mapping"), Input->HasMappingContext(Mapping));
			// 等待映射重建，向 PlayerInput 注入模拟键事件，验证完整映射与绑定。
			// 不伪造物理设备 ID；此测试不覆盖 Windows 鼠标设备到 Controller 的分发。
			Step = 3; Next = Now + 0.1; return false;
		}
		if (Step == 3)
		{
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			const bool Accepted = Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.0F, 0));
			Test->AddInfo(FString::Printf(TEXT("Input injection accepted=%d ControllerInputEnabled=%d Bindings=%d"), Accepted, Controller->InputEnabled(), CastChecked<UEnhancedInputComponent>(Controller->InputComponent)->GetActionEventBindings().Num()));
			Step = 4; Next = Now + 0.05; return false;
		}
		if (Step == 4)
		{
			const auto* Action = Controller->GetRuntimeCombatMappingContext()->GetMappings()[0].Action.Get();
			Test->AddInfo(FString::Printf(TEXT("Mapped attack action=%s ValueBeforeRelease=%d"), *GetPathNameSafe(Action), CastChecked<UEnhancedPlayerInput>(Controller->PlayerInput)->GetActionValue(Action).Get<bool>()));
			Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.0F, 0));
			Step = 5; Next = Now + 0.05; return false;
		}
		Test->TestEqual(TEXT("Actual mapped left-click reaches Controller and starts light opener"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_1")));
		Combat->CancelActiveAttack();
		return true;
	}
private:
	FAutomationTestBase* Test;
	double Started{FPlatformTime::Seconds()};
	double Next{0.0};
	int32 Step{0};
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLPlayerInputTest, "SoulCombatLabCombat.PlayerInput",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSCLPlayerInputTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSCLPlayerInputCommand(this));
	return true;
}
#endif
