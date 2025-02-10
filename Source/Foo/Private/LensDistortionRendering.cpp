// ================================================================================================

#include "LensDistortionAPI.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RHIStaticStates.h"
#include "SceneInterface.h"
#include "ShaderParameterUtils.h"
#include "Logging/MessageLog.h"
#include "TextureResource.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderingThread.h"


#define LOCTEXT_NAMESPACE "FooPlugin"


/**
 * Internal intermediary structure derived from FDrawToTargetParams by the game thread
 * to hand to the render thread.
 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
struct FCompiledParams
{
	FDrawToTargetParams OriginalParams;

	//
};

// ================================================================================================

class FFullScreenShader : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FFullScreenShader, NonVirtual);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FFullScreenShader() {}

	FFullScreenShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PixelUVSize.Bind(Initializer.ParameterMap, TEXT("PixelUVSize"));
		Scale.Bind(Initializer.ParameterMap, TEXT("Scale"));
		SystemTime.Bind(Initializer.ParameterMap, TEXT("SystemTime"));
	}

	void SetParameters(
		FRHIBatchedShaderParameters& BatchedParameters,
		const FCompiledParams& CompiledParams,
		const FIntPoint& TargetResolution)
	{
		FVector2f PixelUVSizeValue(1.0f / float(TargetResolution.X), 1.0f / float(TargetResolution.Y));

		FVector2f ScaleValue(CompiledParams.OriginalParams.Scale);

		float SystemTimeValue = FApp::GetGameTime();
		UE_LOG(LogTemp, Warning, TEXT("time %f"), SystemTimeValue);

		SetShaderValue(BatchedParameters, PixelUVSize, PixelUVSizeValue);
		SetShaderValue(BatchedParameters, Scale, ScaleValue);
		SetShaderValue(BatchedParameters, SystemTime, SystemTimeValue);
	}

private:

	LAYOUT_FIELD(FShaderParameter, PixelUVSize);
	LAYOUT_FIELD(FShaderParameter, Scale);
	LAYOUT_FIELD(FShaderParameter, SystemTime);
};


// ================================================================================================

class FFullScreenVS : public FFullScreenShader
{
	DECLARE_SHADER_TYPE(FFullScreenVS, Global);
public:

	FFullScreenVS() {}

	FFullScreenVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FFullScreenShader(Initializer)
	{
	}
};

// ================================================================================================

class FFullScreenPS : public FFullScreenShader
{
	DECLARE_SHADER_TYPE(FFullScreenPS, Global);
public:

	FFullScreenPS() {}

	FFullScreenPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FFullScreenShader(Initializer)
	{
	}
};

// ================================================================================================

IMPLEMENT_SHADER_TYPE(, FFullScreenVS, TEXT("/Plugin/Foo/Private/MyShader.usf"), TEXT("MainVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FFullScreenPS, TEXT("/Plugin/Foo/Private/MyShader.usf"), TEXT("MainPS"), SF_Pixel)

// ================================================================================================

static void DrawToRenderTarget_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	const FCompiledParams& CompiledParams,
	FTextureRenderTargetResource* OutTextureRenderTargetResource,
	ERHIFeatureLevel::Type FeatureLevel)
{
	check(IsInRenderingThread());

	SCOPED_DRAW_EVENT(RHICmdList, DrawToRenderTarget_RenderThread);

	FRHITexture2D* RenderTargetTexture = OutTextureRenderTargetResource->GetRenderTargetTexture();

	RHICmdList.Transition(FRHITransitionInfo(RenderTargetTexture, ERHIAccess::SRVMask, ERHIAccess::RTV));

	FRHIRenderPassInfo RPInfo(RenderTargetTexture, ERenderTargetActions::DontLoad_Store);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("Draw"));
	{
		FIntPoint TargetResolution(OutTextureRenderTargetResource->GetSizeX(), OutTextureRenderTargetResource->GetSizeY());

		// Update viewport.
		RHICmdList.SetViewport(0, 0, 0.f, TargetResolution.X, TargetResolution.Y, 1.0f);

		// Get shaders.
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef< FFullScreenVS > VertexShader(GlobalShaderMap);
		TShaderMapRef< FFullScreenPS > PixelShader(GlobalShaderMap);

		// Set the graphic pipeline state.
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		// Update viewport.
		RHICmdList.SetViewport(
			0, 0, 0.0f,
			OutTextureRenderTargetResource->GetSizeX(), OutTextureRenderTargetResource->GetSizeY(), 1.0f);

		// Update shader uniform parameters.
		SetShaderParametersLegacyVS(RHICmdList, VertexShader, CompiledParams, TargetResolution);
		SetShaderParametersLegacyPS(RHICmdList, PixelShader, CompiledParams, TargetResolution);

		// Draw grid.
		RHICmdList.DrawPrimitive(0, 2, 1);
	}
	RHICmdList.EndRenderPass();

	RHICmdList.Transition(FRHITransitionInfo(RenderTargetTexture, ERHIAccess::RTV, ERHIAccess::SRVMask));
}

// ================================================================================================

void FDrawToTargetParams::DrawToRenderTarget(UWorld* World, UTextureRenderTarget2D* OutputRenderTarget) const
{
	check(IsInGameThread());

	if (!OutputRenderTarget)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("OutputTargetRequired", "DrawToRenderTarget: Output render target is required."));
		return;
	}

	FCompiledParams CompiledParams;
	CompiledParams.OriginalParams = *this;

	FTextureRenderTargetResource* TextureRenderTargetResource = OutputRenderTarget->GameThread_GetRenderTargetResource();

	ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();

	if (FeatureLevel < ERHIFeatureLevel::SM5)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("SM5Unavailable", "DrawToRenderTarget: Requires RHIFeatureLevel::SM5 which is unavailable."));
		return;
	}

	ENQUEUE_RENDER_COMMAND(CaptureCommand)(
		[CompiledParams, TextureRenderTargetResource, FeatureLevel](FRHICommandListImmediate& RHICmdList)
		{
			DrawToRenderTarget_RenderThread(
				RHICmdList,
				CompiledParams,
				TextureRenderTargetResource,
				FeatureLevel);
		}
	);
}

// ================================================================================================

PRAGMA_ENABLE_DEPRECATION_WARNINGS
#undef LOCTEXT_NAMESPACE
