// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LensDistortionAPI.generated.h"


USTRUCT(BlueprintType)
struct FDrawToTargetParams
{
	GENERATED_USTRUCT_BODY()
	FDrawToTargetParams()
		:
		Scale(0.0f, 0.0f)
	{
		Blah = 0.0f;
	}

	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Foo")
	float Blah;

	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Foo")
	FVector2D Scale;

	void DrawToRenderTarget(
		class UWorld* World,
		class UTextureRenderTarget2D* OutputRenderTarget) const;

	bool operator == (const FDrawToTargetParams& Other) const
	{
		return (
			Blah == Other.Blah);
	}

	bool operator != (const FDrawToTargetParams& Other) const
	{
		return !(*this == Other);
	}
};
