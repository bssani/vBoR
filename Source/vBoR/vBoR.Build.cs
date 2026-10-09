// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class vBoR : ModuleRules
{
	public vBoR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "OculusInteraction", "OculusInteractionPrebuilts" });

		PrivateDependencyModuleNames.AddRange(new string[] { "OculusXRInput", "HeadMountedDisplay", "XRBase", "OculusInteractionLibrary" });

		// P0-b 네이티브 경계 시험용 헤더 (샘플 참조 없음)
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "../../Plugins/MetaXRInteraction/Source/ThirdParty/OculusInteractionLibrary/Include"));

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
