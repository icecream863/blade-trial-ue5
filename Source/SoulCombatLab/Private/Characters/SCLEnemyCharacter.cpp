#include "Characters/SCLEnemyCharacter.h"
#include "Characters/Components/SCLActionMovementComponent.h"

#include "AI/SCLAIState.h"
#include "AI/SCLAIController.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "AbilitySystem/Abilities/SCLLightAttackAbility.h"
#include "Combat/SCLCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/SCLEnemyHealthWidget.h"
#include "Misc/App.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Data/SCLEnemyArchetypeData.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "SoulCombatLab.h"
#include "UObject/ConstructorHelpers.h"

ASCLEnemyCharacter::ASCLEnemyCharacter()
{
	AIControllerClass = ASCLAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	StartupAbilities.Add(USCLLightAttackAbility::StaticClass());
	ArchetypeDataClass = USCLSwordEnemyArchetypeData::StaticClass();
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;
	OverheadHealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadHealthBar"));
	OverheadHealthBar->SetupAttachment(RootComponent);
	OverheadHealthBar->SetRelativeLocation(FVector{0.0F, 0.0F, 160.0F});
	OverheadHealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	OverheadHealthBar->SetDrawSize(FVector2D{160.0F, 40.0F});
	OverheadHealthBar->SetPivot(FVector2D{0.5F, 1.0F});
	OverheadHealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OverheadHealthBar->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> EnemyMesh(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	if (EnemyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(EnemyMesh.Object);
		GetMesh()->SetRelativeLocation(FVector{0.0F, 0.0F, -90.0F});
		GetMesh()->SetRelativeRotation(FRotator{0.0F, -90.0F, 0.0F});
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> EnemyAnimation(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (EnemyAnimation.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(EnemyAnimation.Class);
	}
}

float ASCLEnemyCharacter::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* const EventInstigator,
	AActor* const DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(
		DamageAmount,
		DamageEvent,
		EventInstigator,
		DamageCauser);
	if (AppliedDamage <= 0.0F ||
		GetSCLAbilitySystemComponent() == nullptr ||
		GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
	{
		return AppliedDamage;
	}

	ASCLPlayerCharacter* const PlayerInstigator = [&]()
	{
		if (EventInstigator != nullptr)
		{
			if (ASCLPlayerCharacter* const InstigatorPawn =
				Cast<ASCLPlayerCharacter>(EventInstigator->GetPawn()))
			{
				return InstigatorPawn;
			}
		}
		return Cast<ASCLPlayerCharacter>(DamageCauser);
	}();
	ASCLAIController* const SCLController = Cast<ASCLAIController>(GetController());
	if (PlayerInstigator != nullptr && SCLController != nullptr)
	{
		SCLController->NotifyDamageReceived(*PlayerInstigator);
	}

	return AppliedDamage;
}

const USCLEnemyArchetypeData* ASCLEnemyCharacter::GetArchetypeData() const
{
	return ArchetypeDataClass != nullptr
		? GetDefault<USCLEnemyArchetypeData>(ArchetypeDataClass)
		: nullptr;
}

float ASCLEnemyCharacter::GetAttackRecoveryDuration() const
{
	const USCLEnemyArchetypeData* const Data = GetArchetypeData();
	return Data != nullptr ? Data->GetAttackRecoveryDuration() : 0.75F;
}

int32 ASCLEnemyCharacter::GetAttacksPerBurst() const
{
	const USCLEnemyArchetypeData* const Data = GetArchetypeData();
	return Data != nullptr ? Data->GetAttacksPerBurst() : 2;
}

float ASCLEnemyCharacter::GetCombatEnterDistance() const
{
	return SCLAIStatePolicy::CombatEnterDistance;
}

float ASCLEnemyCharacter::GetCombatExitDistance() const
{
	return SCLAIStatePolicy::CombatExitDistance;
}

void ASCLEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	const USCLEnemyArchetypeData* const Data = GetArchetypeData();
	USCLAbilitySystemComponent* const AbilitySystem = GetSCLAbilitySystemComponent();
	USCLCombatComponent* const Combat = GetCombatComponent_Implementation();
	if (Data == nullptr || AbilitySystem == nullptr || Combat == nullptr)
	{
		return;
	}

	GetActionMovementComponent()->SetBaseWalkSpeed(Data->GetMoveSpeed());
	AbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetMaxHealthAttribute(), Data->GetMaxHealth());
	AbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetHealthAttribute(), Data->GetMaxHealth());
	AbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetMaxPoiseAttribute(), Data->GetMaxPoise());
	AbilitySystem->SetNumericAttributeBase(
		USCLAttributeSet::GetPoiseAttribute(), Data->GetMaxPoise());
	const TArray<TObjectPtr<UMaterialInterface>>& BodyMaterials = Data->GetBodyMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < BodyMaterials.Num(); ++MaterialIndex)
	{
		if (BodyMaterials[MaterialIndex] != nullptr)
		{
			GetMesh()->SetMaterial(MaterialIndex, BodyMaterials[MaterialIndex]);
		}
	}
	Combat->ConfigureEnemyAttackProfile(
		Data->GetDamageScale(),
		Data->AreAttacksParryable(),
		Data->GetWeaponVisualScale());
	if (bShowOverheadHealthBar && FApp::CanEverRender())
	{
		OverheadHealthBar->SetWidgetClass(USCLEnemyHealthWidget::StaticClass());
		OverheadHealthBar->InitWidget();
		if (USCLEnemyHealthWidget* const Widget = Cast<USCLEnemyHealthWidget>(OverheadHealthBar->GetUserWidgetObject()))
			Widget->BindEnemy(*this);
	}

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Enemy archetype applied: Pawn=%s Archetype=%s Speed=%.0f Health=%.0f Poise=%.0f DamageScale=%.2f Burst=%d Recovery=%.2f Parryable=%s Materials=%d"),
		*GetNameSafe(this),
		*Data->GetArchetypeName().ToString(),
		Data->GetMoveSpeed(),
		Data->GetMaxHealth(),
		Data->GetMaxPoise(),
		Data->GetDamageScale(),
		Data->GetAttacksPerBurst(),
		Data->GetAttackRecoveryDuration(),
		Data->AreAttacksParryable() ? TEXT("true") : TEXT("false"),
		BodyMaterials.Num());
}

void ASCLEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USCLEnemyHealthWidget* const Widget = Cast<USCLEnemyHealthWidget>(OverheadHealthBar->GetUserWidgetObject()))
		Widget->UnbindEnemy();
	Super::EndPlay(EndPlayReason);
}

ASCLSwordEnemyCharacter::ASCLSwordEnemyCharacter()
{
	ArchetypeDataClass = USCLSwordEnemyArchetypeData::StaticClass();
}

ASCLHeavyEnemyCharacter::ASCLHeavyEnemyCharacter()
{
	ArchetypeDataClass = USCLHeavyEnemyArchetypeData::StaticClass();
}
