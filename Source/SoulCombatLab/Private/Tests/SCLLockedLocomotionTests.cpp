#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AIController.h"
#include "Animation/SCLPlayerAnimInstance.h"
#include "Animation/AnimClassInterface.h"
#include "BrainComponent.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Demo/SCLDemoMap.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Misc/Paths.h"
#include "Targeting/SCLTargetingComponent.h"
#include "UnrealClient.h"

/** 可渲染回归：锁定敌人后，八个局部移动方向均进入项目 AnimBP 并留下抽样帧。 */
class FSCLLockedLocomotionCommand final : public IAutomationLatentCommand
{
public:
	explicit FSCLLockedLocomotionCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		// 首次 GPU 截图可能触发着色器缓存/截图追踪初始化，给抽样留足墙钟时间。
		if (Now - Started > 120.0) { Test->AddError(TEXT("Locked locomotion timed out")); return true; }
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) World = Context.World();
		USCLDemoSubsystem* Demo = World ? World->GetSubsystem<USCLDemoSubsystem>() : nullptr;
		if (!Demo || !Demo->IsActive()) return false;
		if (Step == 0)
		{
			Demo->RestartRun();
			Demo->GetPlayer()->SetActorLocation(ASCLDemoMap::Checkpoint(ESCLDemoStage::Training));
			Demo->CheckRegion();
			Demo->PrimaryAction();
			Demo->GetPlayer()->SetActorLocation(ASCLDemoMap::Checkpoint(ESCLDemoStage::Sword));
			Demo->CheckRegion();
			Player = Demo->GetPlayer();
			AActor* Target = Demo->GetOpponent();
			if (!Player.IsValid() || !Target) { Test->AddError(TEXT("Locomotion fixture missing")); return true; }
			// 从实际玩家配置记录自由速度，验收恢复逻辑不能写死旧 Walk 的 180。
			FreeMoveSpeed = Player->GetCharacterMovement()->MaxWalkSpeed;
			if (APawn* TargetPawn = Cast<APawn>(Target))
			{
				if (AAIController* AI = Cast<AAIController>(TargetPawn->GetController()))
				{
					AI->StopMovement();
					if (AI->BrainComponent) AI->BrainComponent->StopLogic(TEXT("Locked locomotion sampling"));
				}
			}
			Anchor = Player->GetActorLocation();
			Target->SetActorLocation(Anchor + Player->GetActorForwardVector() * 250.0F);
			if (AController* Controller = Player->GetController()) Controller->SetControlRotation(FRotator::ZeroRotator);
			Step = 1;
			Next = Now + 0.3;
			return false;
		}
		if (!Player.IsValid()) { Test->AddError(TEXT("Player disappeared")); return true; }
		if (Step == 1)
		{
			if (Now < Next) return false;
			Player->GetTargetingComponent()->ToggleLock();
			Test->TestTrue(TEXT("Enemy is locked before direction sampling"), Player->GetTargetingComponent()->IsLockedOn());
			Test->TestTrue(TEXT("Lock speed respects configured free speed"),
				Player->GetCharacterMovement()->MaxWalkSpeed > 0.0F &&
				Player->GetCharacterMovement()->MaxWalkSpeed <= FreeMoveSpeed);
			Anim = Cast<USCLPlayerAnimInstance>(Player->GetMesh()->GetAnimInstance());
			Test->TestNotNull(TEXT("Actual player uses project locomotion AnimInstance"), Anim.Get());
			Test->AddInfo(FString::Printf(TEXT("Mesh foot_l index=%d tick=%d rate=%.2f pause=%d animMode=%d"),
				Player->GetMesh()->GetBoneIndex(TEXT("foot_l")), Player->GetMesh()->IsComponentTickEnabled(),
				Player->GetMesh()->GlobalAnimRateScale, Player->GetMesh()->bPauseAnims,
				static_cast<int32>(Player->GetMesh()->GetAnimationMode())));
			if (const IAnimClassInterface* ClassData = IAnimClassInterface::GetFromClass(Anim->GetClass()))
			{
				Test->AddInfo(FString::Printf(TEXT("Compiled AnimBP node count=%d"), ClassData->GetAnimNodeProperties().Num()));
			}
			if (!Anim.IsValid() || !Player->GetTargetingComponent()->IsLockedOn()) return true;
			Step = 2;
			PhaseStarted = Now;
			FootAtStart = Player->GetMesh()->GetBoneLocation(TEXT("foot_l"), EBoneSpaces::ComponentSpace);
			return false;
		}
        if (Step == 3)
        {
            // 先让松开格挡的收手 Montage 结束，再测纯普通走路输出。
            if (Player->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying())
            {
                PhaseStarted = Now;
                FootAtStart = Player->GetMesh()->GetBoneLocation(TEXT("foot_l"), EBoneSpaces::ComponentSpace);
                FreeFootPoseTravel = 0.0F;
                FreeMaxGroundSpeed = 0.0F;
                FreeMoveStart = Player->GetActorLocation();
                return false;
            }
            // 未锁定时也实际推进基础移动图，确认普通走路不会只移动胶囊。
            Player->AddMovementInput(Player->GetActorForwardVector(), 1.0F);
            // 循环动作可能在 0.4 秒后回到相近姿势，不能只比较首尾两帧。
            // 每帧记录组件空间中的最大脚骨变化，胶囊平移不会冒充脚步动画。
            FreeFootPoseTravel = FMath::Max(FreeFootPoseTravel, FVector::Dist(FootAtStart,
                Player->GetMesh()->GetBoneLocation(TEXT("foot_l"), EBoneSpaces::ComponentSpace)));
            FreeMaxGroundSpeed = FMath::Max(FreeMaxGroundSpeed, Anim->GroundSpeed);
            if (Now - PhaseStarted < 0.4) return false;
            Test->TestFalse(TEXT("Free walk has no locked target"), Player->GetTargetingComponent()->IsLockedOn());
            Test->AddInfo(FString::Printf(TEXT("Free walk foot pose travel %.2f cm, max ground speed %.2f cm/s, capsule travel %.2f cm"),
                FreeFootPoseTravel, FreeMaxGroundSpeed, FVector::Dist2D(FreeMoveStart, Player->GetActorLocation())));
            Test->TestTrue(TEXT("Free walk actually moves"), FreeMaxGroundSpeed > 5.0F &&
                FVector::Dist2D(FreeMoveStart, Player->GetActorLocation()) > 10.0F);
            Test->TestTrue(TEXT("Free walk feet animate"), FreeFootPoseTravel > 2.0F);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/FreeWalk.png"), true, false);
            Step = 4; Next = Now + 0.15; return false;
        }
        if (Step == 4) return Now >= Next;
		if (Index >= 8) return true;
		if (bCapturePending)
		{
			if (Now < Next) return false; // 留一帧给截图请求，之后再把胶囊放回起点。
			++Index;
			if (Index >= 8)
			{
				// 格挡与丢锁同时发生时，松开格挡仍要回到自由移动速度。
				Player->StartBlock();
				Player->GetTargetingComponent()->ClearTarget();
				Player->StopBlock();
				Test->TestTrue(TEXT("Losing lock during block restores free speed on release"),
					FMath::IsNearlyEqual(Player->GetCharacterMovement()->MaxWalkSpeed, FreeMoveSpeed));
                Player->GetCharacterMovement()->StopMovementImmediately();
                // 八向采样最后一次可能已接近场地边缘；自由移动从同一空旷起点独立开始。
                Player->SetActorLocation(Anchor);
                FreeMoveStart = Anchor;
                FootAtStart = Player->GetMesh()->GetBoneLocation(TEXT("foot_l"), EBoneSpaces::ComponentSpace);
                Step = 3; PhaseStarted = Now;
                return false;
			}
			Player->SetActorLocation(Anchor);
			Player->GetCharacterMovement()->StopMovementImmediately();
			PhaseStarted = Now;
			bCapturePending = false;
			bIntermediateCaptured = false;
			FootAtStart = Player->GetMesh()->GetBoneLocation(TEXT("foot_l"), EBoneSpaces::ComponentSpace);
			return false;
		}
		static const FVector2D Directions[] = {
			{0,1}, {1,1}, {1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1,0}, {-1,1}};
		static const TCHAR* Names[] = {
			TEXT("F"), TEXT("FR"), TEXT("R"), TEXT("BR"), TEXT("B"), TEXT("BL"), TEXT("L"), TEXT("FL")};
		const FVector2D Desired = Directions[Index];
		const FVector WorldDirection = Player->GetActorForwardVector() * Desired.Y +
			Player->GetActorRightVector() * Desired.X;
		Player->AddMovementInput(WorldDirection.GetSafeNormal(), 1.0F);
		if (!bIntermediateCaptured && Now - PhaseStarted >= 0.18)
		{
			FootAtIntermediate = Player->GetMesh()->GetBoneLocation(TEXT("foot_l"), EBoneSpaces::ComponentSpace);
			// 前进和右移再取一个相位，避免一张中间站姿被误认为整段没有迈步。
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() /
				FString::Printf(TEXT("Automation/LockedMove-%s-Early.png"), Names[Index]), true, false);
			bIntermediateCaptured = true;
		}
		if (Now - PhaseStarted < 0.4) return false;
		Test->TestTrue(FString::Printf(TEXT("%s keeps lock"), Names[Index]), Player->GetTargetingComponent()->IsLockedOn());
		Test->TestTrue(FString::Printf(TEXT("%s local forward sign"), Names[Index]),
			Desired.Y == 0 || Anim->LocalForwardSpeed * Desired.Y > 0.10F);
		Test->TestTrue(FString::Printf(TEXT("%s local right sign"), Names[Index]),
			Desired.X == 0 || Anim->LocalRightSpeed * Desired.X > 0.10F);
		if (bIntermediateCaptured)
		{
			const FVector FootAtEnd = Player->GetMesh()->GetBoneLocation(TEXT("foot_l"), EBoneSpaces::ComponentSpace);
			const float FootPoseTravel = FMath::Max(FVector::Dist(FootAtStart, FootAtIntermediate),
				FVector::Dist(FootAtIntermediate, FootAtEnd));
			Test->AddInfo(FString::Printf(TEXT("%s foot_l local pose travel %.2f cm"), Names[Index], FootPoseTravel));
			Test->TestTrue(FString::Printf(TEXT("%s feet animate while capsule moves"), Names[Index]), FootPoseTravel > 2.0F);
		}
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() /
			FString::Printf(TEXT("Automation/LockedMove-%s.png"), Names[Index]), true, false);
		bCapturePending = true;
		Next = Now + 0.10;
		return false;
	}
private:
	FAutomationTestBase* Test;
	TWeakObjectPtr<ASCLPlayerCharacter> Player;
	TWeakObjectPtr<USCLPlayerAnimInstance> Anim;
	FVector Anchor{ForceInit};
	FVector FreeMoveStart{ForceInit};
	FVector FootAtStart{ForceInit}, FootAtIntermediate{ForceInit};
	double Started{FPlatformTime::Seconds()}, Next{0.0}, PhaseStarted{0.0};
	int32 Step{0}, Index{0};
	float FreeMoveSpeed{0.0F};
	float FreeFootPoseTravel{0.0F};
	float FreeMaxGroundSpeed{0.0F};
	bool bCapturePending{false};
	bool bIntermediateCaptured{false};
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLLockedLocomotionTest, "SoulCombatLabCombat.LockedLocomotion",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSCLLockedLocomotionTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSCLLockedLocomotionCommand(this));
	return true;
}

// 跳跃属于基础移动动画：在实际玩家上检查动作阶段、原片引用、脚骨姿势与胶囊落地。
// 不给角色手动播放 Montage，不伪造动画变量；由 Jump / Falling 驱动生产动画图。
class FSCLJumpAnimationCommand final : public IAutomationLatentCommand
{
public:
 explicit FSCLJumpAnimationCommand(FAutomationTestBase* InTest) : Test(InTest) {}
 bool Update() override
 {
  if (FPlatformTime::Seconds()-Started>90) { Test->AddError(TEXT("Jump animation timed out")); return true; }
  UWorld* World=nullptr;
  for (const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Game || Context.WorldType==EWorldType::PIE) World=Context.World();
  auto* Demo=World ? World->GetSubsystem<USCLDemoSubsystem>() : nullptr;
  if(!Demo || !Demo->IsActive()) return false;
  const double Now=World->GetTimeSeconds();
  if(!Player.IsValid())
  {
   Demo->RestartRun(); Demo->PrimaryAction(); Player=Demo->GetPlayer();
   Anim=Cast<USCLPlayerAnimInstance>(Player->GetMesh()->GetAnimInstance());
   Test->TestNotNull(TEXT("Jump uses project animation class"),Anim.Get());
   if(!Anim.IsValid()) return true;
   Anchor=Player->GetActorLocation(); MeshTransform=Player->GetMesh()->GetRelativeTransform();
   Next=Now+0.3; return false;
  }
  auto* Movement=Player->GetCharacterMovement();
  const int32 Machine=Anim->GetStateMachineIndex(TEXT("PlayerLocomotion"));
  if(Machine==INDEX_NONE) { Test->AddError(TEXT("Actual player has no PlayerLocomotion state machine")); return true; }
  const FName State=Anim->GetCurrentStateName(Machine);
  if(Step==0)
  {
   if(Now<Next || !Movement->IsMovingOnGround() || State!=TEXT("Ground")) return false;
   Movement->StopMovementImmediately(); Player->ClearMovementInput(); Player->ConsumeMovementInputVector();
   FootStart=Player->GetMesh()->GetBoneLocation(TEXT("foot_l"),EBoneSpaces::ComponentSpace);
   RootStart=Player->GetMesh()->GetBoneLocation(TEXT("root"),EBoneSpaces::ComponentSpace);
   Begin=Now; MaxHeight=MaxFootTravel=MaxRootDrift=0; SeenStart=SeenLoop=SeenLand=false;
   if(Index==2)
   {
    // 直接下落：等价于离开平台的 Falling；不执行 Jump，必须跳过起跳动作。
    Player->SetActorLocation(Anchor+FVector(0,0,180)); Movement->SetMovementMode(MOVE_Falling);
   }
   else Player->Jump();
   Step=1; return false;
  }
  if(Index==1) Player->AddMovementInput(Player->GetActorForwardVector(),1.0F);
  if(Movement->IsFalling())
  {
   Player->StopJumping();
   Test->TestTrue(TEXT("Jump animation observes physical Falling"),Anim->bIsFalling);
   MaxHeight=FMath::Max(MaxHeight,Player->GetActorLocation().Z-Anchor.Z);
  }
  MaxFootTravel=FMath::Max(MaxFootTravel,FVector::Dist(FootStart,Player->GetMesh()->GetBoneLocation(TEXT("foot_l"),EBoneSpaces::ComponentSpace)));
  MaxRootDrift=FMath::Max(MaxRootDrift,FVector::Dist(RootStart,Player->GetMesh()->GetBoneLocation(TEXT("root"),EBoneSpaces::ComponentSpace)));
  SeenStart |= State==TEXT("JumpStart"); SeenLoop |= State==TEXT("JumpLoop"); SeenLand |= State==TEXT("JumpLand");
  if(Now-Begin<0.1 || Movement->IsFalling() || State!=TEXT("Ground")) return false;
  Test->TestTrue(TEXT("Actual jump has capsule height"),MaxHeight>60);
  Test->TestTrue(TEXT("Air loop and landing are visited"),SeenLoop && SeenLand);
  Test->TestTrue(TEXT("Only upward jumps use JumpStart"),Index==2 ? !SeenStart : SeenStart);
  Test->TestTrue(TEXT("Jump feet actually change component-space pose"),MaxFootTravel>5);
  Test->TestTrue(TEXT("Jump root does not double physical height"),MaxRootDrift<3);
  Test->TestTrue(TEXT("Jump does not rewrite whole Mesh transform"),MeshTransform.Equals(Player->GetMesh()->GetRelativeTransform(),0.01));
  Test->TestFalse(TEXT("Landing clears Falling animation flag"),Anim->bIsFalling);
  // 读取编译后的真实 Sequence Player，而非仅检查文件存在或动画状态名称。
  for(const FName Phase : {FName(TEXT("JumpStart")),FName(TEXT("JumpLoop")),FName(TEXT("JumpLand"))})
   Test->TestTrue(TEXT("Each jump state has a real asset player"),Anim->GetInstanceAssetPlayerIndex(TEXT("PlayerLocomotion"),Phase)!=INDEX_NONE);
  Test->AddInfo(FString::Printf(TEXT("Jump case %d: height %.1f foot change %.1f root drift %.2f"),Index,MaxHeight,MaxFootTravel,MaxRootDrift));
  Movement->StopMovementImmediately(); Player->ConsumeMovementInputVector(); Player->StopJumping();
  Player->SetActorLocation(Anchor); Movement->SetMovementMode(MOVE_Walking);
  ++Index; Step=0; Next=Now+0.2;
  return Index==3;
 }
private:
 FAutomationTestBase* Test;
 double Started{FPlatformTime::Seconds()}, Next{0}, Begin{0};
 TWeakObjectPtr<ASCLPlayerCharacter> Player;
 TWeakObjectPtr<USCLPlayerAnimInstance> Anim;
 FVector Anchor,FootStart,RootStart;
 FTransform MeshTransform;
 double MaxHeight{0},MaxFootTravel{0},MaxRootDrift{0};
 int32 Index{0},Step{0};
 bool SeenStart{false},SeenLoop{false},SeenLand{false};
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLJumpAnimationTest,"SoulCombatLabCombat.JumpAnimation",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSCLJumpAnimationTest::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FSCLJumpAnimationCommand(this)); return true;
}
#endif
