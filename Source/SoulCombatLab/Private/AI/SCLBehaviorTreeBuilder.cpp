#include "AI/SCLBehaviorTreeBuilder.h"

#include "AI/SCLAIState.h"
#include "AI/EQS/SCLCombatPositionQuery.h"
#include "AI/Decorators/BTDecorator_SCLAIState.h"
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
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "GameFramework/Actor.h"

namespace
{
template <typename TKeyType>
TKeyType& AddKey(UBlackboardData& BlackboardData, const FName KeyName)
{
	FBlackboardEntry& Entry = BlackboardData.Keys.Emplace_GetRef();
	Entry.EntryName = KeyName;
	Entry.KeyType = NewObject<TKeyType>(&BlackboardData);
	return *CastChecked<TKeyType>(Entry.KeyType);
}

template <typename TTaskType>
TTaskType& AddStateBranch(
	UBehaviorTree& BehaviorTree,
	UBTCompositeNode& ParentNode,
	const ESCLEnemyAIState ExpectedState)
{
	TTaskType* const Task = NewObject<TTaskType>(&BehaviorTree);
	Task->InitializeFromAsset(BehaviorTree);
	FBTCompositeChild& Child = ParentNode.Children.Emplace_GetRef();
	Child.ChildTask = Task;

	UBTDecorator_SCLAIState* const StateDecorator =
		NewObject<UBTDecorator_SCLAIState>(&BehaviorTree);
	StateDecorator->ConfigureState(ExpectedState);
	StateDecorator->InitializeFromAsset(BehaviorTree);
	Child.Decorators.Add(StateDecorator);
	return *Task;
}

template <typename TCompositeType>
TCompositeType& AddStateCompositeBranch(
	UBehaviorTree& BehaviorTree,
	UBTCompositeNode& ParentNode,
	const ESCLEnemyAIState ExpectedState,
	const FName NodeName)
{
	TCompositeType* const Composite = NewObject<TCompositeType>(&BehaviorTree, NodeName);
	Composite->InitializeFromAsset(BehaviorTree);
	FBTCompositeChild& Child = ParentNode.Children.Emplace_GetRef();
	Child.ChildComposite = Composite;

	UBTDecorator_SCLAIState* const StateDecorator =
		NewObject<UBTDecorator_SCLAIState>(&BehaviorTree);
	StateDecorator->ConfigureState(ExpectedState);
	StateDecorator->InitializeFromAsset(BehaviorTree);
	Child.Decorators.Add(StateDecorator);
	return *Composite;
}

template <typename TTaskType>
TTaskType& AddTaskChild(UBehaviorTree& BehaviorTree, UBTCompositeNode& ParentNode)
{
	TTaskType* const Task = NewObject<TTaskType>(&BehaviorTree);
	Task->InitializeFromAsset(BehaviorTree);
	ParentNode.Children.Emplace_GetRef().ChildTask = Task;
	return *Task;
}

template <typename TCompositeType>
TCompositeType& AddCompositeChild(
	UBehaviorTree& BehaviorTree,
	UBTCompositeNode& ParentNode,
	const FName NodeName)
{
	TCompositeType* const Composite = NewObject<TCompositeType>(&BehaviorTree, NodeName);
	Composite->InitializeFromAsset(BehaviorTree);
	ParentNode.Children.Emplace_GetRef().ChildComposite = Composite;
	return *Composite;
}
}

UBehaviorTree* SCLBehaviorTreeBuilder::BuildEnemyBehaviorTree(UObject& Outer)
{
	UBlackboardData* const BlackboardData = NewObject<UBlackboardData>(
		&Outer,
		TEXT("SCLNativeEnemyBlackboard"));
	UBlackboardKeyType_Object& TargetActorKey =
		AddKey<UBlackboardKeyType_Object>(*BlackboardData, SCLBlackboardKeys::TargetActor);
	TargetActorKey.BaseClass = AActor::StaticClass();
	AddKey<UBlackboardKeyType_Float>(*BlackboardData, SCLBlackboardKeys::DistanceToTarget);
	AddKey<UBlackboardKeyType_Bool>(*BlackboardData, SCLBlackboardKeys::HasLineOfSight);
	UBlackboardKeyType_Enum& CombatStateKey =
		AddKey<UBlackboardKeyType_Enum>(*BlackboardData, SCLBlackboardKeys::CombatState);
	CombatStateKey.EnumType = StaticEnum<ESCLEnemyAIState>();
	CombatStateKey.EnumName = CombatStateKey.EnumType->GetPathName();
	CombatStateKey.DefaultValue = static_cast<uint8>(ESCLEnemyAIState::Idle);
	AddKey<UBlackboardKeyType_Bool>(*BlackboardData, SCLBlackboardKeys::CanAttack);
	AddKey<UBlackboardKeyType_Vector>(*BlackboardData, SCLBlackboardKeys::IdealCombatLocation);
	BlackboardData->UpdateParentKeys();
	BlackboardData->UpdateKeyIDs();
	BlackboardData->UpdateIfHasSynchronizedKeys();

	UBehaviorTree* const BehaviorTree = NewObject<UBehaviorTree>(
		&Outer,
		TEXT("SCLNativeEnemyBehaviorTree"));
	BehaviorTree->BlackboardAsset = BlackboardData;

	UBTComposite_Selector* const RootNode = NewObject<UBTComposite_Selector>(
		BehaviorTree,
		TEXT("PrioritySelector"));
	RootNode->InitializeFromAsset(*BehaviorTree);
	BehaviorTree->RootNode = RootNode;

	UBTService_SCLUpdateCombatContext* const ContextService =
		NewObject<UBTService_SCLUpdateCombatContext>(BehaviorTree, TEXT("CombatContextService"));
	ContextService->InitializeFromAsset(*BehaviorTree);
	RootNode->Services.Add(ContextService);

	AddStateBranch<UBTTask_SCLMaintainCombatContext>(
		*BehaviorTree,
		*RootNode,
		ESCLEnemyAIState::Dead);
	AddStateBranch<UBTTask_SCLMaintainCombatContext>(
		*BehaviorTree,
		*RootNode,
		ESCLEnemyAIState::Staggered);
	UBTComposite_Sequence& CombatSequence = AddStateCompositeBranch<UBTComposite_Sequence>(
		*BehaviorTree,
		*RootNode,
		ESCLEnemyAIState::Combat,
		TEXT("CombatAttackAndReposition"));
	AddTaskChild<UBTTask_SCLBasicAttack>(*BehaviorTree, CombatSequence);

	UBTComposite_Selector& RepositionFallback = AddCompositeChild<UBTComposite_Selector>(
		*BehaviorTree,
		CombatSequence,
		TEXT("RepositionFallback"));
	UBTComposite_Sequence& RepositionSequence = AddCompositeChild<UBTComposite_Sequence>(
		*BehaviorTree,
		RepositionFallback,
		TEXT("RunEQSAndMove"));

	UBTTask_SCLRunCombatEQS* const RunQueryTask =
		NewObject<UBTTask_SCLRunCombatEQS>(BehaviorTree);
	RunQueryTask->ConfigureQuery(*SCLCombatPositionQuery::Build(*BehaviorTree));
	RunQueryTask->InitializeFromAsset(*BehaviorTree);
	RepositionSequence.Children.Emplace_GetRef().ChildTask = RunQueryTask;
	AddTaskChild<UBTTask_SCLReposition>(*BehaviorTree, RepositionSequence);

	UBTTask_Wait& RepositionFailureCooldown = AddTaskChild<UBTTask_Wait>(
		*BehaviorTree,
		RepositionFallback);
	RepositionFailureCooldown.NodeName = TEXT("EQS Failure Cooldown");
	RepositionFailureCooldown.WaitTime = FValueOrBBKey_Float{0.25F};
	RepositionFailureCooldown.RandomDeviation = FValueOrBBKey_Float{0.0F};
	AddStateBranch<UBTTask_SCLChaseTarget>(
		*BehaviorTree,
		*RootNode,
		ESCLEnemyAIState::Chase);

	UBTTask_SCLMaintainCombatContext* const IdleTask =
		NewObject<UBTTask_SCLMaintainCombatContext>(BehaviorTree, TEXT("IdleTask"));
	IdleTask->InitializeFromAsset(*BehaviorTree);
	RootNode->Children.Emplace_GetRef().ChildTask = IdleTask;

	return BehaviorTree;
}
