using UnrealBuildTool;

public class CodeArenaEditorTarget : TargetRules
{
	public CodeArenaEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CodeArena");
	}
}
