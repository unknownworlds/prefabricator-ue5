//$ Copyright 2015-23, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Prefab/PrefabTools.h"

#if WITH_EDITOR
#include "ISourceControlModule.h"
#include "ISourceControlState.h"
#include "ISourceControlProvider.h"
#endif
#include "Asset/PrefabricatorAsset.h"
#include "Asset/PrefabricatorAssetUserData.h"
#include "Prefab/PrefabActor.h"
#include "Prefab/PrefabComponent.h"
#include "PrefabricatorSettings.h"
#include "Utils/PrefabricatorService.h"
#include "Utils/PrefabricatorStats.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "HAL/UnrealMemory.h"
#include "PropertyPathHelpers.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogPrefabTools, Log, All);

#define LOCTEXT_NAMESPACE "PrefabTools"

void FPrefabTools::GetSelectedActors(TArray<AActor*>& OutActors)
{
	TSharedPtr<IPrefabricatorService> Service = FPrefabricatorService::Get();
	if (Service.IsValid()) {
		Service->GetSelectedActors(OutActors);
	}
}


int FPrefabTools::GetNumSelectedActors()
{
	TSharedPtr<IPrefabricatorService> Service = FPrefabricatorService::Get();
	return Service.IsValid() ? Service->GetNumSelectedActors() : 0;
}

void FPrefabTools::ParentActors(AActor* ParentActor, AActor* ChildActor)
{
	SCOPE_CYCLE_COUNTER(STAT_ParentActors);
	if (ChildActor && ParentActor) {
		{
			SCOPE_CYCLE_COUNTER(STAT_ParentActors1);
			ChildActor->DetachFromActor(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_ParentActors2);
			TSharedPtr<IPrefabricatorService> Service = FPrefabricatorService::Get();
			if (Service.IsValid()) {
				Service->ParentActors(ParentActor, ChildActor);
			}
		}
	}
}

void FPrefabTools::SelectPrefabActor(AActor* PrefabActor)
{
	TSharedPtr<IPrefabricatorService> Service = FPrefabricatorService::Get();
	if (Service.IsValid()) {
		Service->SelectPrefabActor(PrefabActor);
	}
}

UPrefabricatorAsset* FPrefabTools::CreatePrefabAsset()
{
	TSharedPtr<IPrefabricatorService> Service = FPrefabricatorService::Get();
	return Service.IsValid() ? Service->CreatePrefabAsset() : nullptr;
}

int32 FPrefabTools::GetRandomSeed(const FRandomStream& InRandom)
{
	return InRandom.RandRange(0, 10000000);
}

void FPrefabTools::IterateChildrenRecursive(APrefabActor* Prefab, TFunction<void(AActor*)> Visit)
{
	TArray<AActor*> Stack;
	{
		TArray<AActor*> AttachedActors;
		Prefab->GetAttachedActors(AttachedActors);
		for (AActor* Child : AttachedActors) {
			Stack.Push(Child);
		}
	}

	while (Stack.Num() > 0) {
		AActor* Top = Stack.Pop();

		Visit(Top);

		{
			TArray<AActor*> AttachedActors;
			Top->GetAttachedActors(AttachedActors);
			for (AActor* Child : AttachedActors) {
				Stack.Push(Child);
			}
		}
	}
}

bool FPrefabTools::CanCreatePrefab()
{
	return GetNumSelectedActors() > 0;
}

void FPrefabTools::CreatePrefab()
{
	TArray<AActor*> SelectedActors;
	GetSelectedActors(SelectedActors);

	CreatePrefabFromActors(SelectedActors);
}

namespace {

	void SanitizePrefabActorsForCreation(const TArray<AActor*>& InActors, TArray<AActor*>& OutActors) {
		// Find all the selected prefab actors
		TArray<APrefabActor*> PrefabActors;
		for (AActor* Actor : InActors) {
			if (APrefabActor* PrefabActor = Cast<APrefabActor>(Actor)) {
				PrefabActors.Add(PrefabActor);
			}
		}

		for (AActor* Actor : InActors) {
			bool bValid = true;
			// Make sure we do not include any actors that belong to these prefabs
			if (APrefabActor* ParentPrefab = Cast<APrefabActor>(Actor->GetAttachParentActor())) {
				if (PrefabActors.Contains(ParentPrefab)) {
					bValid = false;
				}
			}

			// Make sure the actor has a root component
			if (!Actor->GetRootComponent()) {
				bValid = false;
			}

			if (bValid) {
				OutActors.Add(Actor);
			}
		}
	}
}

APrefabActor* FPrefabTools::CreatePrefabFromActors(const TArray<AActor*>& InActors)
{
	TArray<AActor*> Actors;
	SanitizePrefabActorsForCreation(InActors, Actors);

	if (Actors.Num() == 0) {
		return nullptr;
	}

	UPrefabricatorAsset* PrefabAsset = CreatePrefabAsset();
	if (!PrefabAsset) {
		return nullptr;
	}

	TSharedPtr<IPrefabricatorService> Service = FPrefabricatorService::Get();
	if (Service.IsValid()) {
		Service->BeginTransaction(LOCTEXT("TransLabel_CreatePrefab", "Create Prefab"));
	}

	UWorld* World = Actors[0]->GetWorld();

	AActor* PivotActor = nullptr;
	for (AActor* Actor : Actors)
	{
		if(Actor && Actor->GetActorNameOrLabel().Contains("PivotActor"))
		{
			PivotActor = Actor;
		}
	}
	
	FVector Pivot;
	if(PivotActor)
	{
		Pivot = PivotActor->GetActorLocation();
		Actors.Remove(PivotActor);
		PivotActor->Destroy();
	}
	else
	{
		Pivot = FPrefabricatorAssetUtils::FindPivot(Actors);
	}
	APrefabActor* PrefabActor = World->SpawnActor<APrefabActor>(Pivot, FRotator::ZeroRotator);

	// Find the compatible mobility for the prefab actor
	EComponentMobility::Type Mobility = FPrefabricatorAssetUtils::FindMobility(Actors);
	PrefabActor->GetRootComponent()->SetMobility(Mobility);

	PrefabActor->PrefabComponent->PrefabAssetInterface = PrefabAsset;
	// Attach the actors to the prefab
	for (AActor* Actor : Actors) {
		ParentActors(PrefabActor, Actor);
	}

	if (Service.IsValid()) {
		Service->EndTransaction();
	}

	SaveStateToPrefabAsset(PrefabActor);

	SelectPrefabActor(PrefabActor);

	return PrefabActor;
}

void FPrefabTools::AssignAssetUserData(AActor* InActor, const FGuid& InItemID, APrefabActor* Prefab)
{
	if (!InActor || !InActor->GetRootComponent()) {
		return;
	}

	UActorComponent* RootComponent = InActor->GetRootComponent();
	UPrefabricatorAssetUserData* PrefabUserData = RootComponent->GetAssetUserData<UPrefabricatorAssetUserData>();
	if (!PrefabUserData) {
		PrefabUserData = NewObject<UPrefabricatorAssetUserData>(InActor->GetRootComponent());
		RootComponent->AddAssetUserData(PrefabUserData);
	}

	PrefabUserData->PrefabActor = Prefab;
	PrefabUserData->ItemID = InItemID;
}


void FPrefabTools::SaveStateToPrefabAsset(APrefabActor* PrefabActor)
{
	if (!PrefabActor) {
		UE_LOG(LogPrefabTools, Error, TEXT("Invalid prefab actor reference"));
		return;
	}

	UPrefabricatorAsset* PrefabAsset = PrefabAsset = Cast<UPrefabricatorAsset>(PrefabActor->PrefabComponent->PrefabAssetInterface.LoadSynchronous());
	if (!PrefabAsset) {
		//UE_LOG(LogPrefabTools, Error, TEXT("Prefab asset is not assigned correctly"));
		return;
	}

	PrefabAsset->PrefabMobility = PrefabActor->GetRootComponent()->Mobility;

	TArray<FPrefabricatorActorData> OldDataCopy = PrefabAsset->ActorData;
	PrefabAsset->ActorData.Reset();
	
	TArray<AActor*> Children;
	GetActorChildren(PrefabActor, Children);

	// Make sure the children do not have duplicate asset user data template ids
	{
		TSet<FGuid> VisitedItemId;
		for (AActor* ChildActor : Children) {
			if (ChildActor && ChildActor->GetRootComponent()) {
				UPrefabricatorAssetUserData* ChildUserData = ChildActor->GetRootComponent()->GetAssetUserData<UPrefabricatorAssetUserData>();
				if (ChildUserData) {
					if (VisitedItemId.Contains(ChildUserData->ItemID)) {
						ChildUserData->ItemID = FGuid::NewGuid();
						ChildUserData->Modify();
					}
					VisitedItemId.Add(ChildUserData->ItemID);
				}
			}
		}
	}

	struct FSaveContext {
		AActor* ChildActor;
		int32 ItemIndex;
		FPrefabricatorActorData OldData;
	};

	FPrefabActorLookup ActorCrossReferences;
	TArray<FSaveContext> ItemsToSave;
	for (AActor* ChildActor : Children)
	{
		if (ChildActor && ChildActor->GetRootComponent())
		{
			UPrefabricatorAssetUserData* ChildUserData = ChildActor->GetRootComponent()->GetAssetUserData<UPrefabricatorAssetUserData>();
			FGuid ItemID;
			if (ChildUserData && ChildUserData->PrefabActor == PrefabActor)
			{
				ItemID = ChildUserData->ItemID;
			}
			else
			{
				ItemID = FGuid::NewGuid();
			}
			AssignAssetUserData(ChildActor, ItemID, PrefabActor);
			
			int32 NewItemIndex = PrefabAsset->ActorData.AddDefaulted();
			FPrefabricatorActorData& NewActorData = PrefabAsset->ActorData[NewItemIndex];
			NewActorData.PrefabItemID = ItemID;//This might be the old ID
			
			FSaveContext SaveInfo;
			SaveInfo.ChildActor = ChildActor;
			SaveInfo.ItemIndex = NewItemIndex;
			
			//Check if we already have data for this child and copy it if we did
			for(int32 i=0; i<OldDataCopy.Num(); i++)
			{
				FPrefabricatorActorData& OldData = OldDataCopy[i];
				if(OldData.PrefabItemID == ItemID)
				{
					FPrefabricatorActorData::StaticStruct()->CopyScriptStruct(&NewActorData, &OldData);
					FPrefabricatorActorData::StaticStruct()->CopyScriptStruct(&SaveInfo.OldData, &OldData);
					break;
				}
			}

			ActorCrossReferences.Register(ChildActor, ItemID);

			
			ItemsToSave.Add(SaveInfo);
		}
	}

	for (const FSaveContext& SaveInfo : ItemsToSave)
	{
		AActor* ChildActor = SaveInfo.ChildActor;
		if (ChildActor && ChildActor->GetRootComponent())
		{
			FPrefabricatorActorData OldActorData = SaveInfo.OldData;
			FPrefabricatorActorData& ActorData = PrefabAsset->ActorData[SaveInfo.ItemIndex];
			SaveActorState(ChildActor, PrefabActor, ActorCrossReferences, ActorData);

			UPrefabricatorAssetUserData* ChildUserData = ChildActor->GetRootComponent()->GetAssetUserData<UPrefabricatorAssetUserData>();

			if(!OldActorData.Equals(ActorData))
			{
				ActorData.ActorLastUpdateID = FGuid::NewGuid();
			}
			
			ChildUserData->ActorLastUpdateID = ActorData.ActorLastUpdateID;
		}
	}
	PrefabAsset->Version = (uint32)EPrefabricatorAssetVersion::LatestVersion;

	PrefabActor->PrefabComponent->UpdateBounds();

	// Regenerate a new update id for the prefab asset
	PrefabAsset->LastUpdateID = FGuid::NewGuid();
	PrefabActor->LastUpdateID = PrefabAsset->LastUpdateID;
	PrefabAsset->Modify();

	TSharedPtr<IPrefabricatorService> Service = FPrefabricatorService::Get();
	if (Service.IsValid()) {
		Service->CaptureThumb(PrefabAsset);
	}
}

namespace {
	bool GetPropertyData(const FProperty* Property, UObject* Obj, UObject * ObjTemplate, FString& OutPropertyData) {
		if (!Obj || !Property) return false;
		
		UObject* DefaultObject = ObjTemplate;
		if (!DefaultObject) {
			UClass* ObjClass = Obj->GetClass();
			if (!ObjClass) return false;
			DefaultObject = ObjClass->GetDefaultObject();
		}

		Property->ExportTextItem_Direct(OutPropertyData, Property->ContainerPtrToValuePtr<void>(Obj), Property->ContainerPtrToValuePtr<void>(DefaultObject), Obj, PPF_Copy);
		return true;
	}

	bool ContainsOuterParent(UObject* ObjectToTest, UObject* Outer) {
		while (ObjectToTest) {
			if (ObjectToTest == Outer) return true;
			ObjectToTest = ObjectToTest->GetOuter();
		}
		return false;
	}

	bool HasDefaultValue(UObject* InContainer, UObject* InDiff, const FString& InPropertyPath) {
		if (!InContainer) return false;

		UObject* DefaultObject = InDiff;
		if (!DefaultObject) {
			UClass* ObjClass = InContainer->GetClass();
			if (!ObjClass) return false;
			DefaultObject = ObjClass->GetDefaultObject();
		}

		FString PropertyValue, DefaultValue;
		PropertyPathHelpers::GetPropertyValueAsString(InContainer, InPropertyPath, PropertyValue);
		PropertyPathHelpers::GetPropertyValueAsString(DefaultObject, InPropertyPath, DefaultValue);
		if (PropertyValue != DefaultValue) {
			UE_LOG(LogPrefabTools, Log, TEXT("Property differs: %s\n> %s\n> %s"), *InPropertyPath, *PropertyValue, *DefaultValue);
		}
		return PropertyValue == DefaultValue;
	}

	bool ShouldSkipSerialization(const FProperty* Property, UObject* ObjToSerialize, APrefabActor* PrefabActor) {
		if (const FObjectProperty* ObjProperty = CastField<FObjectProperty>(Property)) {
			UObject* PropertyObjectValue = ObjProperty->GetObjectPropertyValue_InContainer(ObjToSerialize);
			if (ContainsOuterParent(PropertyObjectValue, ObjToSerialize) ||
				ContainsOuterParent(PropertyObjectValue, PrefabActor)) {
				//UE_LOG(LogPrefabTools, Log, TEXT("Skipping Property: %s"), *Property->GetName());
				return true;
			}
		}

		return false;
	}

	void DeserializeFields(UObject* InObjToDeserialize, const TArray<UPrefabricatorProperty*>& InProperties) {
		if (!InObjToDeserialize) return;

		for (UPrefabricatorProperty* PrefabProperty : InProperties) {
			if (!PrefabProperty || PrefabProperty->bIsCrossReferencedActor) continue;
			FString PropertyName = PrefabProperty->PropertyName;
			if (PropertyName == "AssetUserData") continue;// Skip this as assignment is very slow and is not needed
			if (PropertyName == "ActorGuid" || PropertyName == "FolderGuid") continue;

			//Temporary fix to prevent niagara crashes.
			//TODO - Need to find a more long term solution as this loses instance overrides
			if (PropertyName == "InstanceParameterOverrides") continue;

			FProperty* Property = InObjToDeserialize->GetClass()->FindPropertyByName(*PropertyName);
			if (Property) {
				// do not overwrite properties that have a default sub object or an archetype object
				if (FObjectProperty* ObjProperty = CastField<FObjectProperty>(Property)) {
					UObject* PropertyObjectValue = ObjProperty->GetObjectPropertyValue_InContainer(InObjToDeserialize);
					if (PropertyObjectValue && PropertyObjectValue->HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject)) {
						continue;
					}
				}

				//Some editor systems such as niagara setup delegates in editor, these can cause ambiguous
				//name errors when we try and deserialize them. I don't see a valid situation where a
				//delegate is saved within a prefab and expected to remain valid. 
				FDelegateProperty* DelegateProperty = CastField<FDelegateProperty>(Property);
				FMulticastDelegateProperty* MultiDelegateProperty = CastField<FMulticastDelegateProperty>(Property);
				if(DelegateProperty || MultiDelegateProperty)
				{
					continue;
				}

				{
					SCOPE_CYCLE_COUNTER(STAT_DeserializeFields_Iterate_LoadValue);
					PrefabProperty->LoadReferencedAssetValues();
				}

				{
					SCOPE_CYCLE_COUNTER(STAT_DeserializeFields_Iterate_SetValue);
					PropertyPathHelpers::SetPropertyValueFromString(InObjToDeserialize, PrefabProperty->PropertyName, PrefabProperty->ExportedValue);
				}
			}
		}
	}

	void SerializeFields(UObject* ObjToSerialize, UObject* ObjTemplate, APrefabActor* PrefabActor, const FPrefabActorLookup& CrossReferences, TArray<UPrefabricatorProperty*>& OutProperties) {
		if (!ObjToSerialize || !PrefabActor) {
			return;
		}

		UPrefabricatorAsset* PrefabAsset = Cast<UPrefabricatorAsset>(PrefabActor->PrefabComponent->PrefabAssetInterface.LoadSynchronous());

		if (!PrefabAsset) {
			return;
		}

		TSet<const FProperty*> PropertiesToSerialize;
		for (TFieldIterator<FProperty> PropertyIterator(ObjToSerialize->GetClass()); PropertyIterator; ++PropertyIterator) {
			FProperty* Property = *PropertyIterator;
			if (!Property) continue;
			if (Property->HasAnyPropertyFlags(CPF_Transient)) {
				continue;
			}
			FString PropertyName = Property->GetName();
			if (PropertyName == "ActorGuid" || PropertyName == "FolderGuid") continue;

			if (FPrefabTools::ShouldIgnorePropertySerialization(Property->GetFName())) {
				continue;
			}

			bool bForceSerialize = FPrefabTools::ShouldForcePropertySerialization(Property->GetFName());

			// Check if it has the default value
			if (!bForceSerialize && HasDefaultValue(ObjToSerialize, ObjTemplate, Property->GetName())) {
				continue;
			}

			if (const FObjectProperty* ObjProperty = CastField<FObjectProperty>(Property)) {
				UObject* PropertyObjectValue = ObjProperty->GetObjectPropertyValue_InContainer(ObjToSerialize);
				if (PropertyObjectValue && PropertyObjectValue->HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject)) {
					continue;
				}
			}


			PropertiesToSerialize.Add(Property);
		}

		for (const FProperty* Property : PropertiesToSerialize) {
			if (!Property) continue;
			if (FPrefabTools::ShouldIgnorePropertySerialization(Property->GetFName())) {
				continue;
			}

			UPrefabricatorProperty* PrefabProperty = nullptr;
			FString PropertyName = Property->GetName();

			
			if (ShouldSkipSerialization(Property, ObjToSerialize, PrefabActor)) {
				continue;
			}

			PrefabProperty = NewObject<UPrefabricatorProperty>(PrefabAsset);
			PrefabProperty->PropertyName = PropertyName;

			// Check for cross actor references
			bool bFoundCrossReference = false;

			if (const FObjectProperty* ObjProperty = CastField<FObjectProperty>(Property)) {
				UObject* PropertyObjectValue = ObjProperty->GetObjectPropertyValue_InContainer(ObjToSerialize);
				if (PropertyObjectValue) {
					if (PropertyObjectValue->HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject)) {
						continue;
					}

					FString ObjectPath = PropertyObjectValue->GetPathName();
					FGuid CrossRefPrefabItem;
					if (CrossReferences.GetPrefabItemId(ObjectPath, CrossRefPrefabItem)) {
						PrefabProperty->bIsCrossReferencedActor = true;
						PrefabProperty->CrossReferencePrefabActorId = CrossRefPrefabItem;
						bFoundCrossReference = true;
					}
				}
			}

			// Save as usual if no cross reference was found
			if (!bFoundCrossReference) {
				GetPropertyData(Property, ObjToSerialize, ObjTemplate, PrefabProperty->ExportedValue);
				PrefabProperty->SaveReferencedAssetValues();
			}

			OutProperties.Add(PrefabProperty);
		}
	}

	void CollectAllSubobjects(UObject* Object, TArray<UObject*>& OutSubobjectArray)
	{
		const bool bIncludedNestedObjects = true;
		GetObjectsWithOuter(Object, OutSubobjectArray, bIncludedNestedObjects);

		// Remove contained objects that are not subobjects.
		for (int32 ComponentIndex = 0; ComponentIndex < OutSubobjectArray.Num(); ComponentIndex++)
		{
			UObject* PotentialComponent = OutSubobjectArray[ComponentIndex];
			if (!PotentialComponent->IsDefaultSubobject() && !PotentialComponent->HasAnyFlags(RF_DefaultSubObject))
			{
				OutSubobjectArray.RemoveAtSwap(ComponentIndex--);
			}
		}
	}

	void DumpSerializedProperties(const TArray<UPrefabricatorProperty*>& InProperties) {
		for (UPrefabricatorProperty* Property : InProperties) {
			UE_LOG(LogPrefabTools, Log, TEXT("%s: %s"), *Property->PropertyName, *Property->ExportedValue);
		}

	}

	void DumpSerializedData(const FPrefabricatorActorData& InActorData) {
		UE_LOG(LogPrefabTools, Log, TEXT("############################################################"));
		UE_LOG(LogPrefabTools, Log, TEXT("Actor Properties: %s"), *InActorData.ClassPathRef.GetAssetPathString());
		UE_LOG(LogPrefabTools, Log, TEXT("================="));
		DumpSerializedProperties(InActorData.Properties);

		for (const FPrefabricatorComponentData& ComponentData : InActorData.Components) {
			UE_LOG(LogPrefabTools, Log, TEXT(""));
			UE_LOG(LogPrefabTools, Log, TEXT("Component Properties: %s"), *ComponentData.ComponentName);
			UE_LOG(LogPrefabTools, Log, TEXT("================="));
			DumpSerializedProperties(ComponentData.Properties);
		}
	}
}

bool FPrefabTools::ShouldIgnorePropertySerialization(const FName& InPropertyName)
{
	static const TSet<FName> IgnoredFields = {
		"AttachParent",
		"AttachSocketName",
		"AttachChildren",
		"ClientAttachedChildren",
		"bIsEditorPreviewActor",
		"bIsEditorOnlyActor",
		"UCSModifiedProperties",
		"BlueprintCreatedComponents"
	};

	return IgnoredFields.Contains(InPropertyName);
}

bool FPrefabTools::ShouldForcePropertySerialization(const FName& PropertyName)
{
	static const TSet<FName> FieldsToForceSerialize = {
		"Mobility"
	};

	return FieldsToForceSerialize.Contains(PropertyName);
}

namespace {
	UActorComponent* FindBestComponentInCDO(AActor* CDO, UActorComponent* Component) {
		if (!CDO || !Component) return nullptr;

		for (UActorComponent* DefaultComponent : CDO->GetComponents()) {
			if (DefaultComponent && DefaultComponent->GetFName() == Component->GetFName() && DefaultComponent->GetClass() == Component->GetClass()) {
				return DefaultComponent;
			}
		}
		return nullptr;
	}
}

void FPrefabTools::SaveActorState(AActor* InActor, APrefabActor* PrefabActor, const FPrefabActorLookup& CrossReferences, FPrefabricatorActorData& OutActorData)
{
	if (!InActor) return;

	OutActorData.Components.Empty();
	OutActorData.Properties.Empty();
	
	FTransform InversePrefabTransform = PrefabActor->GetTransform().Inverse();
	FTransform LocalTransform = InActor->GetTransform() * InversePrefabTransform;
	OutActorData.RelativeTransform = LocalTransform;
	FString ClassPath = InActor->GetClass()->GetPathName();
	OutActorData.ClassPathRef = FSoftClassPath(ClassPath);
	OutActorData.ClassPath = ClassPath;
	AActor* ActorCDO = Cast<AActor>(InActor->GetArchetype());
	SerializeFields(InActor, ActorCDO, PrefabActor, CrossReferences, OutActorData.Properties);

#if WITH_EDITOR
	OutActorData.ActorName = InActor->GetActorLabel();
#endif // WITH_EDITOR

	TArray<UActorComponent*> Components;
	InActor->GetComponents(Components);

	for (UActorComponent* Component : Components) {
		int32 ComponentDataIdx = OutActorData.Components.AddDefaulted();
		FPrefabricatorComponentData& ComponentData = OutActorData.Components[ComponentDataIdx];
		
		FString ComponentClassPath = Component->GetClass()->GetPathName();
		ComponentData.ClassPathRef = FSoftClassPath(ComponentClassPath);
		ComponentData.ClassPath = ComponentClassPath;
		
		ComponentData.ComponentName = Component->GetPathName(InActor);
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component)) {
			ComponentData.RelativeTransform = SceneComponent->GetComponentTransform();
		}
		else {
			ComponentData.RelativeTransform = FTransform::Identity;
		}
		UObject* ComponentTemplate = FindBestComponentInCDO(ActorCDO, Component);
		SerializeFields(Component, ComponentTemplate, PrefabActor, CrossReferences, ComponentData.Properties);
	}

	//DumpSerializedData(OutActorData);
}

void FPrefabTools::LoadActorState(AActor* InActor, const FPrefabricatorActorData& InActorData, const FPrefabLoadSettings& InSettings)
{
	SCOPE_CYCLE_COUNTER(STAT_LoadActorState);
	if (!InActor) {
		return;
	}

	TSharedPtr<IPrefabricatorService> Service = FPrefabricatorService::Get();
	if (Service.IsValid()) {
		SCOPE_CYCLE_COUNTER(STAT_LoadActorState_BeginTransaction);
		//Service->BeginTransaction(LOCTEXT("TransLabel_LoadPrefab", "Load Prefab"));
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_LoadActorState_DeserializeFieldsActor);
		DeserializeFields(InActor, InActorData.Properties);
	}

	TMap<FString, UActorComponent*> ComponentsByName;
	TMap<FString, UActorComponent*> InstanceComponentsByName;
	for (UActorComponent* Component : InActor->GetComponents()) {
		FString ComponentPath = Component->GetPathName(InActor);
		ComponentsByName.Add(ComponentPath, Component);
		if (Component->CreationMethod == EComponentCreationMethod::Instance)
		{
			InstanceComponentsByName.Add(ComponentPath, Component);
		}
	}
	
	// Ensure the correct instance components exist since they won't have been created by creating the actor.
	for (const FPrefabricatorComponentData& ComponentData : InActorData.Components)
	{
		
		// Check for CreationMethod: Instance.
		bool bCreationMethodInstance = false;
		for (UPrefabricatorProperty* Property : ComponentData.Properties)
		{
			if (Property->PropertyName == "CreationMethod")
			{
				if (Property->ExportedValue == "Instance")
				{
					bCreationMethodInstance = true;
				}
			}
		}

		if (bCreationMethodInstance)
		{
			
			// If it's not already there, it will need created and added to the list of components on the actor.
			if (UActorComponent** SearchResult = ComponentsByName.Find(ComponentData.ComponentName); !SearchResult)
			{
				
				// Older prefab data lacked ClassPath/ClassPathRef but still possibly contained instance components that weren't actually being used.  Ignore them and warn.
				if (ComponentData.ClassPathRef.IsNull())
				{
					UE_LOG(LogPrefabTools, Warning, TEXT("Can't create prefab instance component %s on actor %s due to the prefab being an older version, try resaving the prefab"), *ComponentData.ComponentName, *InActor->GetName());
					continue;
				}
				
				UClass* ComponentClass = LoadObject<UClass>(nullptr, *ComponentData.ClassPathRef.GetAssetPathString());
				if (!ComponentClass || !ComponentClass->IsChildOf(USceneComponent::StaticClass()))
				{
					UE_LOG(LogPrefabTools, Error, TEXT("Failed to load class for component %s"), *ComponentData.ClassPathRef.GetAssetPathString());
					continue;
				}
				
				USceneComponent* NewComponent = NewObject<USceneComponent>(InActor, ComponentClass, FName(ComponentData.ComponentName));
				NewComponent->CreationMethod = EComponentCreationMethod::Instance;
				NewComponent->AttachToComponent(InActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
				NewComponent->RegisterComponent();
				InActor->AddInstanceComponent(NewComponent);
				
				FString ComponentPath = NewComponent->GetPathName(InActor);
				ComponentsByName.Add(ComponentPath, NewComponent);
			}

			// If this was an instance component, remember it was present in ComponentData.
			InstanceComponentsByName.Remove(ComponentData.ComponentName);
		}
	}
	
	// Remove any instance components that existed on the actor but were no longer present in the prefab data.
	for (const TPair<FString, UActorComponent*>& Pair : InstanceComponentsByName)
	{
		UActorComponent* UnusedComponent = Pair.Value;
		ensure(IsValid(UnusedComponent) && UnusedComponent->IsRegistered());
		UnusedComponent->UnregisterComponent();
		UnusedComponent->DestroyComponent();
	}

	{
		for (const FPrefabricatorComponentData& ComponentData : InActorData.Components) {
			if (UActorComponent** SearchResult = ComponentsByName.Find(ComponentData.ComponentName)) {
				UActorComponent* Component = *SearchResult;
				bool bPreviouslyRegister;
				{
					//SCOPE_CYCLE_COUNTER(STAT_LoadActorState_UnregisterComponent);
					bPreviouslyRegister = Component->IsRegistered();
					if (InSettings.bUnregisterComponentsBeforeLoading && bPreviouslyRegister) {
						Component->UnregisterComponent();
					}
				}

				{
					SCOPE_CYCLE_COUNTER(STAT_LoadActorState_DeserializeFieldsComponents);
					DeserializeFields(Component, ComponentData.Properties);
				}

				{
					//SCOPE_CYCLE_COUNTER(STAT_LoadActorState_RegisterComponent);
					if (InSettings.bUnregisterComponentsBeforeLoading && bPreviouslyRegister) {
						Component->RegisterComponent();
					}
				}

				// Check if we need to recreate the physics state and refresh custom primitive data
				{
					if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
					{
						bool bRecreatePhysicsState = false;
						for (UPrefabricatorProperty* Property : ComponentData.Properties)
						{
							if (Property->PropertyName == "BodyInstance")
							{
								bRecreatePhysicsState = true;
								break;
							}
						}
						if (bRecreatePhysicsState)
						{

							if(!Primitive->HasBeenInitialized())
							{
								Primitive->InitializeComponent();
							}
							Primitive->RecreatePhysicsState();
						}

						//Post load to force an update of custom primitive data
						Primitive->PostLoad();
						Primitive->MarkRenderStateDirty();
					}
				}
			}
		}
	}

	InActor->PostLoad();
	InActor->ReregisterAllComponents();

	const UPrefabricatorSettings* Settings = GetDefault<UPrefabricatorSettings>();
	if(Settings)
	{
		InActor->Tags.AddUnique(Settings->PrefabChildTag);
	}
	
#if WITH_EDITOR
	if (InActorData.ActorName.Len() > 0) {
		InActor->SetActorLabel(InActorData.ActorName);
	}
#endif // WITH_EDITOR

	if (Service.IsValid()) {
		SCOPE_CYCLE_COUNTER(STAT_LoadActorState_EndTransaction);
		//Service->EndTransaction();
	}
}

void FPrefabTools::UnlinkAndDestroyPrefabActor(APrefabActor* PrefabActor)
{
	TSharedPtr<IPrefabricatorService> Service = FPrefabricatorService::Get();
	if (Service.IsValid()) {
		Service->BeginTransaction(LOCTEXT("TransLabel_CreatePrefab", "Unlink Prefab"));
	}

	// Grab all the actors directly attached to this prefab actor
	TArray<AActor*> ChildActors;
	PrefabActor->GetAttachedActors(ChildActors);

	// Detach them from the prefab actor and cleanup
	for (AActor* ChildActor: ChildActors) {
		ChildActor->DetachFromActor(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
		ChildActor->GetRootComponent()->RemoveUserDataOfClass(UPrefabricatorAssetUserData::StaticClass());
	}

	// Finally delete the prefab actor
	PrefabActor->Destroy();

	if (Service.IsValid()) {
		Service->EndTransaction();
	}

}

void FPrefabTools::GetActorChildren(AActor* InParent, TArray<AActor*>& OutChildren)
{
	InParent->GetAttachedActors(OutChildren);
}

namespace {
	void GetPrefabBoundsRecursive(AActor* InActor, FBox& OutBounds, bool bNonColliding, const TSet<UClass*>& IgnoreActorClasses) {
		if (InActor && InActor->IsLevelBoundsRelevant()) {
			const bool bIgnoreBounds = InActor->IsA<APrefabActor>() || IgnoreActorClasses.Contains(InActor->GetClass()); 
			if (!bIgnoreBounds) {
				FBox ActorBounds(ForceInit);
				for (const UActorComponent* ActorComponent : InActor->GetComponents()) {
					if (const UPrimitiveComponent* InPrimComp = Cast<UPrimitiveComponent>(ActorComponent)) {
						if (!IgnoreActorClasses.Contains(InPrimComp->GetClass())) {
							if (InPrimComp->IsRegistered() && (bNonColliding || InPrimComp->IsCollisionEnabled())) {
								ActorBounds += InPrimComp->Bounds.GetBox();
							}
						}
					}
				}
				
				if (ActorBounds.GetExtent() == FVector::ZeroVector) {
					ActorBounds = FBox({ InActor->GetActorLocation() });
				}
				OutBounds += ActorBounds;
			}

			TArray<AActor*> AttachedActors;
			InActor->GetAttachedActors(AttachedActors);
			for (AActor* AttachedActor : AttachedActors) {
				GetPrefabBoundsRecursive(AttachedActor, OutBounds, bNonColliding, IgnoreActorClasses);
			}
		}
	}

	void DestroyActorTree(AActor* InActor) {
		if (!InActor) return;
		TArray<AActor*> Children;
		InActor->GetAttachedActors(Children);

		for (AActor* Child : Children) {
			DestroyActorTree(Child);
		}

		InActor->Destroy();
	}
}

FBox FPrefabTools::GetPrefabBounds(AActor* PrefabActor, bool bNonColliding)
{
	const UPrefabricatorSettings* Settings = GetDefault<UPrefabricatorSettings>();
	FBox Result(EForceInit::ForceInit);
	GetPrefabBoundsRecursive(PrefabActor, Result, bNonColliding, Settings->IgnoreBoundingBoxForObjects);
	return Result;
}

void FPrefabTools::LoadStateFromPrefabAsset(APrefabActor* PrefabActor, const FPrefabLoadSettings& InSettings, bool ForceUpdate/*= false*/, bool IgnoreLastUpdate)
{
	SCOPE_CYCLE_COUNTER(STAT_LoadStateFromPrefabAsset);
	if (!PrefabActor) {
		UE_LOG(LogPrefabTools, Error, TEXT("Invalid prefab actor reference"));
		return;
	}

	UPrefabricatorAsset* PrefabAsset = PrefabActor->GetPrefabAsset();
	if (!PrefabAsset) {
		//UE_LOG(LogPrefabTools, Error, TEXT("Prefab asset is not assigned correctly"));
		return;
	}

	bool bPrefabOutOfDate = PrefabActor->LastUpdateID != PrefabAsset->LastUpdateID;
	if(!bPrefabOutOfDate && !ForceUpdate && !IgnoreLastUpdate)
	{
		return;
	}
	
#if WITH_EDITOR
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (SourceControlModule.IsEnabled() && SourceControlModule.GetProvider().IsAvailable())
	{
		TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> State = SourceControlModule.GetProvider().GetState(PrefabActor->GetPackage(), EStateCacheUsage::ForceUpdate);
		if (State.IsValid() && State->IsCheckedOutOther())
		{
			return;//Don't update prefabs checked out by someone else, they'll get updated when the lock is free
		}
	}
#endif
	
	PrefabActor->GetRootComponent()->SetMobility(PrefabAsset->PrefabMobility);

	// Pool existing child actors that belong to this prefab
	TArray<AActor*> ExistingActorPool;
	GetActorChildren(PrefabActor, ExistingActorPool);

	FPrefabInstanceTemplates* LoadState = FGlobalPrefabInstanceTemplates::Get();
	TSharedPtr<IPrefabricatorService> Service = FPrefabricatorService::Get();

	TMap<FGuid, AActor*> ActorByItemID;
	TMap<FGuid, FGuid> LastUpdateByItemID;
	for (AActor* ExistingActor : ExistingActorPool) {
		if (ExistingActor && ExistingActor->GetRootComponent()) {
			UPrefabricatorAssetUserData* PrefabUserData = ExistingActor->GetRootComponent()->GetAssetUserData<UPrefabricatorAssetUserData>();
			if (PrefabUserData && PrefabUserData->PrefabActor == PrefabActor) {
				TArray<AActor*> ChildActors;
				ActorByItemID.Add(PrefabUserData->ItemID, ExistingActor);
				LastUpdateByItemID.Add(PrefabUserData->ItemID, PrefabUserData->ActorLastUpdateID);
			}
		}
	}

	if (Service.IsValid()) {
		UWorld* World = PrefabActor->GetWorld();
		TMap<FGuid, AActor*> PrefabItemToActorMap;

		for (FPrefabricatorActorData& ActorItemData : PrefabAsset->ActorData) {
			// Handle backward compatibility
			if (!ActorItemData.ClassPathRef.IsValid()) {
				ActorItemData.ClassPathRef = ActorItemData.ClassPath;
			}

			if (ActorItemData.ClassPathRef.GetAssetPathString() != ActorItemData.ClassPath) {
				ActorItemData.ClassPath = ActorItemData.ClassPathRef.GetAssetPathString();
			}

			UClass* ActorClass = LoadObject<UClass>(nullptr, *ActorItemData.ClassPathRef.GetAssetPathString());
			if (!ActorClass) continue;

			// Try to re-use an existing actor from this prefab
			AActor* ChildActor = nullptr;

			bool ActorIsOutOfDate = true;
			FGuid* UpdateGUID = LastUpdateByItemID.Find(ActorItemData.PrefabItemID);
			if(UpdateGUID && ActorItemData.ActorLastUpdateID == *UpdateGUID)
			{
				ActorIsOutOfDate = ForceUpdate;//If we're forcing an update the actor is always out of date
			}
			
			// Always try and reuse Actors if we can
			if (AActor** SearchResult = ActorByItemID.Find(ActorItemData.PrefabItemID))
			{
				ChildActor = *SearchResult;
				if (ChildActor)
				{
					FString ExistingClassName = ChildActor->GetClass()->GetPathName();
					FString RequiredClassName = ActorItemData.ClassPathRef.GetAssetPathString();
					if (ExistingClassName == RequiredClassName)
					{
						// We can reuse this actor
						ExistingActorPool.Remove(ChildActor);
						ActorByItemID.Remove(ActorItemData.PrefabItemID);
						LastUpdateByItemID.Remove(ActorItemData.PrefabItemID);
					}
					else
					{
						ChildActor = nullptr;
					}
				}
			
			}

			//If the actor isn't out of date we don't need to do anything at all
			if(!ActorIsOutOfDate)
			{
				continue;
			}

			FTransform WorldTransform = ActorItemData.RelativeTransform * PrefabActor->GetTransform();
			if (!ChildActor) {
				// Create a new child actor.  Try to create it from an existing template actor that is already preset in the scene
				AActor* Template = nullptr;
				if (LoadState && InSettings.bCanLoadFromCachedTemplate) {
					Template = LoadState->GetTemplate(ActorItemData.PrefabItemID, PrefabAsset->LastUpdateID);
				}

				ChildActor = Service->SpawnActor(ActorClass, WorldTransform, PrefabActor->GetLevel(), Template);
				
				ParentActors(PrefabActor, ChildActor);

				if (Template == nullptr || (bPrefabOutOfDate)) {
					// We couldn't use a template,  so load the prefab properties in
					LoadActorState(ChildActor, ActorItemData, InSettings);

					// Save this as a template for future reuse
					if (LoadState && InSettings.bCanSaveToCachedTemplate) {
						LoadState->RegisterTemplate(ActorItemData.PrefabItemID, PrefabAsset->LastUpdateID, ChildActor);
					}
				}
				
			}
			else {
				// This actor was reused.  re-parent it
				ChildActor->DetachFromActor(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
				ParentActors(PrefabActor, ChildActor);
				
				// Update the world transform.   The reuse happens only on leaf actors (which don't have any further child actors)
				if (ChildActor->GetRootComponent())
				{
					EComponentMobility::Type OldChildMobility = ChildActor->GetRootComponent()->Mobility;
					ChildActor->GetRootComponent()->SetMobility(EComponentMobility::Movable);
					ChildActor->SetActorTransform(WorldTransform);
					ChildActor->GetRootComponent()->SetMobility(OldChildMobility);
				}

				{
					//Actor is out of date so we need to update it
					LoadActorState(ChildActor, ActorItemData, InSettings);
				}
				
			}

			AssignAssetUserData(ChildActor, ActorItemData.PrefabItemID, PrefabActor);

			//We've updated to the target update ID
			if(ChildActor->GetRootComponent())
			{
				UPrefabricatorAssetUserData* PrefabUserData = ChildActor->GetRootComponent()->GetAssetUserData<UPrefabricatorAssetUserData>();
				if(PrefabUserData)
				{
					PrefabUserData->ActorLastUpdateID = ActorItemData.ActorLastUpdateID;
				}
			}


			if (APrefabActor* ChildPrefab = Cast<APrefabActor>(ChildActor)) {
				SCOPE_CYCLE_COUNTER(STAT_LoadStateFromPrefabAsset5);
				if (InSettings.bRandomizeNestedSeed && InSettings.Random) {
					// This is a nested child prefab.  Randomize the seed of the child prefab
					ChildPrefab->Seed = FPrefabTools::GetRandomSeed(*InSettings.Random);
				}
				if (InSettings.bSynchronousBuild) {
					LoadStateFromPrefabAsset(ChildPrefab, InSettings, false, true);
				}

				const UPrefabricatorSettings* Settings = GetDefault<UPrefabricatorSettings>();
				if(Settings)
				{
					ChildPrefab->Tags.AddUnique(Settings->NestedPrefabTag);
				}
			}


#if WITH_EDITOR
			ChildActor->MarkPackageDirty();
#endif 
		}

		// Fix up the cross references
		{
			for (const FPrefabricatorActorData& ActorItemData : PrefabAsset->ActorData) {
				AActor** ActorPtr = PrefabItemToActorMap.Find(ActorItemData.PrefabItemID);
				if (!ActorPtr) continue;

				AActor* Actor = *ActorPtr;
				FixupCrossReferences(ActorItemData.Properties, Actor, PrefabItemToActorMap);

				TMap<FString, UActorComponent*> ComponentByPath;
				for (UActorComponent* Component : Actor->GetComponents()) {
					FString ComponentPath = Component->GetPathName(Actor);
					UActorComponent*& ComponentRef = ComponentByPath.FindOrAdd(ComponentPath);
					ComponentRef = Component;
				}

				for (const FPrefabricatorComponentData& ComponentData : ActorItemData.Components) {
					UActorComponent** ComponentPtr = ComponentByPath.Find(ComponentData.ComponentName);
					UActorComponent* Component = ComponentPtr ? *ComponentPtr : nullptr;
					if (!ComponentPtr) continue;

					FixupCrossReferences(ComponentData.Properties, Component, PrefabItemToActorMap);
				}
			}

		}

	}

	// Destroy the unused actors from the pool
	for (AActor* UnusedActor : ExistingActorPool)
	{
		DestroyActorTree(UnusedActor);
		
#if WITH_EDITOR
		UnusedActor->MarkPackageDirty();
#endif 
	}

	PrefabActor->LastUpdateID = PrefabAsset->LastUpdateID;

	const UPrefabricatorSettings* Settings = GetDefault<UPrefabricatorSettings>();
	if(Settings)
	{
		PrefabActor->Tags.AddUnique(Settings->PrefabParentTag);
	}
	
	if (InSettings.bSynchronousBuild) {
		PrefabActor->HandleBuildComplete();
	}

#if WITH_EDITOR
	PrefabActor->MarkPackageDirty();
#endif
}

void FPrefabTools::FixupCrossReferences(const TArray<UPrefabricatorProperty*>& PrefabProperties, UObject* ObjToWrite, TMap<FGuid, AActor*>& PrefabItemToActorMap)
{
	for (UPrefabricatorProperty* PrefabProperty : PrefabProperties) {
		if (!PrefabProperty || !PrefabProperty->bIsCrossReferencedActor) continue;

		FProperty* Property = ObjToWrite->GetClass()->FindPropertyByName(*PrefabProperty->PropertyName);

		const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);
		if (!ObjectProperty) continue;

		AActor** SearchResult = PrefabItemToActorMap.Find(PrefabProperty->CrossReferencePrefabActorId);
		if (!SearchResult) continue;
		AActor* CrossReference = *SearchResult;

		ObjectProperty->SetObjectPropertyValue_InContainer(ObjToWrite, CrossReference);

		////////
		FString ActorName = CrossReference ? CrossReference->GetName() : "[NONE]";
		UE_LOG(LogPrefabTools, Log, TEXT("Cross Reference: %s -> %s"), *PrefabProperty->CrossReferencePrefabActorId.ToString(), *ActorName);
		////////
	}
}

void FPrefabVersionControl::UpgradeToLatestVersion(UPrefabricatorAsset* PrefabAsset)
{
	if (PrefabAsset->Version == (int32)EPrefabricatorAssetVersion::InitialVersion) {
		UpgradeFromVersion_InitialVersion(PrefabAsset);
	}

	if (PrefabAsset->Version == (int32)EPrefabricatorAssetVersion::AddedSoftReference) {
		UpgradeFromVersion_AddedSoftReferences(PrefabAsset);
	}

	if (PrefabAsset->Version == (int32)EPrefabricatorAssetVersion::AddedSoftReference_PrefabFix) {
		UpgradeFromVersion_AddedSoftReferencesPrefabFix(PrefabAsset);
	}

	//....

}

void FPrefabVersionControl::UpgradeFromVersion_InitialVersion(UPrefabricatorAsset* PrefabAsset)
{
	check(PrefabAsset->Version == (int32)EPrefabricatorAssetVersion::InitialVersion);

	RefreshReferenceList(PrefabAsset);

	PrefabAsset->Version = (int32)EPrefabricatorAssetVersion::AddedSoftReference;
}

void FPrefabVersionControl::UpgradeFromVersion_AddedSoftReferences(UPrefabricatorAsset* PrefabAsset)
{
	check(PrefabAsset->Version == (int32)EPrefabricatorAssetVersion::AddedSoftReference);

	RefreshReferenceList(PrefabAsset);

	PrefabAsset->Version = (int32)EPrefabricatorAssetVersion::AddedSoftReference_PrefabFix;
}

void FPrefabVersionControl::UpgradeFromVersion_AddedSoftReferencesPrefabFix(UPrefabricatorAsset* PrefabAsset)
{
	check(PrefabAsset->Version == (int32)EPrefabricatorAssetVersion::AddedSoftReference_PrefabFix);

	// Handle upgrade here to move to the next version
}

void FPrefabVersionControl::RefreshReferenceList(UPrefabricatorAsset* PrefabAsset)
{
	for (FPrefabricatorActorData& Entry : PrefabAsset->ActorData) {
		for (UPrefabricatorProperty* ActorProperty : Entry.Properties) {
			ActorProperty->SaveReferencedAssetValues();
		}

		for (FPrefabricatorComponentData& ComponentData : Entry.Components) {
			for (UPrefabricatorProperty* ComponentProperty : ComponentData.Properties) {
				ComponentProperty->SaveReferencedAssetValues();
			}
		}
	}

	PrefabAsset->Modify();
}

/////////////////////// FGlobalPrefabLoadState /////////////////////// 

FPrefabInstanceTemplates* FGlobalPrefabInstanceTemplates::Instance = nullptr;
void FGlobalPrefabInstanceTemplates::_CreateSingleton()
{
	check(Instance == nullptr);
	Instance = new FPrefabInstanceTemplates();
}

void FGlobalPrefabInstanceTemplates::_ReleaseSingleton()
{
	delete Instance;
	Instance = nullptr;
}

void FPrefabInstanceTemplates::RegisterTemplate(const FGuid& InPrefabItemId, FGuid InPrefabLastUpdateId, AActor* InActor)
{
	FPrefabInstanceTemplateInfo& TemplateRef = PrefabItemTemplates.FindOrAdd(InPrefabItemId);
	TemplateRef.TemplatePtr = InActor;
	TemplateRef.PrefabLastUpdateId = InPrefabLastUpdateId;
}

AActor* FPrefabInstanceTemplates::GetTemplate(const FGuid& InPrefabItemId, FGuid InPrefabLastUpdateId)
{
	FPrefabInstanceTemplateInfo* SearchResult = PrefabItemTemplates.Find(InPrefabItemId);
	if (!SearchResult) return nullptr;
	FPrefabInstanceTemplateInfo& Info = *SearchResult;
	AActor* Actor = Info.TemplatePtr.Get();

	if (Info.PrefabLastUpdateId != InPrefabLastUpdateId) {
		// The prefab has been changed since we last cached this template. Invalidate it
		Actor = nullptr;
	}

	// Remove from the map if the actor state is stale
	if (!Actor) {
		PrefabItemTemplates.Remove(InPrefabItemId);
	}

	return Actor;
}


///////////////////////////////// FPrefabSaveModeCrossReferences ///////////////////////////////// 


void FPrefabActorLookup::Register(const FString& InActorPath, const FGuid& InPrefabItemId)
{
	FGuid& ItemIdRef = ActorPathToItemId.FindOrAdd(InActorPath);
	ItemIdRef = InPrefabItemId;
}

void FPrefabActorLookup::Register(AActor* InActor, const FGuid& InPrefabItemId)
{
	if (!InActor) return;
	Register(InActor->GetPathName(), InPrefabItemId);
}

bool FPrefabActorLookup::GetPrefabItemId(const FString& InObjectPath, FGuid& OutCrossRefPrefabItem) const
{
	const FGuid* SearchResult = ActorPathToItemId.Find(InObjectPath);
	if (SearchResult) {
		OutCrossRefPrefabItem = *SearchResult;
		return true;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE

