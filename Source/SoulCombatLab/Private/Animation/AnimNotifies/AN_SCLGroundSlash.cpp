#include "Animation/AnimNotifies/AN_SCLGroundSlash.h"

#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"

UAN_SCLGroundSlash::UAN_SCLGroundSlash()
{
	// 地面爆发粒子生成后留在世界中，不随角色的根运动继续滑走。
	Attached = false;
}

void UAN_SCLGroundSlash::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !MeshComp->GetWorld()) return;
	Super::Notify(MeshComp, Animation, EventReference);
	const AActor* Owner = MeshComp->GetOwner();
	if (!Owner || !GroundMaterial) return;

	// 与同一 Notify 的 Niagara 使用相同落点偏移；不拿胶囊中心的高度当作地面高度。
	const FVector EffectPoint = MeshComp->GetSocketTransform(SocketName).TransformPosition(LocationOffset);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SCLGroundSlash), false, Owner);
	// 排除 Pawn 类型，避免刀落在敌人附近时把敌人的头顶误当成地面。
	FCollisionObjectQueryParams GroundObjects;
	GroundObjects.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult GroundHit;
	if (!MeshComp->GetWorld()->LineTraceSingleByObjectType(GroundHit,
		EffectPoint + FVector(0.0, 0.0, 100.0), EffectPoint - FVector(0.0, 0.0, 300.0),
		GroundObjects, Query) || GroundHit.ImpactNormal.Z < 0.5F)
	{
		// 没有地面或探到墙面时，只播放粒子，不凭空制造悬浮刀痕。
		return;
	}

	const FVector SlashForward = Owner->GetActorForwardVector().RotateAngleAxis(DirectionYawOffset, FVector::UpVector);
	const FVector GroundDirection = FVector::VectorPlaneProject(SlashForward, GroundHit.ImpactNormal).GetSafeNormal();
	if (GroundDirection.IsNearlyZero()) return;
	// UE DeferredDecal.usf 使用 UV=(局部 Z, 局部 Y)：纵向贴图长轴实际对应局部 Y。
	// X 朝地面投射，Y 沿竖劈平面与地面的交线；直接让 Z 沿前方会再次得到横向刀痕。
	const FRotator DecalRotation = FRotationMatrix::MakeFromXY(-GroundHit.ImpactNormal, GroundDirection).Rotator();
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(MeshComp, GroundMaterial, DecalSize,
		GroundHit.ImpactPoint, DecalRotation, 0.0F);
	if (Decal)
	{
		// World 负责生命周期；通知对象不保存每个角色的运行时组件，避免共享 Notify 串状态。
		Decal->SetFadeOut(HoldSeconds, FadeSeconds, false);
		Decal->SetLifeSpan(HoldSeconds + FadeSeconds + 0.1F);
	}
}
