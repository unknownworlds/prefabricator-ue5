//$ Copyright 2015-23, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "PrefabricatorAsset.generated.h"

class APrefabActor;

USTRUCT()
struct PREFABRICATORRUNTIME_API FPrefabricatorPropertyAssetMapping {
	GENERATED_BODY()

	UPROPERTY()
	FSoftObjectPath AssetReference;

	UPROPERTY()
	FString AssetClassName;

	UPROPERTY()
	FName AssetObjectPath;

	UPROPERTY()
	bool bUseQuotes = false;
};

UCLASS()
class PREFABRICATORRUNTIME_API UPrefabricatorProperty : public UObject {
	GENERATED_BODY()
public:
	UPROPERTY()
	FString PropertyName;

	UPROPERTY()
	FString ExportedValue;

	UPROPERTY()
	TArray<FPrefabricatorPropertyAssetMapping> AssetSoftReferenceMappings;

	UPROPERTY()
	bool bIsCrossReferencedActor = false;

	UPROPERTY()
	FGuid CrossReferencePrefabActorId;

	void SaveReferencedAssetValues();
	void LoadReferencedAssetValues();

	
	inline bool Equals(const UPrefabricatorProperty& Other) const
	{
		bool PropEqual = Other.PropertyName.Equals(PropertyName)
					&& Other.ExportedValue.Equals(ExportedValue);

		return PropEqual;
	}
};

USTRUCT()
struct PREFABRICATORRUNTIME_API FPrefabricatorComponentData {
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FTransform RelativeTransform;

	UPROPERTY()
	FString ComponentName;

	UPROPERTY()
	TArray<UPrefabricatorProperty*> Properties;

	inline bool Equals(const FPrefabricatorComponentData& Other) const
	{
		bool Equal = ComponentName.Equals(Other.ComponentName)
		&& RelativeTransform.Equals(Other.RelativeTransform);

		if(!Equal)
		{
			return false;
		}

		if(Properties.Num() != Other.Properties.Num())
		{
			return false;
		}
		
		//Check saved properties for differences
		if(Properties.Num() != Other.Properties.Num())
		{
			return false;
		}
		
		for(int32 i=0; i<Properties.Num(); i++)
		{
			if(!Properties[i]->Equals(*Other.Properties[i]))
			{
				return false;
			}
		}

		return true;
	}
};

USTRUCT()
struct PREFABRICATORRUNTIME_API FPrefabricatorActorData {
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FGuid PrefabItemID;
	
	// ID for the last time this actor was updated so we can update each actor not the entire prefab each time
	UPROPERTY()
	FGuid ActorLastUpdateID;
	
	UPROPERTY()
	FTransform RelativeTransform;

	UPROPERTY()
	FString ClassPath;

	UPROPERTY()
	FSoftClassPath ClassPathRef;

	UPROPERTY()
	TArray<UPrefabricatorProperty*> Properties;

	UPROPERTY()
	TArray<FPrefabricatorComponentData> Components;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString ActorName;
#endif // WITH_EDITORONLY_DATA

	inline bool Equals(const FPrefabricatorActorData& Other) const
	{
		bool Equal = PrefabItemID == Other.PrefabItemID
		&& ActorLastUpdateID == Other.ActorLastUpdateID
		&& RelativeTransform.Equals(Other.RelativeTransform)
		&& ClassPathRef == Other.ClassPathRef;
		
		if(!Equal)
		{
			return false;
		}

		//Check saved properties for differences
		if(Properties.Num() != Other.Properties.Num())
		{
			return false;
		}
		
		for(int32 i=0; i<Properties.Num(); i++)
		{
			if(!Properties[i]->Equals(*Other.Properties[i]))
			{
				return false;
			}
		}

		//Check components for differences
		if(Components.Num() != Other.Components.Num())
		{
			return false;
		}
		
		for(int32 i=0; i<Components.Num(); i++)
		{
			if(!Components[i].Equals(Other.Components[i]))
			{
				return false;
			}
		}

		return true;
	}
};

struct FPrefabAssetSelectionConfig {
	int32 Seed = 0;
};

UCLASS(Blueprintable)
class PREFABRICATORRUNTIME_API UPrefabricatorEventListener : public UObject {
	GENERATED_BODY()
public:
	/** Called when the prefab and all its child prefabs have been spawned and initialized */
	UFUNCTION(BlueprintNativeEvent, Category = "Prefabricator")
	void PostSpawn(APrefabActor* Prefab);
	virtual void PostSpawn_Implementation(APrefabActor* Prefab);
};

UCLASS(Abstract, BlueprintType, Blueprintable)
class PREFABRICATORRUNTIME_API UPrefabricatorAssetInterface : public UObject {
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Prefabricator")
	TSubclassOf<UPrefabricatorEventListener> EventListener;

	UPROPERTY(EditAnywhere, Category = "Replication")
	bool bReplicates = false;

public:
	virtual class UPrefabricatorAsset* GetPrefabAsset(const FPrefabAssetSelectionConfig& InConfig) { return nullptr; }
};

enum class EPrefabricatorAssetVersion {
	InitialVersion = 0,
	AddedSoftReference,
	AddedSoftReference_PrefabFix,

	//----------- Versions should be placed above this line -----------------
	LastVersionPlusOne,
	LatestVersion = LastVersionPlusOne -1
};

UCLASS(Blueprintable)
class PREFABRICATORRUNTIME_API UPrefabricatorAsset : public UPrefabricatorAssetInterface {
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY()
	TArray<FPrefabricatorActorData> ActorData;

	UPROPERTY()
	TEnumAsByte<EComponentMobility::Type> PrefabMobility;

	// The ID that is regenerated on every update
	// This allows prefab actors to test against their own LastUpdateID and determine if a refresh is needed
	UPROPERTY()
	FGuid LastUpdateID;


	/** Information for thumbnail rendering */
	UPROPERTY()
	class UThumbnailInfo* ThumbnailInfo;

	UPROPERTY()
	uint32 Version;

public:
	virtual UPrefabricatorAsset* GetPrefabAsset(const FPrefabAssetSelectionConfig& InConfig) override;
};


USTRUCT(BlueprintType)
struct PREFABRICATORRUNTIME_API FPrefabricatorAssetCollectionItem {
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Prefabricator")
	TSoftObjectPtr<UPrefabricatorAsset> PrefabAsset;

	UPROPERTY(EditAnywhere, Category = "Prefabricator")
	float Weight = 1.0f;
};

enum class EPrefabricatorCollectionAssetVersion {
	InitialVersion = 0,

	//----------- Versions should be placed above this line -----------------
	LastVersionPlusOne,
	LatestVersion = LastVersionPlusOne - 1
};

UCLASS(Blueprintable)
class PREFABRICATORRUNTIME_API UPrefabricatorAssetCollection : public UPrefabricatorAssetInterface {
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Prefabricator")
	TArray<FPrefabricatorAssetCollectionItem> Prefabs;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Prefabricator")
	TSoftObjectPtr<UTexture2D> CustomThumbnail;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY()
	uint32 Version;

public:
	virtual UPrefabricatorAsset* GetPrefabAsset(const FPrefabAssetSelectionConfig& InConfig) override;
};


class PREFABRICATORRUNTIME_API FPrefabricatorAssetUtils {
public:
	static FVector FindPivot(const TArray<AActor*>& InActors);
	static EComponentMobility::Type FindMobility(const TArray<AActor*>& InActors);
	
};

