//$ Copyright 2015-23, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "PrefabricatorEditorModule.h"

#include "Asset/PrefabricatorAsset.h"
#include "Asset/PrefabricatorAssetBroker.h"
#include "Asset/PrefabricatorAssetTypeActions.h"
#include "Prefab/PrefabTools.h"
#include "PrefabEditorCommands.h"
#include "PrefabEditorStyle.h"
#include "UI/EditorUIExtender.h"
#include "UI/PrefabCustomization.h"
#include "Utils/MapChangeHook.h"
#include "Utils/PrefabricatorEditorService.h"
#include "Utils/SelectionHook.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "PrefabricatorEditorModule" 

FPrefabDetailsExtend PrefabActorDetailsExtender;

void FPrefabricatorEditorModule::SetActivePrefabActor(APrefabActor* InPrefabActor)
{
	FEditorDelegates::OnNewActorsPlaced.AddRaw(this, &FPrefabricatorEditorModule::OnLevelActorsPlaced);
		
	ActivePrefabActor = InPrefabActor;
}

APrefabActor* FPrefabricatorEditorModule::GetActivePrefabActor() const
{
	return ActivePrefabActor.IsValid() ? ActivePrefabActor.Get() : nullptr;
}

void FPrefabricatorEditorModule::ClearActivePrefab()
{
	FEditorDelegates::OnNewActorsPlaced.RemoveAll(this);

	ActivePrefabActor = nullptr;
}

void FPrefabricatorEditorModule::OnLevelActorsPlaced(UObject* Object, const TArray<AActor*>& Actors) const
{
	// if we have an active prefab actor, set it as the parent of the new actor
	if (ActivePrefabActor.IsValid())
	{
		for (AActor* Actor : Actors)
		{
			if (Actor && !Actor->IsA(APrefabActor::StaticClass()))
			{
				FPrefabTools::ParentActors(ActivePrefabActor.Get(), Actor);
			}
		}
	}
}

void FPrefabricatorEditorModule::StartupModule() 
{
	FPrefabEditorStyle::Initialize();
	FPrefabricatorCommands::Register();

	// Extend the editor menu and toolbar
	UIExtender.Extend();
	SelectionHook.Initialize();
	MapChangeHook.Initialize();

	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FPrefabricatorAssetTypeActions));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FPrefabricatorAssetCollectionTypeActions));

	// Add a category for the prefabricator assets in the context menu
	PrefabricatorAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Prefabricator")), LOCTEXT("PrefabricatorAssetCategory", "Prefabricator"));

	// Register the details customization
	RegisterCustomClassLayouts();

	// Register the asset brokers (used for asset to component mapping)
	PrefabAssetBroker = MakeShareable(new FPrefabricatorAssetBroker);
	FComponentAssetBrokerage::RegisterBroker(PrefabAssetBroker, UPrefabComponent::StaticClass(), true, true);

	// Override the prefabricator service with the editor version, so the runtime module can access it
	FPrefabricatorService::Set(MakeShareable(new FPrefabricatorEditorService));
}

void FPrefabricatorEditorModule::RegisterCustomClassLayouts() {
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	RegisterCustomClassLayout<FPrefabActorCustomization>("PrefabActor", PropertyEditorModule);
	RegisterCustomClassLayout<FPrefabRandomizerCustomization>("PrefabRandomizer", PropertyEditorModule);
	RegisterCustomClassLayout<FPrefabricatorAssetCustomization>("PrefabricatorAsset", PropertyEditorModule);
	RegisterCustomClassLayout<FPrefabDebugCustomization>("PrefabDebugActor", PropertyEditorModule);
}

void FPrefabricatorEditorModule::UnregisterCustomClassLayouts() {
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	for (const FName& ClassName : RegisteredCustomClassLayouts) {
		PropertyEditorModule.UnregisterCustomClassLayout(ClassName);
	}
	RegisteredCustomClassLayouts.Reset();
}

void FPrefabricatorEditorModule::ShutdownModule()
{
	// Unregister the prefabricator asset broker
	if (PrefabAssetBroker.IsValid()) {
		FComponentAssetBrokerage::UnregisterBroker(PrefabAssetBroker);
		PrefabAssetBroker = nullptr;
	}

	UnregisterCustomClassLayouts();

	// Unregister all the asset types that we registered
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (int32 Index = 0; Index < CreatedAssetTypeActions.Num(); ++Index)
		{
			AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeActions[Index].ToSharedRef());
		}
	}
	CreatedAssetTypeActions.Empty();

	MapChangeHook.Release();
	SelectionHook.Release();
	UIExtender.Release();

	FPrefabricatorCommands::Unregister();
	FPrefabEditorStyle::Shutdown();
}

EAssetTypeCategories::Type FPrefabricatorEditorModule::GetPrefabricatorAssetCategoryBit() const
{
	return PrefabricatorAssetCategoryBit;
}

FPrefabDetailsExtend& FPrefabricatorEditorModule::GetPrefabActorDetailsExtender()
{
	return PrefabActorDetailsExtender;
}

void FPrefabricatorEditorModule::UpgradePrefabAssets() {
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(UPrefabricatorAsset::StaticClass()->GetClassPathName(), AssetDataList);
	for (const FAssetData& AssetData : AssetDataList) {
		UPrefabricatorAsset* PrefabAsset = Cast<UPrefabricatorAsset>(AssetData.GetAsset());
		if (PrefabAsset) {
			if (PrefabAsset->Version != (uint32)EPrefabricatorAssetVersion::LatestVersion) {
				FPrefabVersionControl::UpgradeToLatestVersion(PrefabAsset);
			}
		}
	}
}

void FPrefabricatorEditorModule::CreatePathPickerSubMenu(FMenuBuilder& MenuBuilder)
{
	TSharedRef<SWidget> AssetTextBox =
		SNew(SEditableTextBox)
					.OnTextChanged(FOnTextChanged::CreateLambda([this](const FText InText)
					{
						AssetName = InText;
					}));

	MenuBuilder.AddWidget(AssetTextBox, FText::FromString("Asset Name"), true);
	
	TSharedRef<SWidget> ClassPicker =
		SNew(SClassPropertyEntryBox)
				.MetaClass(APrefabActor::StaticClass())
				.AllowNone(false)
				.AllowAbstract(false)
				.OnSetClass(FOnSetClass::CreateRaw(this, &FPrefabricatorEditorModule::OnSetPrefabClass))
				.SelectedClass_Lambda([this]() { return CurrentPrefabClass; });

	MenuBuilder.AddWidget(ClassPicker, FText::FromString("Prefab Class"), true);
		
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("PrefabDestinationFolder", "Destination Folder"));
	
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = SelectedFolder;
	PathPickerConfig.bAllowContextMenu = false;
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateLambda([this](const FString& SelectedPath) {
		SelectedFolder = SelectedPath;
	});

	// Create the path picker widget
	TSharedRef<SWidget> PathPicker =
		SNew(SBox)
			.HeightOverride(400.0f)
			.WidthOverride(350.0f)
			[
				ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
			];

	MenuBuilder.AddWidget(PathPicker, FText(), true);

	MenuBuilder.EndSection();

	TSharedRef<SWidget> CreatePrefabButton =
		SNew(SBox)
			.Padding(FMargin(0, 10, 10, 0))
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			[
				SNew(SButton)
				.Text(LOCTEXT("CreatePrefab", "Create Prefab"))
				.OnClicked_Lambda([this]()
				{
					if (IsValid(CurrentPrefabClass)) {
						TArray<AActor*> SelectedActors;
						FPrefabTools::GetSelectedActors(SelectedActors);

						TSubclassOf<UPrefabricatorAsset> PrefabAssetClass = CurrentPrefabClass.GetDefaultObject()->GetPrefabAssetClass(); 
							
						FPrefabTools::CreatePrefabFromActors(PrefabAssetClass, CurrentPrefabClass, SelectedFolder, AssetName.ToString(), SelectedActors);
					}
					return FReply::Handled();
				})
			];

	MenuBuilder.AddWidget(CreatePrefabButton, FText(), true);
}

void FPrefabricatorEditorModule::OnSetPrefabClass(const UClass* Class)
{
	CurrentPrefabClass = TSubclassOf<APrefabActor>(const_cast<UClass*>(Class));
}

void FPrefabricatorEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

IMPLEMENT_MODULE(FPrefabricatorEditorModule, PrefabricatorEditor)

#undef LOCTEXT_NAMESPACE