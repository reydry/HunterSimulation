// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HunterSimulation : ModuleRules
{
	public HunterSimulation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"HunterSimulation",
			"HunterSimulation/Variant_Strategy",
			"HunterSimulation/Variant_Strategy/UI",
			"HunterSimulation/Variant_TwinStick",
			"HunterSimulation/Variant_TwinStick/AI",
			"HunterSimulation/Variant_TwinStick/Gameplay",
			"HunterSimulation/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
