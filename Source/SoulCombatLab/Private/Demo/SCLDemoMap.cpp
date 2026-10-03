#include "Demo/SCLDemoMap.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "SoulCombatLab.h"
#include "UObject/ConstructorHelpers.h"

ASCLDemoNavigation::ASCLDemoNavigation()
{
	RuntimeGeneration = ERuntimeGenerationType::Dynamic;
	bForceRebuildOnLoad = true;
}

void USCLDemoNavigationSystem::UseDemoNavigation()
{
	// Configure this world instance only; lab/profiling worlds keep their inherited setup.
	if (!SupportedAgents.IsEmpty())
	{
		SupportedAgents[0].SetNavDataClass(ASCLDemoNavigation::StaticClass());
		SupportedAgents[0].SetPreferredNavData(ASCLDemoNavigation::StaticClass());
	}
}

ASCLDemoMap::ASCLDemoMap()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* const Scene = CreateDefaultSubobject<USceneComponent>(TEXT("MapRoot"));
	Scene->SetMobility(EComponentMobility::Static);
	SetRootComponent(Scene);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UTextureCube> AmbientAsset(TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
	Cube = CubeAsset.Object;
	BaseMaterial = MaterialAsset.Object;
	AmbientCube = AmbientAsset.Object;
}

bool ASCLDemoMap::ShouldUseDemo(const UWorld* World)
{
	FString Profile;
	return World != nullptr && UGameplayStatics::GetCurrentLevelName(World, true) == TEXT("L_CombatLab") &&
		!FParse::Param(FCommandLine::Get(), TEXT("SCLLab")) &&
		!FParse::Value(FCommandLine::Get(), TEXT("SCLProfile="), Profile);
}

UStaticMeshComponent* ASCLDemoMap::AddBlock(const FName Name, const FVector& Center,
	const FVector& Size, const FLinearColor& Color, const bool bSolid)
{
	UStaticMeshComponent* const Mesh = NewObject<UStaticMeshComponent>(this, Name);
	AddInstanceComponent(Mesh);
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetStaticMesh(Cube);
	Mesh->SetRelativeLocation(Center);
	Mesh->SetRelativeScale3D(Size / 100.0F);
	Mesh->SetMobility(EComponentMobility::Static);
	Mesh->SetCollisionProfileName(bSolid ? TEXT("BlockAll") : TEXT("NoCollision"));
	// Interior doors/walls must shorten the spring arm instead of hiding the player behind them.
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	Mesh->SetCanEverAffectNavigation(bSolid);
	Mesh->SetGenerateOverlapEvents(false);
	if (UMaterialInstanceDynamic* const Material = UMaterialInstanceDynamic::Create(BaseMaterial, this))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Mesh->SetMaterial(0, Material);
	}
	Mesh->RegisterComponent();
	return Mesh;
}

void ASCLDemoMap::AddSign(const TCHAR* Text, const FVector& Location, const FColor& Color)
{
	UTextRenderComponent* const Sign = NewObject<UTextRenderComponent>(this);
	AddInstanceComponent(Sign);
	Sign->SetupAttachment(GetRootComponent());
	Sign->SetRelativeLocation(Location);
	Sign->SetRelativeRotation(FRotator{0.0F, 180.0F, 0.0F});
	Sign->SetText(FText::FromString(Text));
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetWorldSize(65.0F);
	Sign->SetTextRenderColor(Color);
	Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sign->SetCanEverAffectNavigation(false);
	Sign->RegisterComponent();
}

void ASCLDemoMap::BuildMap()
{
	if (bBuilt) return;
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogSoulCombatLab, Error, TEXT("Six-region map generation failed: world is unavailable"));
		return;
	}
	bBuilt = true;
	// Retain the original lab asset/lights. Only its demo instance replaces geometry and nav data.
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It) It->Destroy();
	for (TActorIterator<ANavigationData> It(GetWorld()); It; ++It) It->Destroy();
	for (TActorIterator<ANavMeshBoundsVolume> It(GetWorld()); It; ++It) It->Destroy();
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It) It->Destroy();
	ASkyLight* const Ambient = World->SpawnActor<ASkyLight>();
	USkyLightComponent* const Sky = Ambient != nullptr ? Ambient->GetLightComponent() : nullptr;
	if (Sky == nullptr)
	{
		bBuilt = false;
		UE_LOG(LogSoulCombatLab, Error, TEXT("Six-region map generation failed: unable to create sky light"));
		return;
	}
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SourceType = SLS_SpecifiedCubemap;
	Sky->SetCubemap(AmbientCube);
	Sky->SetIntensity(1.0F);
	Sky->SetCastShadows(false);
	const FLinearColor Stone{0.20F, 0.23F, 0.27F};
	const FLinearColor Trim{0.08F, 0.12F, 0.16F};
	const FLinearColor Gold{0.9F, 0.55F, 0.12F};
	AddBlock(TEXT("ContinuousFloor"), FVector{7300.0F, 0.0F, -100.0F}, FVector{17000.0F, 2800.0F, 200.0F}, Stone);
	// A visible low parapet explains the edge; a taller invisible Pawn-only wall stops jumps/dashes.
	AddBlock(TEXT("NorthParapet"), FVector{7300.0F, 1390.0F, 65.0F}, FVector{17000.0F, 100.0F, 130.0F}, Trim);
	AddBlock(TEXT("SouthParapet"), FVector{7300.0F, -1390.0F, 65.0F}, FVector{17000.0F, 100.0F, 130.0F}, Trim);
	AddBlock(TEXT("SpawnBack"), FVector{-1160.0F, 0.0F, 150.0F}, FVector{80.0F, 2800.0F, 300.0F}, Trim);
	AddBlock(TEXT("BossBack"), FVector{15760.0F, 0.0F, 150.0F}, FVector{80.0F, 2800.0F, 300.0F}, Trim);
	AActor* const Boundary = GetWorld()->SpawnActor<AActor>();
	if (Boundary == nullptr)
	{
		bBuilt = false;
		UE_LOG(LogSoulCombatLab, Error, TEXT("Six-region map generation failed: unable to spawn boundary actor"));
		return;
	}
	Boundary->Tags.Add(TEXT("SCLArenaBoundary"));
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const bool bX = Side < 2;
		UBoxComponent* const Wall = NewObject<UBoxComponent>(Boundary);
		Boundary->AddInstanceComponent(Wall);
		Wall->SetBoxExtent(bX ? FVector{40.0F, 1400.0F, 1500.0F} : FVector{8500.0F, 40.0F, 1500.0F});
		Wall->SetWorldLocation(FVector{bX ? (Side == 0 ? -1160.0F : 15760.0F) : 7300.0F,
			bX ? 0.0F : (Side == 2 ? -1360.0F : 1360.0F), 1450.0F});
		Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Wall->SetCollisionObjectType(ECC_WorldStatic);
		Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
		Wall->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Wall->SetCanEverAffectNavigation(false);
		Wall->RegisterComponent();
	}
	// Doorways are 1000 cm wide. Full-height dividers prevent bypassing locked gates by jumping.
	const float Dividers[]{900.0F, 3300.0F, 5900.0F, 8500.0F, 11600.0F, 12400.0F};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Dividers); ++Index)
	{
		const float X = Dividers[Index];
		for (const float Y : {-950.0F, 950.0F})
			AddBlock(*FString::Printf(TEXT("Divider%d_%d"), Index, Y > 0 ? 1 : 0),
				FVector{X, Y, 750.0F}, FVector{100.0F, 900.0F, 1500.0F}, Trim);
		AddBlock(*FString::Printf(TEXT("Lintel%d"), Index), FVector{X, 0.0F, 780.0F}, FVector{140.0F, 1000.0F, 140.0F}, Stone);
		if (Index > 0)
		{
			Gates.Add(AddBlock(*FString::Printf(TEXT("Gate%d"), Index), FVector{X, 0.0F, 750.0F},
				FVector{60.0F, 1000.0F, 1500.0F}, Index == 4 ? Gold : FLinearColor{0.25F, 0.4F, 0.5F}));
		}
	}
	const TCHAR* Signs[]{TEXT("01  SPAWN  >"), TEXT("02  TRAINING  >"), TEXT("03A  SWORD  >"),
		TEXT("03B  HEAVY  >"), TEXT("04  MINI ARENA  >"), TEXT("05  BOSS GATE  >"), TEXT("06  BOSS ARENA")};
	const float SignX[]{800.0F, 3150.0F, 5750.0F, 8350.0F, 11450.0F, 12250.0F, 15500.0F};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SignX); ++Index)
	{
		AddSign(Signs[Index], FVector{SignX[Index], 0.0F, 620.0F}, Index >= 5 ? FColor::Orange : FColor::Cyan);
		// Non-colliding floor strips connect every doorway into a readable route.
		const float BeginX = Index == 0 ? -400.0F : SignX[Index - 1] + 170.0F;
		AddBlock(*FString::Printf(TEXT("Route%d"), Index), FVector{(BeginX + SignX[Index]) * 0.5F, 0.0F, 1.0F},
			FVector{SignX[Index] - BeginX, 28.0F, 2.0F}, Gold, false);
	}
	// Obstacles sit clear of the central route but give the multi-enemy arena useful spacing/LOS.
	for (const float X : {9700.0F, 10600.0F, 14100.0F})
		for (const float Y : {-900.0F, 900.0F})
			AddBlock(*FString::Printf(TEXT("Pillar%d_%d"), static_cast<int32>(X), static_cast<int32>(Y)),
				FVector{X, Y, 180.0F}, FVector{180.0F, 180.0F, 360.0F}, Stone);
	ANavMeshBoundsVolume* const Bounds = GetWorld()->SpawnActor<ANavMeshBoundsVolume>();
	UBoxComponent* const NavBox = NewObject<UBoxComponent>(Bounds);
	Bounds->AddInstanceComponent(NavBox);
	NavBox->SetupAttachment(Bounds->GetRootComponent());
	NavBox->SetBoxExtent(FVector{8600.0F, 1500.0F, 700.0F});
	NavBox->SetRelativeLocation(FVector{7300.0F, 0.0F, 200.0F});
	NavBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavBox->SetCanEverAffectNavigation(false);
	NavBox->RegisterComponent();
	if (USCLDemoNavigationSystem* const Nav = FNavigationSystem::GetCurrent<USCLDemoNavigationSystem>(GetWorld()))
	{
		// Registration compares the exact class, not IsA; configure this world before spawning.
		Nav->UseDemoNavigation();
		ASCLDemoNavigation* const Data = GetWorld()->SpawnActor<ASCLDemoNavigation>();
		if (Data != nullptr && !Nav->GetSupportedAgents().IsEmpty()) Data->SetConfig(Nav->GetSupportedAgents()[0]);
		Nav->OnNavigationBoundsUpdated(Bounds);
	}
	else UE_LOG(LogSoulCombatLab, Error, TEXT("Six-region navigation system missing; check engine configuration"));
	UpdateGates(ESCLDemoStage::Spawn, false);
	UE_LOG(LogSoulCombatLab, Log, TEXT("Six-region map ready: X=-1200..15800 Y=-1400..1400 Gates=5 DynamicNav=1"));
}

FVector ASCLDemoMap::Checkpoint(const ESCLDemoStage Stage)
{
	switch (Stage)
	{
	case ESCLDemoStage::Spawn: return FVector{0.0F, 0.0F, 100.0F};
	case ESCLDemoStage::Training: return FVector{1250.0F, 0.0F, 100.0F};
	case ESCLDemoStage::Sword: return FVector{3650.0F, 0.0F, 100.0F};
	case ESCLDemoStage::Heavy: return FVector{6250.0F, 0.0F, 100.0F};
	case ESCLDemoStage::MiniArena: return FVector{8850.0F, 0.0F, 100.0F};
	case ESCLDemoStage::BossGate: return FVector{11900.0F, 0.0F, 100.0F};
	case ESCLDemoStage::Boss: return FVector{12750.0F, 0.0F, 100.0F};
	}
	return FVector::ZeroVector;
}

bool ASCLDemoMap::IsInsideEntry(const ESCLDemoStage Stage, const FVector& Position)
{
	const FVector Entry = Checkpoint(Stage);
	return Position.X >= Entry.X - 100.0F && Position.X <= Entry.X + 500.0F &&
		FMath::Abs(Position.Y) < 1100.0F && Position.Z > 0.0F && Position.Z < 400.0F;
}

UStaticMeshComponent* ASCLDemoMap::GetExitGate(const ESCLDemoStage Stage) const
{
	const int32 Index = static_cast<int32>(Stage) - static_cast<int32>(ESCLDemoStage::Training);
	return Gates.IsValidIndex(Index) ? Gates[Index].Get() : nullptr;
}

void ASCLDemoMap::UpdateGates(const ESCLDemoStage Stage, const bool bCleared)
{
	for (int32 Index = 0; Index < Gates.Num(); ++Index)
	{
		const auto OwnerStage = static_cast<ESCLDemoStage>(Index + static_cast<int32>(ESCLDemoStage::Training));
		// The last gate is the Boss entrance: open in the antechamber, closed during the boss fight.
		const bool bOpen = Index == 4 ? Stage == ESCLDemoStage::BossGate || (Stage == ESCLDemoStage::Boss && bCleared)
			: Stage > OwnerStage || (Stage == OwnerStage && bCleared);
		Gates[Index]->SetVisibility(!bOpen);
		Gates[Index]->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
}
