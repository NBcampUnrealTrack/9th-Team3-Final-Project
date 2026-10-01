// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(Logknight, Log, All);

/** Domain log categories. Filter one system with `Log LogknightCombat Verbose`. */
DECLARE_LOG_CATEGORY_EXTERN(LogknightAbility, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogknightCombat, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogknightAI, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogknightCamera, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogknightData, Log, All);
