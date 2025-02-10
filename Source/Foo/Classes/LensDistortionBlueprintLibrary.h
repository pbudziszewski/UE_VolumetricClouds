// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LensDistortionAPI.h"
#include "LensDistortionBlueprintLibrary.generated.h"


UCLASS(MinimalAPI, meta=(ScriptName="FooLibrary"))
class UFooBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category = "Foo", meta = (WorldContext = "WorldContextObject"))
	static void DrawToRenderTarget(
		const UObject* WorldContextObject,
		const FDrawToTargetParams& Params,
		class UTextureRenderTarget2D* OutputRenderTarget
		);

	/* Returns true if A is equal to B (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (DrawToTargetControl)", CompactNodeTitle = "==", Keywords = "== equal"), Category = "Foo")
	static bool EqualEqual_CompareLensDistortionModels(
		const FDrawToTargetParams& A,
		const FDrawToTargetParams& B)
	{
		return A == B;
	}

	/* Returns true if A is not equal to B (A != B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "NotEqual (DrawToTargetControl)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category = "Foo")
	static bool NotEqual_CompareLensDistortionModels(
		const FDrawToTargetParams& A,
		const FDrawToTargetParams& B)
	{
		return A != B;
	}
};
