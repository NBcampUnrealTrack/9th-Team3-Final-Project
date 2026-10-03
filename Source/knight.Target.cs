// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class knightTarget : TargetRules
{
	public knightTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "knight", "knightUI" });

		// 개발 도구(치트, 테스트 도우미)는 Shipping 빌드에서 제외한다.
		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			ExtraModuleNames.Add("knightDev");
		}
	}
}
