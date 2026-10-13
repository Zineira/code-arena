using UnrealBuildTool;

public class CodeArenaTarget : TargetRules
{
	public CodeArenaTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CodeArena");
	}
}
