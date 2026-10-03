#include "Data/SCLAttackData.h"

#include "GameplayTags/SCLGameplayTags.h"

USCLAttackData::USCLAttackData()
	: AttackTag{SCLGameplayTags::Ability_Attack_Light}
{
}

const FSCLAttackInfo* USCLAttackData::FindComboStep(const int32 StepIndex) const
{
	return LightCombo.IsValidIndex(StepIndex) ? &LightCombo[StepIndex] : nullptr;
}

int32 USCLAttackData::FindComboStepIndex(const FName MontageSection) const
{
	return LightCombo.IndexOfByPredicate(
		[MontageSection](const FSCLAttackInfo& Step)
		{
			return Step.MontageSection == MontageSection;
		});
}
