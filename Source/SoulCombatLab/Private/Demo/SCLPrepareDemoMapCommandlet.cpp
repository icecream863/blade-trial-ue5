#include "Demo/SCLPrepareDemoMapCommandlet.h"
#include "Demo/SCLDemoMap.h"
#include "AI/NavigationSystemConfig.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/PackageName.h"
#include "SoulCombatLab.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

USCLPrepareDemoMapCommandlet::USCLPrepareDemoMapCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 USCLPrepareDemoMapCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const TCHAR* const PackageName = TEXT("/Game/Maps/L_CombatLab");
	UPackage* const Package = LoadPackage(nullptr, PackageName, LOAD_None);
	UWorld* const World = Package != nullptr ? UWorld::FindWorldInPackage(Package) : nullptr;
	if (World == nullptr || World->GetWorldSettings() == nullptr) return 1;
	UNavigationSystemConfig* const Config = World->GetWorldSettings()->GetNavigationSystemConfig();
	if (Config == nullptr)
	{
		UE_LOG(LogSoulCombatLab, Error, TEXT("Map has no navigation configuration"));
		return 1;
	}
	Config->NavigationSystemClass = FSoftClassPath(USCLDemoNavigationSystem::StaticClass());
	Package->MarkPackageDirty();
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetMapPackageExtension());
	if (!UPackage::SavePackage(Package, World, *Filename, SaveArgs)) return 1;
	UE_LOG(LogSoulCombatLab, Log, TEXT("Prepared demo map navigation: %s -> %s"), *Filename, *Config->NavigationSystemClass.ToString());
	return 0;
#else
	return 1;
#endif
}
