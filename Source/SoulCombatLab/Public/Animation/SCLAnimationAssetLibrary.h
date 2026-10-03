#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "SCLAnimationAssetLibrary.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class UBlendSpace;
class UAnimBlueprint;
class UAnimSequence;

// 动画编辑辅助函数：生成项目动画资源的编辑器工具，游戏运行时不调用这些配置入口。
UCLASS()
class SOULCOMBATLAB_API USCLAnimationAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 复用现有 Slot 下的地面姿势，加入可在 AnimBP 中编辑的起跳/空中/落地状态机。 */
	UFUNCTION(BlueprintCallable, Category = "Soul Combat Lab|Editor")
	static bool ConfigurePlayerJumpStateMachine(UAnimBlueprint* Blueprint,
		UAnimSequence* JumpStart, UAnimSequence* JumpLoop, UAnimSequence* JumpLand);

	/** Python 写入 Blend Space 采样后，重建运行时三角剖分；只有采样列表并不能保证动画会播放。 */
	UFUNCTION(BlueprintCallable, Category = "Soul Combat Lab|Editor")
	static bool RebuildBlendSpaceSampling(UBlendSpace* BlendSpace);

	/** 读取资产是否真的包含可供运行时插值的三角剖分。 */
	UFUNCTION(BlueprintPure, Category = "Soul Combat Lab|Editor")
	static bool HasBlendSpaceSampling(const UBlendSpace* BlendSpace);

	UFUNCTION(BlueprintCallable, Category = "Soul Combat Lab|Editor")
	static bool ConfigureComboSections(
		UAnimMontage* Montage,
		const TArray<UAnimSequenceBase*>& Animations,
		const TArray<FName>& SectionNames);

	UFUNCTION(BlueprintPure, Category = "Soul Combat Lab|Editor")
	static TArray<float> GetComboSectionStartTimes(const UAnimMontage* Montage);
};
