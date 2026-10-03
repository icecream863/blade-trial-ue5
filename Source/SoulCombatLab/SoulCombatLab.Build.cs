using UnrealBuildTool;

public class SoulCombatLab : ModuleRules
{
	public SoulCombatLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"AIModule",
			"Core",
			"CoreUObject",
			"Engine",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"InputCore",
			"MotionWarping",
			"NavigationSystem",
			"Niagara",
			"NiagaraAnimNotifies",
			"Slate",
			"SlateCore",
			"UMG"
		});
		// 仅编辑器生成普通动画状态机时需要；打包游戏只运行编译后的 AnimBP。
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "AnimGraph", "BlueprintGraph" });
		}

	}
}
