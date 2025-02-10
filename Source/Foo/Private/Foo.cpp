// Copyright Epic Games, Inc. All Rights Reserved.

#include "Foo.h"
#include "Interfaces/IPluginManager.h"
#include "CloudSceneViewExtension.h"

#define LOCTEXT_NAMESPACE "FFooModule"

void FFooModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("Foo"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/Foo"), PluginShaderDir);

#if 1
	FCoreDelegates::OnPostEngineInit.AddLambda([this]()
	{
		check(GEngine);
		CloudSceneViewExtension = FSceneViewExtensions::NewExtension<FCloudSceneViewExtension>();
		UE_LOG(LogTemp, Warning, TEXT("Init done ========"));
	});
#endif
}

void FFooModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	//CloudSceneViewExtension.Reset();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FFooModule, Foo)