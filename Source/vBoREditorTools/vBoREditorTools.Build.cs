using UnrealBuildTool;
using System.IO;

public class vBoREditorTools : ModuleRules
{
	public vBoREditorTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// 기존 runtime 헤더 배치를 유지함 (참조는 editor → runtime 만, P5j D1)
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "vBoR"));
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "vBoR", "UnrealEd", "ToolMenus", "Slate", "SlateCore"
		});
	}
}
