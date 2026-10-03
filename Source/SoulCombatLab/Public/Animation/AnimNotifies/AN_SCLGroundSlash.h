#pragma once

#include "CoreMinimal.h"
#include "AnimNotify_PlayNiagaraEffect.h"
#include "AN_SCLGroundSlash.generated.h"

class UMaterialInterface;

/** 竖劈落地表现：播放刀光粒子，并把刀痕沿攻击前方投影到实际地面。
 * 只负责画面，不结算伤害；伤害仍由 ANS_SCLWeaponTrace 控制。
 * 放在 Montage 的落刀时刻，避免用角色 Tick 或新增组件管理一次性效果。
 */
UCLASS(const, meta = (DisplayName = "SCL Ground Slash / 竖劈地面刀痕"))
class SOULCOMBATLAB_API UAN_SCLGroundSlash : public UAnimNotify_PlayNiagaraEffect
{
	GENERATED_BODY()

public:
	UAN_SCLGroundSlash();
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** 复用素材包的刀痕材质；Niagara 副本中的旧贴花已关闭，避免同时出现两条刀痕。 */
	UPROPERTY(EditAnywhere, Category = "Ground Slash", meta = (DisplayName = "刀痕材质"))
	TObjectPtr<UMaterialInterface> GroundMaterial;

	/** X 是投影厚度，Y 是贴图长轴，Z 是宽度；UE 贴花的 UV=(局部 Z, 局部 Y)。 */
	UPROPERTY(EditAnywhere, Category = "Ground Slash", meta = (DisplayName = "刀痕投影范围", ClampMin = "1.0"))
	FVector DecalSize{16.0, 160.0, 90.0};

	/** 当前竖劈沿角色前方；换成斜劈时可在该条 Notify 中调整平面角度。 */
	UPROPERTY(EditAnywhere, Category = "Ground Slash", meta = (DisplayName = "刀痕方向偏移（度）"))
	float DirectionYawOffset{0.0F};

	UPROPERTY(EditAnywhere, Category = "Ground Slash", meta = (DisplayName = "刀痕停留时间", ClampMin = "0.0"))
	float HoldSeconds{1.5F};

	UPROPERTY(EditAnywhere, Category = "Ground Slash", meta = (DisplayName = "刀痕淡出时间", ClampMin = "0.01"))
	float FadeSeconds{0.5F};
};
