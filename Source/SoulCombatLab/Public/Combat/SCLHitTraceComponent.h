#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"

#include "SCLHitTraceComponent.generated.h"

class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSCLTraceHitSignature,
	AActor*,
	HitActor,
	const FHitResult&,
	HitResult);

// 在动画有效窗口采样武器轨迹，对命中目标去重并发出命中事件。
UCLASS(ClassGroup = "SoulCombatLab", meta = (BlueprintSpawnableComponent))
class SOULCOMBATLAB_API USCLHitTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USCLHitTraceComponent();

	// 武器提供刀根与刀尖组件，两端之间生成 SampleCount 个世界空间采样点。
	void SetTraceEndpoints(USceneComponent* InTraceStart, USceneComponent* InTraceEnd);

	// Notify Begin 调用：清空上一刀命中集合，记录武器采样点的初始位置。
	UFUNCTION(BlueprintCallable, Category = "Combat|Trace")
	bool BeginTrace();

	// Notify Tick 调用：从上帧到本帧逐点扫掠；同一目标在这一刀只广播一次。
	UFUNCTION(BlueprintCallable, Category = "Combat|Trace")
	// SweepFraction=1 扫完整两帧轨迹；窗口末尾小于 1 时截断到有效时间。
	void TickTrace(float SweepFraction = 1.0F);

	UFUNCTION(BlueprintCallable, Category = "Combat|Trace")
	/** 清空采样与去重集合并改变代次，使同步命中回调外的旧扫掠循环立即失效。 */
	void EndTrace();

	UFUNCTION(BlueprintPure, Category = "Combat|Trace")
	bool IsTraceActive() const { return bTraceActive; }

	UFUNCTION(BlueprintPure, Category = "Combat|Trace")
	int32 GetSampleCount() const { return SampleCount; }

	UPROPERTY(BlueprintAssignable, Category = "Combat|Trace")
	FSCLTraceHitSignature OnTraceHit;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Trace", meta = (ClampMin = "2", ClampMax = "12"))
	// 包含首尾端点的采样数；更多点增加 Sweep 次数，而不是扩大命中范围。
	int32 SampleCount{5};

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Trace", meta = (ClampMin = "0.1", Units = "cm"))
	// 每个采样点扫掠球的半径（厘米），影响检测宽度。
	float TraceRadius{6.0F};

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Trace")
	// 目标对象通道，默认只查询 Pawn；武器和持有者另在 QueryParams 中忽略。
	TEnumAsByte<ECollisionChannel> TraceObjectType{ECC_Pawn};

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Debug")
	bool bDebugDraw{false};

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> TraceStart;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> TraceEnd;

	// 上一帧各采样点的世界坐标；与当前帧一一配对进行 Sphere Sweep。
	TArray<FVector> PreviousSamplePositions;
	// 一个 Trace 窗口内按 Actor 去重；不是整个连招永久去重，新窗口会清空。
	TSet<TWeakObjectPtr<AActor>> HitActors;
	// 命中回调可能同步结束 Trace；代次变化时外层采样循环立即退出。
	uint64 TraceGeneration{0};
	bool bTraceActive{false};

	TArray<FVector> CaptureSamplePositions() const;
	// 先加去重集合再广播，防止事件同步重入造成重复命中。
	void ProcessHit(const FHitResult& HitResult);
};
