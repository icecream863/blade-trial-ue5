#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_SCLSheathWeapon.generated.h"
// 收刀时间轴上的单次入鞘事件；只改变挂点，不开武器碰撞或命中窗口。
UCLASS()
class SOULCOMBATLAB_API UAN_SCLSheathWeapon : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("SCL 刀入鞘"); }
};
