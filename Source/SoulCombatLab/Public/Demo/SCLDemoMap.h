#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"
#include "SCLDemoMap.generated.h"

enum class ESCLDemoStage : uint8;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UTextureCube;

// Demo 使用的导航系统类型，配合扩展区域的导航数据。
UCLASS()
class USCLDemoNavigationSystem final : public UNavigationSystemV1
{
	GENERATED_BODY()
public:
	void UseDemoNavigation();
};

// Demo 专用 Recast 导航数据 Actor。
UCLASS()
class ASCLDemoNavigation final : public ARecastNavMesh
{
	GENERATED_BODY()
public:
	ASCLDemoNavigation();
};

// 六区域 Demo 的场景 Actor：创建区域几何、门和场景元素；重试战斗时保留。
// 本类里的位置和尺寸使用 Unreal 默认的厘米单位。
UCLASS()
class SOULCOMBATLAB_API ASCLDemoMap final : public AActor
{
	GENERATED_BODY()
public:
	ASCLDemoMap();
	void BuildMap();
	void UpdateGates(ESCLDemoStage Stage, bool bCleared);
	static bool ShouldUseDemo(const UWorld* World);
	static FVector Checkpoint(ESCLDemoStage Stage);
	static bool IsInsideEntry(ESCLDemoStage Stage, const FVector& Position);
	UStaticMeshComponent* GetExitGate(ESCLDemoStage Stage) const;
	static constexpr float FloorMinX = -1200.0F;
	static constexpr float FloorMaxX = 15800.0F;
	static constexpr float FloorHalfWidth = 1400.0F;
private:
	UStaticMeshComponent* AddBlock(FName Name, const FVector& Center, const FVector& Size,
		const FLinearColor& Color, bool bSolid = true);
	void AddSign(const TCHAR* Text, const FVector& Location, const FColor& Color);
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
	UPROPERTY() TObjectPtr<UTextureCube> AmbientCube;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Gates;
	bool bBuilt{false};
};
