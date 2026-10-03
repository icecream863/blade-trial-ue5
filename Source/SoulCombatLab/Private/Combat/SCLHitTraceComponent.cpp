#include "Combat/SCLHitTraceComponent.h"

#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Debug/SCLCombatDebugSubsystem.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

/**
 * 默认关闭组件 Tick；只有攻击 Notify 的有效帧会调用 TickTrace。
 */
USCLHitTraceComponent::USCLHitTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/**
 * 由武器设置刀根/刀尖组件，采样点在两端之间均匀生成。
 */
void USCLHitTraceComponent::SetTraceEndpoints(USceneComponent* const InTraceStart, USceneComponent* const InTraceEnd)
{
	TraceStart = InTraceStart;
	TraceEnd = InTraceEnd;
}

/**
 * 新命中窗口：约束采样参数、递增代次、清命中集合并保存首帧位置。
 * 端点无效时返回 false；不会在开启瞬间扫掠一条历史轨迹。
 */
bool USCLHitTraceComponent::BeginTrace()
{
	if (!IsValid(TraceStart) || !IsValid(TraceEnd))
	{
		return false;
	}

	SampleCount = FMath::Clamp(SampleCount, 2, 12);
	TraceRadius = FMath::Max(TraceRadius, 0.1F);
	++TraceGeneration;
	HitActors.Reset();
	PreviousSamplePositions = CaptureSamplePositions();
	bTraceActive = PreviousSamplePositions.Num() == SampleCount;
	return bTraceActive;
}

/**
 * 将每个刀刃采样点从上帧位置扫到本帧位置，收集 Pawn 命中。
 * 事件回调可能同步 EndTrace；循环每次都核对代次，失效就立刻返回。
 */
void USCLHitTraceComponent::TickTrace(const float SweepFraction)
{
	if (!bTraceActive)
	{
		return;
	}
	// 命中事件可能同步取消攻击并调用 EndTrace；用代次防止继续访问旧采样数组。
	const uint64 ActiveTraceGeneration = TraceGeneration;
	TRACE_CPUPROFILER_EVENT_SCOPE(SCL_WeaponTrace);

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		EndTrace();
		return;
	}

	TArray<FVector> CurrentSamplePositions = CaptureSamplePositions();
	if (CurrentSamplePositions.Num() != PreviousSamplePositions.Num())
	{
		PreviousSamplePositions = MoveTemp(CurrentSamplePositions);
		return;
	}
	const float Fraction = FMath::Clamp(SweepFraction, 0.0F, 1.0F);
	if (Fraction <= 0.0F)
	{
		// Begin 通知可能早于本帧骨骼：第一次回调只建立已完成姿势的基线。
		// 不把窗口开启前的旧姿势带进下一帧扫掠，也不在零有效时间里结算伤害。
		PreviousSamplePositions = MoveTemp(CurrentSamplePositions);
		return;
	}
	for (int32 Index = 0; Index < CurrentSamplePositions.Num(); ++Index)
		CurrentSamplePositions[Index] = FMath::Lerp(PreviousSamplePositions[Index], CurrentSamplePositions[Index], Fraction);

	FCollisionQueryParams QueryParams{SCENE_QUERY_STAT(SCLWeaponTrace), false, GetOwner()};
	if (const AActor* const Wielder = GetOwner() != nullptr ? GetOwner()->GetOwner() : nullptr)
	{
		// Trace 组件的 Owner 是武器；武器的 Owner 才是持刀角色，两者都不能命中自己。
		QueryParams.AddIgnoredActor(Wielder);
	}

	const FCollisionShape TraceShape = FCollisionShape::MakeSphere(TraceRadius);
	const FCollisionObjectQueryParams ObjectQueryParams{TraceObjectType};
	for (int32 SampleIndex = 0; SampleIndex < CurrentSamplePositions.Num(); ++SampleIndex)
	{
		if (!bTraceActive ||
			TraceGeneration != ActiveTraceGeneration ||
			!PreviousSamplePositions.IsValidIndex(SampleIndex))
		{
			return;
		}

		const FVector& PreviousPosition = PreviousSamplePositions[SampleIndex];
		const FVector& CurrentPosition = CurrentSamplePositions[SampleIndex];
		TArray<FHitResult> HitResults;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SCL_WeaponSweep);
			// 相邻两帧的同一刀刃采样点之间做球形扫掠，减少快速挥刀漏判。
			World->SweepMultiByObjectType(
				HitResults,
				PreviousPosition,
				CurrentPosition,
				FQuat::Identity,
				ObjectQueryParams,
				TraceShape,
				QueryParams);
		}

		if (bDebugDraw || USCLCombatDebugSubsystem::IsEnabledForWorld(World))
		{
			const FVector Delta = CurrentPosition - PreviousPosition;
			const FQuat Rotation = Delta.IsNearlyZero() ? FQuat::Identity
				: FQuat::FindBetweenNormals(FVector::UpVector, Delta.GetSafeNormal());
			DrawDebugCapsule(World, (PreviousPosition + CurrentPosition) * 0.5F,
				Delta.Size() * 0.5F + TraceRadius, TraceRadius, Rotation, FColor::Cyan,
				false, 0.2F, 0, 0.75F);
		}

		for (const FHitResult& HitResult : HitResults)
		{
			ProcessHit(HitResult);
			if (!bTraceActive || TraceGeneration != ActiveTraceGeneration)
			{
				return;
			}
		}
	}

	PreviousSamplePositions = MoveTemp(CurrentSamplePositions);
}

/**
 * 终止窗口并递增代次，使正在执行的旧循环失效；清除采样与去重集合。
 */
void USCLHitTraceComponent::EndTrace()
{
	++TraceGeneration;
	bTraceActive = false;
	PreviousSamplePositions.Reset();
	HitActors.Reset();
}

/**
 * 在两个端点的世界坐标间线性插值；SampleCount 包括首尾两个端点。
 */
TArray<FVector> USCLHitTraceComponent::CaptureSamplePositions() const
{
	TArray<FVector> SamplePositions;
	if (!IsValid(TraceStart) || !IsValid(TraceEnd))
	{
		return SamplePositions;
	}

	SamplePositions.Reserve(SampleCount);
	// 骨骼已经生成本帧姿势，但 Notify 派发可能早于挂接子组件的变换传播。
	// 从当前手部 Socket 刷新武器变换，否则扫到的是上一帧的刀，窗口结束时尤其容易漏判。
	// 这里只更新武器及其子组件，不递归刷新角色骨骼，也不推进动画时间。
	if (AActor* Weapon = GetOwner())
		if (USceneComponent* Root = Weapon->GetRootComponent()) Root->UpdateComponentToWorld();
	const FVector StartLocation = TraceStart->GetComponentLocation();
	const FVector EndLocation = TraceEnd->GetComponentLocation();
	const float Denominator = static_cast<float>(FMath::Max(SampleCount - 1, 1));
	for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
	{
		const float Alpha = static_cast<float>(SampleIndex) / Denominator;
		SamplePositions.Add(FMath::Lerp(StartLocation, EndLocation, Alpha));
	}

	return SamplePositions;
}

/**
 * 无效或已命中的 Actor 不广播；先加入集合，再发事件，防止同步重入重复命中。
 */
void USCLHitTraceComponent::ProcessHit(const FHitResult& HitResult)
{
	AActor* const HitActor = HitResult.GetActor();
	if (!IsValid(HitActor) || HitActor == GetOwner())
	{
		return;
	}

	const TWeakObjectPtr<AActor> HitActorKey{HitActor};
	if (HitActors.Contains(HitActorKey))
	{
		return;
	}

	// 必须先记入集合再广播：监听者可能在同步伤害回调中再次进入检测或取消攻击。
	HitActors.Add(HitActorKey);
	if (bDebugDraw || USCLCombatDebugSubsystem::IsEnabledForWorld(GetWorld()))
	{
		DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 14.0F, FColor::Red,
			false, USCLCombatDebugSubsystem::EventLifetime);
	}
	OnTraceHit.Broadcast(HitActor, HitResult);
}
