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

TGlobalResource<FTriangleVertexBuffer> GCloudVertexBuffer;
TGlobalResource<FTriangleIndexBuffer> GCloudIndexBuffer;
TGlobalResource<FTriangleVertexDeclaration> GCloudVertexDeclaration;

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
		FMatrix WorldToProjMatrix = WorldToViewMatrix * ViewToProjMatrix;

		FVector4 v = WorldToViewMatrix.TransformFVector4(FVector4(3000.0, -1000.0, 70.0, 1.0));
		FVector4 p3 = ViewToProjMatrix.TransformFVector4(v);
		FVector p4(p3[0] / p3[3], p3[1] / p3[3], p3[2] / p3[3]);

		FVector4 p = WorldToProjMatrix.TransformFVector4(FVector4(3000.0, -1000.0, 70.0, 1.0));
		FVector p2(p[0] / p[3], p[1] / p[3], p[2] / p[3]);

		UE_LOG(LogTemp, Warning, TEXT("View to proj Matrix: %s"), *ViewToProjMatrix.ToString());
		UE_LOG(LogTemp, Warning, TEXT("World to view: %s"), *WorldToViewMatrix.ToString());

		UE_LOG(LogTemp, Warning, TEXT("P: %s"), *p.ToString());
		UE_LOG(LogTemp, Warning, TEXT("P3: %s"), *p3.ToString());

		UE_LOG(LogTemp, Warning, TEXT("P2: %s"), *p2.ToString());
		UE_LOG(LogTemp, Warning, TEXT("P4: %s"), *p4.ToString());

		UE_LOG(LogTemp, Warning, TEXT("V: %s"), *v.ToString());
		UE_LOG(LogTemp, Warning, TEXT("==="));

		RenderTriangle(GraphBuilder, ViewShaderMap, ViewInfo, SceneColor, WorldToProjMatrix);
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
	const FMatrix& WorldProjMatrix)
{
	// Shader Parameter Setup
	FCloudPSParams* PassParams = GraphBuilder.AllocParameters<FCloudPSParams>();
	PassParams->RenderTargets[0] = FRenderTargetBinding(InSceneColor.Texture, ERenderTargetLoadAction::ELoad);
	float GameTime = FApp::GetGameTime();
	PassParams->Offset = FVector4f(GameTime, GameTime * 1.34, GameTime * 0.47, 0.0);

	FCloudVSParams* VertexShaderParams = GraphBuilder.AllocParameters<FCloudVSParams>();
	VertexShaderParams->Transform = FMatrix44f(WorldProjMatrix);

	// Create Pixel Shader
	TShaderMapRef<FCloudPS> PixelShader(ShaderMap);

	check(PixelShader.IsValid());
	ClearUnusedGraphResources(PixelShader, PassParams);

	GraphBuilder.AddPass(
		Forward<FRDGEventName>(RDG_EVENT_NAME("CloudPass")),
		PassParams,
		ERDGPassFlags::Raster,
		[VertexShaderParams, PassParams, ShaderMap, PixelShader, ViewInfo](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport((float)ViewInfo.Min.X, (float)ViewInfo.Min.Y, 0.0f, (float)ViewInfo.Max.X, (float)ViewInfo.Max.Y, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;

			TShaderMapRef<FCloudVS> VertexShader(ShaderMap);

			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GCloudVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			//GraphicsPSOInit.BlendState = BlendState ? BlendState : GraphicsPSOInit.BlendState;
			//GraphicsPSOInit.RasterizerState = RasterizerState ? RasterizerState : GraphicsPSOInit.RasterizerState;
			//GraphicsPSOInit.DepthStencilState = DepthStencilState ? DepthStencilState : GraphicsPSOInit.DepthStencilState;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParams);
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), *VertexShaderParams);

			RHICmdList.SetStreamSource(0, GCloudVertexBuffer.VertexBufferRHI, 0);

			RHICmdList.DrawIndexedPrimitive(
				GCloudIndexBuffer.IndexBufferRHI,
				/*BaseVertexIndex=*/ 0,
				/*MinIndex=*/ 0,
				/*NumVertices=*/ 8,
				/*StartIndex=*/ 0,
				/*NumPrimitives=*/ 12,
				/*NumInstances=*/ 1);

		});
}
