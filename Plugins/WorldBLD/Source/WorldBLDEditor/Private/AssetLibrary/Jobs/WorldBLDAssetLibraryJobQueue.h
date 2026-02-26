#pragma once

#include "CoreMinimal.h"
#include "TickableEditorObject.h"

class UWorldBLDAssetDownloadManager;

class IWorldBLDAssetJob
{
public:
	virtual ~IWorldBLDAssetJob() = default;

	// Return true when finished.
	virtual bool TickJob(float DeltaTime) = 0;
};

class FWorldBLDAssetLibraryJobQueue : public FTickableEditorObject
{
public:
	explicit FWorldBLDAssetLibraryJobQueue(TWeakObjectPtr<UWorldBLDAssetDownloadManager> InManager);
	virtual ~FWorldBLDAssetLibraryJobQueue() override = default;

	void Enqueue(TSharedPtr<IWorldBLDAssetJob> Job);

	bool IsBusy() const;
	bool CanExecuteNow() const;

	//~ Begin FTickableEditorObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	//~ End FTickableEditorObject

private:
	mutable FCriticalSection QueueMutex;
	TQueue<TSharedPtr<IWorldBLDAssetJob>> Queue;
	TSharedPtr<IWorldBLDAssetJob> ActiveJob;

	TWeakObjectPtr<UWorldBLDAssetDownloadManager> Manager;
};

