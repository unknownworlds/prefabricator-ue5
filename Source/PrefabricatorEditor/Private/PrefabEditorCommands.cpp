//$ Copyright 2015-23, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "PrefabEditorCommands.h"

#include "Prefab/PrefabTools.h"
#include "PrefabEditorStyle.h"
#include "PrefabricatorEditorModule.h"

#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Prefab/PrefabActor.h"
#include "UObject/Object.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

FPrefabricatorCommands::FPrefabricatorCommands() : TCommands<FPrefabricatorCommands>(
	TEXT("Prefabricator"),
	NSLOCTEXT("Prefabricator", "Prefabricator", "Prefabricator"),
	NAME_None,
	FPrefabEditorStyle::GetStyleSetName())
{
}

void FPrefabricatorCommands::RegisterCommands()
{
	UI_COMMAND(CreatePrefab, "Create Prefab (from selection)", "Create a new prefab from selection", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter));
	UI_COMMAND(AddSelectedToActivePrefab, "Add selection to recent prefab", "Adds selection to most recently selected prefab", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter, EModifierKey::Control));
	UI_COMMAND(SetActivePrefab, "Set Active Prefab", "Set the active prefab. All newly placed actors will be added to this prefab.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ClearActivePrefab, "Clear Active Prefab", "Clear the active prefab. New actors will no longer be added to any prefab.", EUserInterfaceActionType::Button, FInputChord());

	LevelMenuActionList = MakeShareable(new FUICommandList);

	LevelMenuActionList->MapAction(
		CreatePrefab,
		FExecuteAction::CreateStatic(&FPrefabTools::CreatePrefabAtDefaultPath),
		FCanExecuteAction::CreateStatic(&FPrefabTools::CanCreatePrefab));

	LevelMenuActionList->MapAction(
		AddSelectedToActivePrefab,
		FExecuteAction::CreateLambda([]()
			{
				FPrefabricatorEditorModule& Module = FPrefabricatorEditorModule::Get();
				if (APrefabActor* PrefabActor = Module.GetActivePrefabActor())
				{
					FPrefabTools::AddSelectedActorsToPrefab(PrefabActor);
				}
			}),
		FCanExecuteAction::CreateStatic(&FPrefabTools::CanCreatePrefab));
	
	LevelMenuActionList->MapAction(
		SetActivePrefab,
		FExecuteAction::CreateLambda([]()
			{
				FPrefabricatorEditorModule& Module = FPrefabricatorEditorModule::Get();
				Module.SetActivePrefabActor(nullptr);
			}),
		FCanExecuteAction::CreateStatic(&FPrefabTools::HasPrefabSelected));

	LevelMenuActionList->MapAction(
		ClearActivePrefab,
		FExecuteAction::CreateLambda([]()
			{
				FPrefabricatorEditorModule& Module = FPrefabricatorEditorModule::Get();
				Module.ClearActivePrefab();
			}),
		FCanExecuteAction::CreateLambda([]()
			{
				FPrefabricatorEditorModule& Module = FPrefabricatorEditorModule::Get();
				return Module.GetActivePrefabActor() != nullptr;
			}));
}

#undef LOCTEXT_NAMESPACE

