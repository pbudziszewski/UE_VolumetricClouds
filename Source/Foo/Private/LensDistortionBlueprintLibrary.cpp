#include "LensDistortionBlueprintLibrary.h"


UFooBlueprintLibrary::UFooBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }


// static
void UFooBlueprintLibrary::DrawToRenderTarget(
	const UObject* WorldContextObject,
	const FDrawToTargetParams& Params,
	class UTextureRenderTarget2D* OutputRenderTarget)
{
	Params.DrawToRenderTarget(
		WorldContextObject->GetWorld(),
		OutputRenderTarget);
}
