#include "Animation/SCLAnimationAssetLibrary.h"

#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"

#if WITH_EDITOR
#include "Animation/SCLPlayerAnimInstance.h"
#include "AnimationStateGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_Slot.h"
#include "AnimStateEntryNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_AnimGetter.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

bool USCLAnimationAssetLibrary::HasBlendSpaceSampling(const UBlendSpace* const BlendSpace)
{
#if WITH_EDITOR
	return BlendSpace && !BlendSpace->GetBlendSpaceData().IsEmpty();
#else
	return false;
#endif
}

bool USCLAnimationAssetLibrary::RebuildBlendSpaceSampling(UBlendSpace* const BlendSpace)
{
#if WITH_EDITOR
	if (!BlendSpace || BlendSpace->GetNumberOfBlendSamples() < 3) return false;
	UE_LOG(LogTemp, Display, TEXT("SCL blend %s before rebuild: empty=%d"),
		*BlendSpace->GetName(), BlendSpace->GetBlendSpaceData().IsEmpty());
	BlendSpace->Modify();
	BlendSpace->ResampleData();
	UE_LOG(LogTemp, Display, TEXT("SCL blend %s after rebuild: empty=%d"),
		*BlendSpace->GetName(), BlendSpace->GetBlendSpaceData().IsEmpty());
	return !BlendSpace->GetBlendSpaceData().IsEmpty();
#else
	return false;
#endif
}

bool USCLAnimationAssetLibrary::ConfigureComboSections(
	UAnimMontage* const Montage,
	const TArray<UAnimSequenceBase*>& Animations,
	const TArray<FName>& SectionNames)
{
#if WITH_EDITOR
	if (Montage == nullptr || Animations.IsEmpty() || Animations.Num() != SectionNames.Num())
	{
		return false;
	}

	Montage->Modify();
	if (Montage->SlotAnimTracks.IsEmpty())
	{
		Montage->SlotAnimTracks.AddDefaulted();
		Montage->SlotAnimTracks[0].SlotName = TEXT("DefaultSlot");
	}

	FAnimTrack& AnimTrack = Montage->SlotAnimTracks[0].AnimTrack;
	AnimTrack.AnimSegments.Reset(Animations.Num());
	float SegmentStartTime = 0.0F;
	for (UAnimSequenceBase* const Animation : Animations)
	{
		if (Animation == nullptr)
		{
			return false;
		}

		FAnimSegment& Segment = AnimTrack.AnimSegments.AddDefaulted_GetRef();
		Segment.SetAnimReference(Animation, true);
		Segment.StartPos = SegmentStartTime;
		Segment.AnimStartTime = 0.0F;
		Segment.AnimEndTime = Animation->GetPlayLength();
		Segment.AnimPlayRate = 1.0F;
		Segment.LoopingCount = 1;
		SegmentStartTime += Animation->GetPlayLength();
	}
	AnimTrack.CollapseAnimSegments();

	TArray<float> SectionStartTimes;
	SectionStartTimes.Reserve(AnimTrack.AnimSegments.Num());
	for (const FAnimSegment& Segment : AnimTrack.AnimSegments)
	{
		SectionStartTimes.Add(Segment.StartPos);
	}

	for (int32 SectionIndex = Montage->GetNumSections() - 1; SectionIndex >= 0; --SectionIndex)
	{
		Montage->DeleteAnimCompositeSection(SectionIndex);
	}

	for (int32 SectionIndex = 0; SectionIndex < SectionNames.Num(); ++SectionIndex)
	{
		if (SectionNames[SectionIndex].IsNone() ||
			Montage->AddAnimCompositeSection(SectionNames[SectionIndex], SectionStartTimes[SectionIndex]) == INDEX_NONE)
		{
			return false;
		}
	}
	Montage->UpdateLinkableElements();

	for (int32 SectionIndex = 0; SectionIndex < Montage->CompositeSections.Num(); ++SectionIndex)
	{
		FCompositeSection& Section = Montage->CompositeSections[SectionIndex];
#if WITH_EDITORONLY_DATA
		// UE 5.8 PostLoad gives this legacy field precedence when it is non-zero.
		// Clear stale values left by the previously malformed generated montage.
		Section.StartTime_DEPRECATED = 0.0F;
#endif
		Section.Link(Montage, SectionStartTimes[SectionIndex]);
		Section.ChangeLinkMethod(EAnimLinkMethod::Absolute);
		Section.SetTime(SectionStartTimes[SectionIndex], EAnimLinkMethod::Absolute);
		Section.NextSectionName = NAME_None;
	}

	Montage->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

TArray<float> USCLAnimationAssetLibrary::GetComboSectionStartTimes(const UAnimMontage* const Montage)
{
	TArray<float> SectionStartTimes;
	if (Montage == nullptr)
	{
		return SectionStartTimes;
	}

	SectionStartTimes.Reserve(Montage->GetNumSections());
	for (int32 SectionIndex = 0; SectionIndex < Montage->GetNumSections(); ++SectionIndex)
	{
		SectionStartTimes.Add(Montage->CompositeSections[SectionIndex].GetTime());
	}
	return SectionStartTimes;
}

#if WITH_EDITOR
namespace SCLJumpEditor
{
// 以下函数只在首次生成资源时运行。生成结果是普通状态机，之后直接在 AnimBP 编辑。
template<typename T>
T* AddNode(UEdGraph* Graph, const int32 X, const int32 Y)
{
 FGraphNodeCreator<T> Creator(*Graph);
 T* Node = Creator.CreateNode();
 Node->NodePosX = X; Node->NodePosY = Y;
 Creator.Finalize();
 return Node;
}

bool Connect(UEdGraphPin* Output, UEdGraphPin* Input)
{
 return Output && Input && Input->GetOwningNode()->GetGraph()->GetSchema()->TryCreateConnection(Output, Input);
}

UEdGraphPin* ReadVariable(UEdGraph* Graph, const FName Name, const int32 X, const int32 Y)
{
 FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
 auto* Node = Creator.CreateNode();
 Node->VariableReference.SetSelfMember(Name);
 Node->NodePosX = X; Node->NodePosY = Y;
 Creator.Finalize();
 return Node->FindPin(Name, EGPD_Output);
}

UK2Node_CallFunction* Math(UEdGraph* Graph, const FName Function, const int32 X, const int32 Y)
{
 FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
 auto* Node = Creator.CreateNode();
 Node->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(Function));
 Node->NodePosX = X; Node->NodePosY = Y;
 Creator.Finalize();
 return Node;
}

UAnimStateNode* State(UAnimationStateMachineGraph* Graph, const TCHAR* Name, int32 X, int32 Y)
{
 auto* Node = AddNode<UAnimStateNode>(Graph, X, Y);
 FBlueprintEditorUtils::RenameGraph(Node->BoundGraph, Name);
 Node->bAlwaysResetOnEntry = true; // 连续跳跃时重新起播，不能沿用上一次的末帧。
 return Node;
}

UAnimStateTransitionNode* Transition(UAnimationStateMachineGraph* Graph,
 UAnimStateNode* From, UAnimStateNode* To, const int32 Priority, const float BlendSeconds)
{
 auto* Node = AddNode<UAnimStateTransitionNode>(Graph, (From->NodePosX+To->NodePosX)/2,
  (From->NodePosY+To->NodePosY)/2);
 Node->CreateConnections(From, To);
 Node->PriorityOrder = Priority;
 Node->CrossfadeDuration = BlendSeconds;
 return Node;
}

UEdGraphPin* ResultPin(UAnimStateTransitionNode* Transition)
{
 TArray<UAnimGraphNode_TransitionResult*> Results;
 Transition->BoundGraph->GetNodesOfClass(Results);
 return Results.Num()==1 ? Results[0]->FindPin(TEXT("bCanEnterTransition")) : nullptr;
}

// Falling && (VerticalSpeed > 0 或 <= 0)：上升进起跳，走下台阶直接进入空中循环。
bool AirRule(UAnimStateTransitionNode* Transition, const bool bRising)
{
 UEdGraph* Graph = Transition->BoundGraph;
 auto* Compare = Math(Graph, bRising ? TEXT("Greater_DoubleDouble") : TEXT("LessEqual_DoubleDouble"), -400, 120);
 auto* And = Math(Graph, TEXT("BooleanAND"), -170, 0);
 return Connect(ReadVariable(Graph,TEXT("VerticalSpeed"),-660,120), Compare->FindPin(TEXT("A"))) &&
  Connect(ReadVariable(Graph,TEXT("bIsFalling"),-660,-80), And->FindPin(TEXT("A"))) &&
  Connect(Compare->GetReturnValuePin(),And->FindPin(TEXT("B"))) &&
  Connect(And->GetReturnValuePin(),ResultPin(Transition));
}

bool LandRule(UAnimStateTransitionNode* Transition)
{
 UEdGraph* Graph = Transition->BoundGraph;
 auto* Not = Math(Graph,TEXT("Not_PreBool"),-200,0);
 return Connect(ReadVariable(Graph,TEXT("bIsFalling"),-440,0),Not->FindPin(TEXT("A"))) &&
  Connect(Not->GetReturnValuePin(),ResultPin(Transition));
}

bool Sequence(UAnimStateNode* State, UAnimSequence* Asset, const bool bLoop, const float Rate)
{
 auto* Node = AddNode<UAnimGraphNode_SequencePlayer>(State->BoundGraph,-300,0);
 Node->Node.SetSequence(Asset);
 Node->Node.SetLoopAnimation(bLoop);
 Node->Node.SetPlayRate(Rate);
 return Connect(Node->FindPin(TEXT("Pose")), State->GetPoseSinkPinInsideState());
}
}
#endif

bool USCLAnimationAssetLibrary::ConfigurePlayerJumpStateMachine(UAnimBlueprint* Blueprint,
 UAnimSequence* JumpStart, UAnimSequence* JumpLoop, UAnimSequence* JumpLand)
{
#if WITH_EDITOR
 using namespace SCLJumpEditor;
 if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(USCLPlayerAnimInstance::StaticClass()) ||
  !JumpStart || !JumpLoop || !JumpLand || JumpStart->GetSkeleton()!=Blueprint->TargetSkeleton ||
  JumpLoop->GetSkeleton()!=Blueprint->TargetSkeleton || JumpLand->GetSkeleton()!=Blueprint->TargetSkeleton) return false;
 TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
 for (const auto* Graph : Graphs)
  if (Graph->GetFName()==TEXT("PlayerLocomotion")) return Blueprint->Status!=BS_Error;
 UEdGraph* AnimGraph = nullptr;
 for (auto* Graph : Graphs) if (Graph->GetFName()==TEXT("AnimGraph")) AnimGraph=Graph;
 if (!AnimGraph) return false;
 TArray<UAnimGraphNode_Slot*> Slots; AnimGraph->GetNodesOfClass(Slots);
 // 只改已有 DefaultSlot 的底层输入，绝不删除用户原来的 Blend Space 或事件图。
 auto* Slot = Slots.Num()==1 ? Slots[0] : nullptr;
 UEdGraphPin* Source = Slot ? Slot->FindPin(TEXT("Source")) : nullptr;
 if (!Source || Source->LinkedTo.Num()!=1) return false;
 UEdGraphPin* GroundPose = Source->LinkedTo[0];
 Blueprint->Modify(); AnimGraph->Modify();
 auto* Cache = AddNode<UAnimGraphNode_SaveCachedPose>(AnimGraph,-100,300);
 Cache->CacheName = TEXT("PlayerGroundPose");
 auto* Machine = AddNode<UAnimGraphNode_StateMachine>(AnimGraph,200,0);
 auto* StateGraph = Machine->EditorStateMachineGraph.Get();
 FBlueprintEditorUtils::RenameGraph(StateGraph,TEXT("PlayerLocomotion"));
 auto* Ground = State(StateGraph,TEXT("Ground"),200,0);
 auto* Start = State(StateGraph,TEXT("JumpStart"),440,-180);
 auto* Air = State(StateGraph,TEXT("JumpLoop"),700,0);
 auto* Land = State(StateGraph,TEXT("JumpLand"),440,200);
 auto* UseGround = AddNode<UAnimGraphNode_UseCachedPose>(Ground->BoundGraph,-300,0);
 UseGround->SaveCachedPoseNode = Cache;
 bool bConnected = Connect(GroundPose,Cache->FindPin(TEXT("Pose"))) &&
  Connect(UseGround->FindPin(TEXT("Pose")),Ground->GetPoseSinkPinInsideState()) &&
  Connect(StateGraph->EntryNode->GetOutputPin(),Ground->GetInputPin());
 // 起跳原片 0.83 秒，默认 1.8 倍使准备段在当前约 0.51 秒的上升过程内完成。
 // Play Rate、混合时间与转换规则都保存在节点上，之后可直接在资源里调整。
 bConnected &= Sequence(Start,JumpStart,false,1.8F);
 bConnected &= Sequence(Air,JumpLoop,true,1.0F);
 bConnected &= Sequence(Land,JumpLand,false,1.6F);
 bConnected &= AirRule(Transition(StateGraph,Ground,Start,0,0.08F),true);
 bConnected &= AirRule(Transition(StateGraph,Ground,Air,1,0.10F),false);
 bConnected &= LandRule(Transition(StateGraph,Start,Land,0,0.08F));
 auto* StartFinished = Transition(StateGraph,Start,Air,1,0.08F);
 StartFinished->bAutomaticRuleBasedOnSequencePlayerInState = true;
 bConnected &= LandRule(Transition(StateGraph,Air,Land,0,0.08F));
 // 落地时再次离地优先；移动可以提前结束落地尾段，站立则播到末尾再回待机。
 bConnected &= AirRule(Transition(StateGraph,Land,Start,0,0.08F),true);
 bConnected &= AirRule(Transition(StateGraph,Land,Air,1,0.10F),false);
 auto* Moving = Transition(StateGraph,Land,Ground,2,0.10F);
 auto* Compare = Math(Moving->BoundGraph,TEXT("Greater_DoubleDouble"),-200,0);
 Compare->FindPin(TEXT("B"))->DefaultValue = TEXT("5.0");
 bConnected &= Connect(ReadVariable(Moving->BoundGraph,TEXT("GroundSpeed"),-440,0),Compare->FindPin(TEXT("A")));
 // 至少给落地 0.08 秒，再允许移动接管；否则一帧内 Land→Ground，落地姿势根本看不到。
 FGraphNodeCreator<UK2Node_AnimGetter> GetterCreator(*Moving->BoundGraph);
 auto* Elapsed = GetterCreator.CreateNode();
 Elapsed->SetFromFunction(UAnimInstance::StaticClass()->FindFunctionByName(TEXT("GetInstanceCurrentStateElapsedTime")));
 Elapsed->SourceNode = Machine;
 Elapsed->GetterClass = USCLPlayerAnimInstance::StaticClass();
 Elapsed->SourceAnimBlueprint = Blueprint;
 Elapsed->Contexts.Add(TEXT("Transition"));
 Elapsed->CachedTitle = FText::FromString(TEXT("落地状态已经经过的秒数"));
 Elapsed->NodePosX = -440; Elapsed->NodePosY = 140;
 GetterCreator.Finalize();
 auto* TimeCompare = Math(Moving->BoundGraph,TEXT("Greater_DoubleDouble"),-200,140);
 TimeCompare->FindPin(TEXT("B"))->DefaultValue = TEXT("0.08");
 auto* And = Math(Moving->BoundGraph,TEXT("BooleanAND"),20,0);
 bConnected &= Connect(Elapsed->GetReturnValuePin(),TimeCompare->FindPin(TEXT("A")));
 bConnected &= Connect(Compare->GetReturnValuePin(),And->FindPin(TEXT("A")));
 bConnected &= Connect(TimeCompare->GetReturnValuePin(),And->FindPin(TEXT("B")));
 bConnected &= Connect(And->GetReturnValuePin(),ResultPin(Moving));
 auto* LandFinished = Transition(StateGraph,Land,Ground,3,0.10F);
 LandFinished->bAutomaticRuleBasedOnSequencePlayerInState = true;
 if (!bConnected) return false;
 Source->BreakAllPinLinks();
 if (!Connect(Machine->FindPin(TEXT("Pose")),Source)) return false;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
 FKismetEditorUtilities::CompileBlueprint(Blueprint);
 Blueprint->MarkPackageDirty();
 return Blueprint->Status!=BS_Error;
#else
 return false;
#endif
}
