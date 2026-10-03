#if WITH_DEV_AUTOMATION_TESTS

#include "AI/SCLAIController.h"
#include "AI/SCLAIState.h"
#include "AI/SCLBehaviorTreeBuilder.h"
#include "AI/Decorators/BTDecorator_SCLAIState.h"
#include "AI/EQS/SCLCombatPositionQuery.h"
#include "AI/EQS/SCLEnvQueryNodes.h"
#include "AI/Services/BTService_SCLUpdateCombatContext.h"
#include "AI/Tasks/BTTask_SCLBasicAttack.h"
#include "AI/Tasks/BTTask_SCLChaseTarget.h"
#include "AI/Tasks/BTTask_SCLMaintainCombatContext.h"
#include "AI/Tasks/BTTask_SCLReposition.h"
#include "AI/Tasks/BTTask_SCLRunCombatEQS.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKey.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "Characters/SCLEnemyCharacter.h"
#include "AbilitySystem/Abilities/SCLLightAttackAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Perception/AIPerceptionComponent.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Distance.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Pathfinding.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Trace.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSCLAIControllerPerceptionConfigurationTest,
	"SoulCombatLab.AI.ControllerPerceptionConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSCLAIControllerPerceptionConfigurationTest::RunTest(const FString& Parameters)
{
	const ASCLAIController* const ControllerDefaults = GetDefault<ASCLAIController>();
	TestNotNull(TEXT("AIController CDO exists"), ControllerDefaults);
	if (ControllerDefaults == nullptr)
	{
		return false;
	}

	TestNotNull(
		TEXT("AIController owns an AIPerception component"),
		ControllerDefaults->GetAIPerceptionComponent());
	TestEqual(
		TEXT("Sight acquisition radius"),
		ControllerDefaults->GetConfiguredSightRadius(),
		1600.0F);
	TestEqual(
		TEXT("Sight loss radius"),
		ControllerDefaults->GetConfiguredLoseSightRadius(),
		1900.0F);
	TestEqual(
		TEXT("Sight peripheral half angle"),
		ControllerDefaults->GetConfiguredPeripheralVisionHalfAngle(),
		65.0F);
	TestEqual(
		TEXT("Sight stimulus max age"),
		ControllerDefaults->GetConfiguredSightMaxAge(),
		2.0F);
	TestEqual(
		TEXT("Sight loss grace period"),
		ControllerDefaults->GetConfiguredSightLossGracePeriod(),
		2.0F);
	TestFalse(TEXT("AIController has no Actor Tick"), ControllerDefaults->PrimaryActorTick.bCanEverTick);
	TestFalse(TEXT("AIController CDO begins without a target"), ControllerDefaults->HasPerceivedTarget());

	const ASCLEnemyCharacter* const EnemyDefaults = GetDefault<ASCLEnemyCharacter>();
	TestNotNull(TEXT("Enemy Character CDO exists"), EnemyDefaults);
	if (EnemyDefaults == nullptr)
	{
		return false;
	}

	TestEqual(
		TEXT("Enemy uses the native SCL AIController"),
		EnemyDefaults->AIControllerClass.Get(),
		ASCLAIController::StaticClass());
	TestEqual(
		TEXT("Enemy is possessed when placed or spawned"),
		EnemyDefaults->AutoPossessAI,
		EAutoPossessAI::PlacedInWorldOrSpawned);
	TestFalse(TEXT("Enemy inherits the Tick-free character boundary"), EnemyDefaults->PrimaryActorTick.bCanEverTick);
	TestTrue(
		TEXT("Enemy receives the native Light Attack Ability"),
		EnemyDefaults->HasStartupAbility(USCLLightAttackAbility::StaticClass()));
	TestTrue(
		TEXT("Enemy navigation uses acceleration so the locomotion AnimBP can enter movement"),
		EnemyDefaults->GetCharacterMovement()->UseAccelerationForPathFollowing());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSCLBlackboardBehaviorTreeContractTest,
	"SoulCombatLab.AI.BlackboardBehaviorTreeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSCLBlackboardBehaviorTreeContractTest::RunTest(const FString& Parameters)
{
	UBehaviorTree* const BehaviorTree =
		SCLBehaviorTreeBuilder::BuildEnemyBehaviorTree(*GetTransientPackage());
	TestNotNull(TEXT("Native Behavior Tree exists"), BehaviorTree);
	if (BehaviorTree == nullptr)
	{
		return false;
	}

	UBlackboardData* const BlackboardData = BehaviorTree->BlackboardAsset;
	TestNotNull(TEXT("Native Blackboard exists"), BlackboardData);
	TestNotNull(TEXT("Behavior Tree root exists"), BehaviorTree->RootNode.Get());
	if (BlackboardData == nullptr || BehaviorTree->RootNode == nullptr)
	{
		return false;
	}

	TestTrue(TEXT("Blackboard schema is valid"), BlackboardData->IsValid());
	TestEqual(TEXT("Blackboard key count including engine SelfActor"), BlackboardData->GetNumKeys(), 7);
	TestTrue(
		TEXT("Engine SelfActor key exists"),
		BlackboardData->IsValidKey(BlackboardData->GetKeyID(FBlackboard::KeySelf)));

	struct FExpectedKey
	{
		FName Name;
		UClass* Type;
	};
	const FExpectedKey ExpectedKeys[] = {
		{SCLBlackboardKeys::TargetActor, UBlackboardKeyType_Object::StaticClass()},
		{SCLBlackboardKeys::DistanceToTarget, UBlackboardKeyType_Float::StaticClass()},
		{SCLBlackboardKeys::HasLineOfSight, UBlackboardKeyType_Bool::StaticClass()},
		{SCLBlackboardKeys::CombatState, UBlackboardKeyType_Enum::StaticClass()},
		{SCLBlackboardKeys::CanAttack, UBlackboardKeyType_Bool::StaticClass()},
		{SCLBlackboardKeys::IdealCombatLocation, UBlackboardKeyType_Vector::StaticClass()}};
	for (const FExpectedKey& ExpectedKey : ExpectedKeys)
	{
		const FBlackboard::FKey KeyID = BlackboardData->GetKeyID(ExpectedKey.Name);
		TestTrue(
			*FString::Printf(TEXT("Key exists: %s"), *ExpectedKey.Name.ToString()),
			BlackboardData->IsValidKey(KeyID));
		TestEqual(
			*FString::Printf(TEXT("Key type: %s"), *ExpectedKey.Name.ToString()),
			BlackboardData->GetKeyType(KeyID).Get(),
			ExpectedKey.Type);
	}

	const FBlackboardEntry* const CombatStateEntry = BlackboardData->GetKey(
		BlackboardData->GetKeyID(SCLBlackboardKeys::CombatState));
	const UBlackboardKeyType_Enum* const CombatStateType = CombatStateEntry != nullptr
		? Cast<UBlackboardKeyType_Enum>(CombatStateEntry->KeyType)
		: nullptr;
	TestNotNull(TEXT("CombatState enum key exists"), CombatStateType);
	if (CombatStateType != nullptr)
	{
		TestEqual(
			TEXT("CombatState uses the native Enemy AI enum"),
			CombatStateType->EnumType.Get(),
			StaticEnum<ESCLEnemyAIState>());
		TestEqual(
			TEXT("CombatState defaults to Idle"),
			CombatStateType->DefaultValue,
			static_cast<uint8>(ESCLEnemyAIState::Idle));
	}

	TestEqual(TEXT("Root service count"), BehaviorTree->RootNode->Services.Num(), 1);
	TestTrue(
		TEXT("Root is a priority Selector"),
		BehaviorTree->RootNode->IsA<UBTComposite_Selector>());
	TestEqual(TEXT("Root child count"), BehaviorTree->RootNode->Children.Num(), 5);
	if (BehaviorTree->RootNode->Services.Num() == 1)
	{
		TestTrue(
			TEXT("Root uses the native combat-context Service"),
			BehaviorTree->RootNode->Services[0]->IsA<UBTService_SCLUpdateCombatContext>());
	}
	if (BehaviorTree->RootNode->Children.Num() == 5)
	{
		const ESCLEnemyAIState ExpectedBranchStates[] = {
			ESCLEnemyAIState::Dead,
			ESCLEnemyAIState::Staggered,
			ESCLEnemyAIState::Combat,
			ESCLEnemyAIState::Chase};
		for (int32 BranchIndex = 0; BranchIndex < UE_ARRAY_COUNT(ExpectedBranchStates); ++BranchIndex)
		{
			const FBTCompositeChild& Branch = BehaviorTree->RootNode->Children[BranchIndex];
			TestEqual(
				*FString::Printf(TEXT("Branch %d decorator count"), BranchIndex),
				Branch.Decorators.Num(),
				1);
			const UBTDecorator_SCLAIState* const StateDecorator =
				Branch.Decorators.Num() == 1
					? Cast<UBTDecorator_SCLAIState>(Branch.Decorators[0])
					: nullptr;
			TestNotNull(
				*FString::Printf(TEXT("Branch %d uses native state decorator"), BranchIndex),
				StateDecorator);
			if (StateDecorator != nullptr)
			{
				TestEqual(
					*FString::Printf(TEXT("Branch %d expected state"), BranchIndex),
					StateDecorator->GetExpectedState(),
					ExpectedBranchStates[BranchIndex]);
				TestEqual(
					*FString::Printf(TEXT("Branch %d abort mode"), BranchIndex),
					StateDecorator->GetFlowAbortMode(),
					EBTFlowAbortMode::Both);
			}
		}

		TestTrue(
			TEXT("Dead branch holds position"),
			BehaviorTree->RootNode->Children[0].ChildTask->IsA<UBTTask_SCLMaintainCombatContext>());
		TestTrue(
			TEXT("Staggered branch holds position"),
			BehaviorTree->RootNode->Children[1].ChildTask->IsA<UBTTask_SCLMaintainCombatContext>());
		const UBTComposite_Sequence* const CombatSequence = Cast<UBTComposite_Sequence>(
			BehaviorTree->RootNode->Children[2].ChildComposite);
		TestNotNull(TEXT("Combat branch is an attack-and-reposition Sequence"), CombatSequence);
		if (CombatSequence != nullptr)
		{
			TestEqual(TEXT("Combat Sequence child count"), CombatSequence->Children.Num(), 2);
			if (CombatSequence->Children.Num() == 2)
			{
				const UBTTask_SCLBasicAttack* const AttackTask = Cast<UBTTask_SCLBasicAttack>(
					CombatSequence->Children[0].ChildTask);
				TestNotNull(TEXT("Combat Sequence starts with native Basic Attack Task"), AttackTask);
				if (AttackTask != nullptr)
				{
					TestEqual(
						TEXT("Basic Attack Task reads TargetActor"),
						AttackTask->GetSelectedBlackboardKey(),
						SCLBlackboardKeys::TargetActor);
					TestEqual(
						TEXT("Basic Attack burst size"),
						AttackTask->GetAttacksPerBurst(),
						2);
					TestEqual(
						TEXT("Basic Attack recovery duration"),
						AttackTask->GetRecoveryDuration(),
						0.75F);
					TestEqual(
						TEXT("Basic Attack retry delay"),
						AttackTask->GetActivationRetryDelay(),
						0.50F);
				}

				const UBTComposite_Selector* const RepositionFallback =
					Cast<UBTComposite_Selector>(CombatSequence->Children[1].ChildComposite);
				TestNotNull(TEXT("Combat Sequence has a Reposition fallback Selector"), RepositionFallback);
				if (RepositionFallback != nullptr)
				{
					TestEqual(
						TEXT("Reposition fallback child count"),
						RepositionFallback->Children.Num(),
						2);
					if (RepositionFallback->Children.Num() == 2)
					{
						const UBTComposite_Sequence* const RepositionSequence =
							Cast<UBTComposite_Sequence>(
								RepositionFallback->Children[0].ChildComposite);
						TestNotNull(
							TEXT("Reposition path runs EQS then Move To"),
							RepositionSequence);
						if (RepositionSequence != nullptr)
						{
							TestEqual(
								TEXT("Reposition Sequence child count"),
								RepositionSequence->Children.Num(),
								2);
							if (RepositionSequence->Children.Num() == 2)
							{
								const UBTTask_SCLRunCombatEQS* const QueryTask =
									Cast<UBTTask_SCLRunCombatEQS>(
										RepositionSequence->Children[0].ChildTask);
								const UBTTask_SCLReposition* const MoveTask =
									Cast<UBTTask_SCLReposition>(
										RepositionSequence->Children[1].ChildTask);
								TestNotNull(TEXT("Reposition uses the native EQS Task"), QueryTask);
								TestNotNull(TEXT("Reposition uses the native Move Task"), MoveTask);
								if (QueryTask != nullptr)
								{
									TestEqual(
										TEXT("EQS result writes IdealCombatLocation"),
										QueryTask->GetSelectedBlackboardKey(),
										SCLBlackboardKeys::IdealCombatLocation);
									TestEqual(
										TEXT("EQS chooses a single best result"),
										QueryTask->EQSRequest.RunMode.GetValue(),
										EEnvQueryRunMode::SingleResult);
									TestFalse(
										TEXT("EQS failure preserves the previous key and takes fallback"),
										QueryTask->bUpdateBBOnFail);
								}
								if (MoveTask != nullptr)
								{
									TestEqual(
										TEXT("Reposition Move reads IdealCombatLocation"),
										MoveTask->GetSelectedBlackboardKey(),
										SCLBlackboardKeys::IdealCombatLocation);
									TestEqual(
										TEXT("Reposition Move acceptance radius"),
										MoveTask->GetConfiguredAcceptanceRadius(),
										35.0F);
								}
							}
						}

						const UBTTask_Wait* const FailureCooldown = Cast<UBTTask_Wait>(
							RepositionFallback->Children[1].ChildTask);
						TestNotNull(TEXT("EQS failure uses a bounded cooldown"), FailureCooldown);
						if (FailureCooldown != nullptr)
						{
							TestEqual(
								TEXT("EQS failure cooldown duration"),
								FailureCooldown->WaitTime.GetValue(
									static_cast<const UBlackboardComponent*>(nullptr)),
								0.25F);
						}
					}
				}
			}
		}
		const UBTTask_SCLChaseTarget* const ChaseTask =
			Cast<UBTTask_SCLChaseTarget>(BehaviorTree->RootNode->Children[3].ChildTask);
		TestNotNull(TEXT("Chase branch uses native Chase Task"), ChaseTask);
		if (ChaseTask != nullptr)
		{
			TestEqual(
				TEXT("Chase Task reads TargetActor"),
				ChaseTask->GetSelectedBlackboardKey(),
				SCLBlackboardKeys::TargetActor);
			TestEqual(
				TEXT("Chase Task acceptance radius"),
				ChaseTask->GetConfiguredAcceptanceRadius(),
				120.0F);
		}
		TestEqual(
			TEXT("Idle fallback has no state decorator"),
			BehaviorTree->RootNode->Children[4].Decorators.Num(),
			0);
		TestTrue(
			TEXT("Idle fallback holds position"),
			BehaviorTree->RootNode->Children[4].ChildTask->IsA<UBTTask_SCLMaintainCombatContext>());
	}

	const UBTComposite_Sequence* const CombatSequence = Cast<UBTComposite_Sequence>(
		BehaviorTree->RootNode->Children.IsValidIndex(2)
			? BehaviorTree->RootNode->Children[2].ChildComposite
			: nullptr);
	const UBTComposite_Selector* const RepositionFallback =
		CombatSequence != nullptr && CombatSequence->Children.IsValidIndex(1)
			? Cast<UBTComposite_Selector>(CombatSequence->Children[1].ChildComposite)
			: nullptr;
	const UBTComposite_Sequence* const RepositionSequence =
		RepositionFallback != nullptr && RepositionFallback->Children.IsValidIndex(0)
			? Cast<UBTComposite_Sequence>(RepositionFallback->Children[0].ChildComposite)
			: nullptr;
	const UBTTask_SCLRunCombatEQS* const QueryTask =
		RepositionSequence != nullptr && RepositionSequence->Children.IsValidIndex(0)
			? Cast<UBTTask_SCLRunCombatEQS>(RepositionSequence->Children[0].ChildTask)
			: nullptr;
	UEnvQuery* const CombatQuery = QueryTask != nullptr
		? QueryTask->EQSRequest.QueryTemplate
		: nullptr;
	TestNotNull(TEXT("Combat reposition owns an EQS query template"), CombatQuery);
	if (CombatQuery != nullptr)
	{
		TestEqual(TEXT("Combat EQS option count"), CombatQuery->GetOptions().Num(), 1);
		if (CombatQuery->GetOptions().Num() == 1)
		{
			const UEnvQueryOption* const Option = CombatQuery->GetOptions()[0];
			const USCLEnvQueryGenerator_CombatRing* const Generator =
				Option != nullptr
					? Cast<USCLEnvQueryGenerator_CombatRing>(Option->Generator)
					: nullptr;
			TestNotNull(TEXT("Combat EQS generates a target-centered combat ring"), Generator);
			if (Generator != nullptr)
			{
				TestEqual(
					TEXT("Combat ring preferred radius"),
					Generator->GetConfiguredRadius(),
					SCLCombatPositionQuery::PreferredDistance);
				TestEqual(
					TEXT("Combat ring candidate count"),
					Generator->GetConfiguredPointCount(),
					SCLCombatPositionQuery::CandidatePointCount);
				TestEqual(
					TEXT("Combat ring projects candidates onto navmesh"),
					Generator->GetProjectionData().TraceMode.GetValue(),
					EEnvQueryTrace::Navigation);
			}

			TestEqual(TEXT("Combat EQS test count"), Option->Tests.Num(), 4);
			const UEnvQueryTest_Distance* DistanceTest = nullptr;
			const UEnvQueryTest_Trace* LineOfSightTest = nullptr;
			const UEnvQueryTest_Pathfinding* PathTest = nullptr;
			const USCLEnvQueryTest_LateralPosition* LateralTest = nullptr;
			for (const UEnvQueryTest* const QueryTest : Option->Tests)
			{
				DistanceTest = DistanceTest != nullptr
					? DistanceTest
					: Cast<UEnvQueryTest_Distance>(QueryTest);
				LineOfSightTest = LineOfSightTest != nullptr
					? LineOfSightTest
					: Cast<UEnvQueryTest_Trace>(QueryTest);
				PathTest = PathTest != nullptr
					? PathTest
					: Cast<UEnvQueryTest_Pathfinding>(QueryTest);
				LateralTest = LateralTest != nullptr
					? LateralTest
					: Cast<USCLEnvQueryTest_LateralPosition>(QueryTest);
			}
			TestNotNull(TEXT("Combat EQS filters and scores target distance"), DistanceTest);
			TestNotNull(TEXT("Combat EQS filters line of sight"), LineOfSightTest);
			TestNotNull(TEXT("Combat EQS filters unreachable paths"), PathTest);
			TestNotNull(TEXT("Combat EQS scores lateral angles"), LateralTest);
			if (DistanceTest != nullptr)
			{
				TestEqual(
					TEXT("Combat EQS minimum target distance"),
					DistanceTest->FloatValueMin.DefaultValue,
					SCLCombatPositionQuery::MinimumDistance);
				TestEqual(
					TEXT("Combat EQS maximum target distance"),
					DistanceTest->FloatValueMax.DefaultValue,
					SCLCombatPositionQuery::MaximumDistance);
			}
			if (LineOfSightTest != nullptr)
			{
				TestEqual(
					TEXT("Combat EQS line-of-sight trace channel"),
					LineOfSightTest->TraceData.SerializedChannel.GetValue(),
					ECC_Visibility);
				TestFalse(
					TEXT("Combat EQS requires an unobstructed trace"),
					LineOfSightTest->BoolValue.DefaultValue);
			}
			if (PathTest != nullptr)
			{
				TestEqual(
					TEXT("Combat EQS uses path-existence testing"),
					PathTest->TestMode.GetValue(),
					EEnvTestPathfinding::PathExist);
				TestTrue(
					TEXT("Combat EQS requires a reachable path"),
					PathTest->BoolValue.DefaultValue);
			}
			if (LateralTest != nullptr)
			{
				TestTrue(
					TEXT("Combat EQS uses a 2D absolute angle score"),
					LateralTest->UsesAbsoluteTwoDimensionalDot());
				TestTrue(
					TEXT("Combat EQS compares target facing against target-to-item direction"),
					LateralTest->UsesTargetFacingAndTargetToItemDirection());
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSCLCombatRangeHysteresisTest,
	"SoulCombatLab.AI.CombatRangeHysteresis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSCLCombatRangeHysteresisTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Chase enters Combat at the inclusive enter boundary"),
		SCLAIStatePolicy::ResolveTargetRangeState(
			SCLAIStatePolicy::CombatEnterDistance,
			ESCLEnemyAIState::Chase),
		ESCLEnemyAIState::Combat);
	TestEqual(
		TEXT("Chase stays Chase just outside the enter boundary"),
		SCLAIStatePolicy::ResolveTargetRangeState(
			SCLAIStatePolicy::CombatEnterDistance + 0.1F,
			ESCLEnemyAIState::Chase),
		ESCLEnemyAIState::Chase);
	TestEqual(
		TEXT("Combat remains stable at the inclusive exit boundary"),
		SCLAIStatePolicy::ResolveTargetRangeState(
			SCLAIStatePolicy::CombatExitDistance,
			ESCLEnemyAIState::Combat),
		ESCLEnemyAIState::Combat);
	TestEqual(
		TEXT("Combat returns to Chase outside the exit boundary"),
		SCLAIStatePolicy::ResolveTargetRangeState(
			SCLAIStatePolicy::CombatExitDistance + 0.1F,
			ESCLEnemyAIState::Combat),
		ESCLEnemyAIState::Chase);
	TestEqual(
		TEXT("The logged 220.5 cm transition remains Combat"),
		SCLAIStatePolicy::ResolveTargetRangeState(220.5F, ESCLEnemyAIState::Combat),
		ESCLEnemyAIState::Combat);
	TestEqual(
		TEXT("Attack exits through the same close-combat hysteresis"),
		SCLAIStatePolicy::ResolveTargetRangeState(250.0F, ESCLEnemyAIState::Attack),
		ESCLEnemyAIState::Combat);
	TestTrue(
		TEXT("An unseen engaged target is retained at the awareness boundary"),
		SCLAIStatePolicy::ShouldRetainUnseenTarget(1900.0F, 1900.0F));
	TestFalse(
		TEXT("An unseen target outside the awareness boundary is released"),
		SCLAIStatePolicy::ShouldRetainUnseenTarget(1900.1F, 1900.0F));
	TestFalse(
		TEXT("An invalid awareness distance cannot retain a target"),
		SCLAIStatePolicy::ShouldRetainUnseenTarget(0.0F, 0.0F));

	return true;
}

#endif
