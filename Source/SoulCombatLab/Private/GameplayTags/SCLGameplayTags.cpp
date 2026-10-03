#include "GameplayTags/SCLGameplayTags.h"

namespace SCLGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(State_Attacking, "State.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Dodging, "State.Dodging");
	UE_DEFINE_GAMEPLAY_TAG(State_Blocking, "State.Blocking");
	UE_DEFINE_GAMEPLAY_TAG(State_Parrying, "State.Parrying");
	UE_DEFINE_GAMEPLAY_TAG(State_ParryAction, "State.ParryAction");
	UE_DEFINE_GAMEPLAY_TAG(State_Staggered, "State.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Invincible, "State.Invincible");
	UE_DEFINE_GAMEPLAY_TAG(State_Executable, "State.Executable");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Attack_Light, "Ability.Attack.Light");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Attack_Heavy, "Ability.Attack.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Dodge, "Ability.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Block, "Ability.Block");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Parry, "Ability.Parry");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Execution, "Ability.Execution");

	UE_DEFINE_GAMEPLAY_TAG(Event_Hit, "Event.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Event_Parry, "Event.Parry");
	UE_DEFINE_GAMEPLAY_TAG(Event_Execution, "Event.Execution");

	UE_DEFINE_GAMEPLAY_TAG(Data_Cost_Stamina, "Data.Cost.Stamina");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_Poise, "Data.Damage.Poise");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_Guard, "Data.Damage.Guard");
}
