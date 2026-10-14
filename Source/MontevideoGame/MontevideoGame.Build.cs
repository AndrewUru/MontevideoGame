// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MontevideoGame : ModuleRules
{
	public MontevideoGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"MontevideoGame",
			"MontevideoGame/Variant_Platforming",
			"MontevideoGame/Variant_Platforming/Animation",
			"MontevideoGame/Variant_Combat",
			"MontevideoGame/Variant_Combat/AI",
			"MontevideoGame/Variant_Combat/Animation",
			"MontevideoGame/Variant_Combat/Gameplay",
			"MontevideoGame/Variant_Combat/Interfaces",
			"MontevideoGame/Variant_Combat/UI",
			"MontevideoGame/Variant_SideScrolling",
			"MontevideoGame/Variant_SideScrolling/AI",
			"MontevideoGame/Variant_SideScrolling/Gameplay",
			"MontevideoGame/Variant_SideScrolling/Interfaces",
			"MontevideoGame/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
