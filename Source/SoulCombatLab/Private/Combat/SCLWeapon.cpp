#include "Combat/SCLWeapon.h"

#include "Combat/SCLHitTraceComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

ASCLWeapon::ASCLWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(SceneRoot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetRelativeScale3D(FVector{0.08F, 0.08F, 1.2F});

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (PlaceholderMesh.Succeeded())
	{
		WeaponMesh->SetStaticMesh(PlaceholderMesh.Object);
	}

	TraceStart = CreateDefaultSubobject<USceneComponent>(TEXT("TraceStart"));
	TraceStart->SetupAttachment(SceneRoot);
	TraceStart->SetRelativeLocation(FVector{0.0F, 0.0F, -60.0F});

	TraceEnd = CreateDefaultSubobject<USceneComponent>(TEXT("TraceEnd"));
	TraceEnd->SetupAttachment(SceneRoot);
	TraceEnd->SetRelativeLocation(FVector{0.0F, 0.0F, 60.0F});

	HitTraceComponent = CreateDefaultSubobject<USCLHitTraceComponent>(TEXT("HitTraceComponent"));
	HitTraceComponent->SetTraceEndpoints(TraceStart, TraceEnd);
}

ASCLGhostKatanaWeapon::ASCLGhostKatanaWeapon()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> KatanaMesh(
		TEXT("/Game/GhostSamurai_Bundle/GhostSamurai/Weapon/Mesh/Katana/SM_Katana01.SM_Katana01"));
	if (KatanaMesh.Succeeded())
	{
		WeaponMesh->SetStaticMesh(KatanaMesh.Object);
		WeaponMesh->SetRelativeScale3D(FVector::OneVector);
		TraceStart->SetRelativeLocation(FVector{0.0F, 0.0F, 0.0F});
		TraceEnd->SetRelativeLocation(FVector{0.0F, 95.0F, 0.0F});
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> HitSystem(
		TEXT("/Game/GhostSamurai_Bundle/GhostSamurai/VFX/Effect/Slash/NS_Slash_Hit_L.NS_Slash_Hit_L"));
	HitEffect = HitSystem.Object;
}

void ASCLGhostKatanaWeapon::PlayHitEffects(const FHitResult& HitResult)
{
	if (HitEffect == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	// 这套素材的斩击朝向按角色 Mesh 的局部坐标制作（Manny 的 Mesh 有 -90° 偏移）。
	// 胶囊碰撞法线只说明表面朝向，不代表刀砍方向；用它旋转整个刀光会随接触点翻转。
	const ACharacter* Wielder = Cast<ACharacter>(GetOwner());
	const FRotator EffectRotation(0.0F,
		Wielder && Wielder->GetMesh() ? Wielder->GetMesh()->GetComponentRotation().Yaw : GetActorRotation().Yaw,
		0.0F);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		HitEffect,
		HitResult.ImpactPoint,
		EffectRotation);
}
