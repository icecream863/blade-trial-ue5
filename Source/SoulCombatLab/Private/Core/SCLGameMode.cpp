#include "Core/SCLGameMode.h"

#include "Characters/Player/SCLPlayerCharacter.h"
#include "UI/SCLHUD.h"
#include "Demo/SCLDemoPlayerController.h"
#include "Demo/SCLDemoMap.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "SoulCombatLab.h"

ASCLGameMode::ASCLGameMode()
{
	PlayerCharacterClass = ASCLPlayerCharacter::StaticClass();
	DefaultPawnClass = PlayerCharacterClass;
	HUDClass = ASCLHUD::StaticClass();
	PlayerControllerClass = ASCLDemoPlayerController::StaticClass();
}

UClass* ASCLGameMode::GetDefaultPawnClassForController_Implementation(AController* const InController)
{
	return PlayerCharacterClass != nullptr
		? PlayerCharacterClass.Get()
		: Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ASCLGameMode::StartPlay()
{
	if (ASCLDemoMap::ShouldUseDemo(GetWorld()))
	{
		if (ASCLDemoMap* const DemoMap = GetWorld() != nullptr
			? GetWorld()->SpawnActor<ASCLDemoMap>()
			: nullptr)
		{
			DemoMap->BuildMap();
		}
		else
		{
			UE_LOG(LogSoulCombatLab, Error, TEXT("Demo map generation failed: unable to spawn ASCLDemoMap"));
		}
		Super::StartPlay();
		return;
	}
	// Runtime collision also works in the packaged map without editor-only brushes.
	if (UGameplayStatics::GetCurrentLevelName(this, true) == TEXT("L_CombatLab"))
	{
		FBox FloorBounds{ForceInit};
		float LargestArea = 0.0F;
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			const UStaticMeshComponent* const Mesh = It->GetStaticMeshComponent();
			if (Mesh == nullptr || !Mesh->IsCollisionEnabled()) continue;
			const FBox Bounds = Mesh->Bounds.GetBox();
			const FVector Size = Bounds.GetSize();
			const float Area = Size.X * Size.Y;
			if (Size.X > 1000.0F && Size.Y > 1000.0F && Size.Z < 600.0F && Area > LargestArea)
			{
				LargestArea = Area;
				FloorBounds = Bounds;
			}
		}
		if (FloorBounds.IsValid)
		{
			AActor* const Boundary = GetWorld()->SpawnActor<AActor>();
			if (Boundary == nullptr)
			{
				UE_LOG(LogSoulCombatLab, Error, TEXT("Arena boundary setup failed: unable to spawn boundary actor"));
				Super::StartPlay();
				return;
			}
			Boundary->Tags.Add(TEXT("SCLArenaBoundary"));
			const FVector Center = FloorBounds.GetCenter();
			const FVector Half = FloorBounds.GetExtent();
			constexpr float Thickness = 40.0F;
			constexpr float Height = 1500.0F;
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const bool bXAxis = Side < 2;
				const float Sign = Side % 2 == 0 ? -1.0F : 1.0F;
				UBoxComponent* const Wall = NewObject<UBoxComponent>(Boundary);
				Boundary->AddInstanceComponent(Wall);
				Wall->SetBoxExtent(bXAxis ? FVector{Thickness, Half.Y, Height}
					: FVector{Half.X, Thickness, Height});
				Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Wall->SetCollisionObjectType(ECC_WorldStatic);
				Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
				Wall->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				Wall->SetCanEverAffectNavigation(false);
				Wall->SetGenerateOverlapEvents(false);
				Wall->RegisterComponent();
				Wall->SetWorldLocation(FVector{Center.X + (bXAxis ? Sign * (Half.X - Thickness) : 0.0F),
					Center.Y + (bXAxis ? 0.0F : Sign * (Half.Y - Thickness)), FloorBounds.Max.Z + Height - 100.0F});
			}
			UE_LOG(LogSoulCombatLab, Log, TEXT("Arena boundaries ready: Floor=%s Walls=4"), *FloorBounds.ToString());
		}
		else UE_LOG(LogSoulCombatLab, Error, TEXT("Arena boundary setup failed: no arena floor bounds"));
	}
	Super::StartPlay();
}
