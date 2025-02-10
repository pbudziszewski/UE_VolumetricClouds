#include "CloudSceneViewExtension.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "PixelShaderUtils.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"
#include "SceneTextureParameters.h"
#include "ShaderParameterStruct.h"

IMPLEMENT_SHADER_TYPE(, FCloudVS, TEXT("/Plugin/Foo/Private/CloudShader.usf"), TEXT("MainVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FCloudPS, TEXT("/Plugin/Foo/Private/CloudShader.usf"), TEXT("MainPS"), SF_Pixel)

TGlobalResource<FTriangleVertexBuffer> GTriangleVertexBuffer;
TGlobalResource<FTriangleIndexBuffer> GTriangleIndexBuffer;
TGlobalResource<FTriangleVertexDeclaration> GTriangleVertexDeclaration;

// ================================================================================================

FCloudSceneViewExtension::FCloudSceneViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

// ================================================================================================

FCloudSceneViewExtension::~FCloudSceneViewExtension()
{
}

// ================================================================================================

void FCloudSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
}

// ================================================================================================

void FCloudSceneViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
}

// ================================================================================================

void FCloudSceneViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (Pass == EPostProcessingPass::Tonemap)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FCloudSceneViewExtension::TrianglePass_RenderThread));
	}
}

// ================================================================================================

FScreenPassTexture FCloudSceneViewExtension::TrianglePass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs)
{
	FScreenPassTexture SceneColor(InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor));

	FScreenPassRenderTarget Output = InOutInputs.OverrideOutput;

	// If the override output is provided, it means that this is the last pass in post processing.
	// You need to do this since we cannot read from SceneColor and write to it (At least it's not supported by UE)
	// OverrideOutput is also required to use so the Pass Sequence in PostProcessing.cpp knows this is the last pass so we don't get a bad texture from Scene Color on the next frame.

	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, SceneColor, View.GetOverwriteLoadAction(), TEXT("OverrideSceneColorTexture"));
	}

	if (EnumHasAllFlags(SceneColor.Texture->Desc.Flags, TexCreate_ShaderResource) && EnumHasAnyFlags(SceneColor.Texture->Desc.Flags, TexCreate_RenderTargetable | TexCreate_ResolveTargetable))
	{
		const FIntRect ViewInfo = static_cast<const FViewInfo&>(View).ViewRect;
		const FGlobalShaderMap* ViewShaderMap = static_cast<const FViewInfo&>(View).ShaderMap;

		FMatrix WorldToViewMatrix = View.ViewMatrices.GetViewMatrix();
		FMatrix ViewToProjMatrix = View.ViewMatrices.GetProjectionMatrix();
		FMatrix ViewProjMatrix = WorldToViewMatrix * ViewToProjMatrix;

		UE_LOG(LogTemp, Warning, TEXT("View Matrix: %s"), *WorldToViewMatrix.ToString());
		UE_LOG(LogTemp, Warning, TEXT("==="));

		RenderTriangle(GraphBuilder, ViewShaderMap, ViewInfo, SceneColor, ViewProjMatrix);
	}

	return MoveTemp(SceneColor);
}

// ================================================================================================

void FCloudSceneViewExtension::RenderTriangle
(
	FRDGBuilder& GraphBuilder,
	const FGlobalShaderMap* ShaderMap,
	const FIntRect& ViewInfo,
	const FScreenPassTexture& InSceneColor,
	const FMatrix& ViewProjMatrix)
{
	// Shader Parameter Setup
	FCloudPSParams* PassParams = GraphBuilder.AllocParameters<FCloudPSParams>();
	PassParams->RenderTargets[0] = FRenderTargetBinding(InSceneColor.Texture, ERenderTargetLoadAction::ELoad);
	float GameTime = FApp::GetGameTime();
	PassParams->Offset = FVector4f(GameTime, GameTime * 0.34, GameTime * 0.47, 0.0);

	// Create Pixel Shader
	TShaderMapRef<FCloudPS> PixelShader(ShaderMap);

	check(PixelShader.IsValid());
	ClearUnusedGraphResources(PixelShader, PassParams);

	GraphBuilder.AddPass(
		Forward<FRDGEventName>(RDG_EVENT_NAME("CloudPass")),
		PassParams,
		ERDGPassFlags::Raster,
		[PassParams, ShaderMap, PixelShader, ViewInfo](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport((float)ViewInfo.Min.X, (float)ViewInfo.Min.Y, 0.0f, (float)ViewInfo.Max.X, (float)ViewInfo.Max.Y, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;

			TShaderMapRef<FCloudVS> VertexShader(ShaderMap);

			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GTriangleVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			//GraphicsPSOInit.BlendState = BlendState ? BlendState : GraphicsPSOInit.BlendState;
			//GraphicsPSOInit.RasterizerState = RasterizerState ? RasterizerState : GraphicsPSOInit.RasterizerState;
			//GraphicsPSOInit.DepthStencilState = DepthStencilState ? DepthStencilState : GraphicsPSOInit.DepthStencilState;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParams);

			RHICmdList.SetStreamSource(0, GTriangleVertexBuffer.VertexBufferRHI, 0);
			RHICmdList.DrawIndexedPrimitive(
				GTriangleIndexBuffer.IndexBufferRHI,
				/*BaseVertexIndex=*/ 0,
				/*MinIndex=*/ 0,
				/*NumVertices=*/ 3,
				/*StartIndex=*/ 0,
				/*NumPrimitives=*/ 1,
				/*NumInstances=*/ 1);
		});
}
