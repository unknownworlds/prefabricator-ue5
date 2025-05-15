//$ Copyright 2015-23, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "PrefabEditorTypes.h"

#include "AssetTypeCategories.h"
#include "ComponentAssetBroker.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "UI/EditorUIExtender.h"
#include "Utils/MapChangeHook.h"
#include "Utils/SelectionHook.h"

/**
 * The public interface to this module
 */
class PREFABRICATOREDITOR_API FPrefabricatorEditorModule : public IModuleInterface
{

public:
	FPrefabricatorEditorModule() = default;
	
	FPrefabDetailsExtend PrefabActorDetailsExtender;

	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FPrefabricatorEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FPrefabricatorEditorModule>("PrefabricatorEditor");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("PrefabricatorEditor");
	}

	void SetActivePrefabActor(APrefabActor* InPrefabActor);
	
	APrefabActor* GetActivePrefabActor() const;

	void ClearActivePrefab();

	void OnLevelActorsPlaced(UObject* Object, const TArray<AActor*>& Actors) const;

	template<typename TCustomization>
	void RegisterCustomClassLayout(const FName& ClassName, FPropertyEditorModule& PropertyEditorModule) {
		PropertyEditorModule.RegisterCustomClassLayout(ClassName, FOnGetDetailCustomizationInstance::CreateStatic(&TCustomization::MakeInstance));
		RegisteredCustomClassLayouts.Add(ClassName);
	}

	void RegisterCustomClassLayouts();

	void UnregisterCustomClassLayouts();
	
	EAssetTypeCategories::Type GetPrefabricatorAssetCategoryBit() const;

	FPrefabDetailsExtend& GetPrefabActorDetailsExtender();

	static void UpgradePrefabAssets();

	void OnSetPrefabClass(const UClass* Class);
	
	void CreatePathPickerSubMenu(FMenuBuilder& MenuBuilder);

private:
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

private:
	FEditorUIExtender UIExtender;
	FPrefabricatorSelectionHook SelectionHook;
	FMapChangeHook MapChangeHook;
	TSharedPtr<IComponentAssetBroker> PrefabAssetBroker;
	TArray< TSharedPtr<IAssetTypeActions> > CreatedAssetTypeActions;
	TSet<FName> RegisteredCustomClassLayouts;
	EAssetTypeCategories::Type PrefabricatorAssetCategoryBit;

	FText AssetName;
	TSubclassOf<APrefabActor> CurrentPrefabClass = nullptr;
	FString SelectedFolder;
	
	TWeakObjectPtr<APrefabActor> ActivePrefabActor = nullptr;
};

