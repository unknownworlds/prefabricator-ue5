//$ Copyright 2015-23, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TickableEditorObject.h"
#include "Containers/Queue.h"

struct FPrefabricatorSelectionRequest {
	TWeakObjectPtr<AActor> Actor;
	bool bSelected;
};

UENUM()
enum EPrefabricatorSelectMode : uint8
{
	Default,		// The default selection mode everyone is used to
	RootOnly,		// Only select the root actors
	ComponentOnly	// Only select the component actors
};

class PREFABRICATOREDITOR_API FPrefabricatorSelectionHook : public FTickableEditorObject {
public:
	void Initialize();
	void Release();

	//~ Begin FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableEditorObject Interface

private:
	void OnObjectSelected(UObject* Object);
	void OnSelectNone();

private:
	FDelegateHandle CallbackHandle_SelectObject;
	FDelegateHandle CallbackHandle_SelectNone;
	TWeakObjectPtr<AActor> LastSelectedObject;
	bool bSelectionGuard = false;
	TQueue<FPrefabricatorSelectionRequest> SelectionRequests;
};

