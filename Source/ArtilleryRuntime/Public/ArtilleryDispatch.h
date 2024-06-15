// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UBristleconeWorldSubsystem.h"
#include "UCablingWorldSubsystem.h"
#include "ArtilleryCommonTypes.h"
#include "FArtilleryBusyWorker.h"
#include "CanonicalInputStreamECS.h"
#include <map>

#include "UFireControlMachine.h"
#include "ArtilleryDispatch.generated.h"


/**
 * This is the backbone of artillery, and along with the ArtilleryGuns and ArtilleryFireControlMachines, 
 * this is most of what any other code will see of artillery in C++. Blueprints will mostly not even see that
 * as they are already encapsulated in the artillerygun's ability sequencer. Abilities merely need to be uninstanced,
 * only modify attributes tracked in Artillery, and use artillery bp nodes where possible.
 * 
 * No unusual additional serialization or similar is required for determinism. I hope.
 * 
 * For an organized view of the code, start by understanding that most work is done outside the gamethread.
 * Bristlecone handles network inputs, Cabling handles keymapping and local inputs. Artillery handles outcomes.
 * 
 * Here's the core rule:
 * 
 * Core gameplay simulation happens in the ArtilleryBusyWorker at about 120hz.
 * Anything that cannot cause the core gameplay sim to become desyncrhonized happens in the game thread.
 * Surprisingly, this means _most things flow through UE normally and happen on the game thread & render threads._
 * 
 * Our goal is that cosmetic ability components, animation, IK, particle systems, cloth, and really, almost everything
 * happens in UE normally. Rollbacks don't happen in the broader UE system at all, and in fact, most of UE acts like its
 * running in a pretty simple Iris push config.
 * 
 * Artillery just pushes transform and ability updates upward as needed. That's a big just, but it really is a narrow scope.
 * 
 * Iris does normal replication on a slow cadence as a fall back and to provide attribute sync reassurances.
 */
UCLASS()
class ARTILLERYRUNTIME_API UArtilleryDispatch : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	friend class FArtilleryBusyWorker;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;


	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//fully specifying the type is necessary to prevent spurious warnings in some cases.
	TSharedPtr<TCircularQueue<std::pair<FGunKey, Arty::ArtilleryTime>>> ActionsToOrder;
	TSharedPtr<TCircularQueue<std::pair<FGunKey, Arty::ArtilleryTime>>> ActionsToReconcile;
	//this is THE function we use to queue up Gun Activations.
	//These will eventually need a complete and consistent ordering to ensure determinism.
	//copy op is intentional but may be unneeded. assess before revising signature.
	//TODO: assess if this needs to be a multimap. I think it needs to NOT be.
	TMap<FGunKey, FireControlKey> GunToMachineMapping;
	
	void queueFire(FGunKey Key, Arty::ArtilleryTime Time)
	{
		if (ActionsToOrder && ActionsToOrder.IsValid())
		{
			ActionsToOrder->Enqueue(std::pair<FGunKey, Arty::ArtilleryTime>(Key, Time));
		}
	};

	void queueResimFire(FGunKey Key, Arty::ArtilleryTime Time)
	{
		if (ActionsToOrder && ActionsToOrder.IsValid())
		{
			ActionsToReconcile->Enqueue(std::pair<FGunKey, Arty::ArtilleryTime>(Key, Time));
		}
	};

	void RunGuns()
	{
		//oh, didjerthink it were to go the other way round? Aye, I could be seeing why ye might.
		TMultiMap<Arty::ArtilleryTime, FGunKey> TimeSortedFireEvents_ReplaceWithTripleBuffer;
		UCanonicalInputStreamECS* MyBrother = GetWorld()->GetSubsystem<UCanonicalInputStreamECS>();
		if (ActionsToOrder && ActionsToOrder.IsValid())
		{
			while (!ActionsToOrder->IsEmpty())
			{
				auto toLoad = *ActionsToOrder->Peek();
				TimeSortedFireEvents_ReplaceWithTripleBuffer.Add(toLoad.second, toLoad.first);
				ActionsToOrder->Dequeue();
			}
		}
		//map is ordered in C++. Real C++, I mean.
		for (auto x : TimeSortedFireEvents_ReplaceWithTripleBuffer)
		{
			GunToMachineMapping.Find(x.Value);
		}
	};


	//********************************
	//DUMMY. USED BY RECONCILE AND RERUN.
	//Here to demonstrate that we actually queue normal and rerun separately.
	//We can't risk intermingling them, which should never happen, but...
	//c'mon. Seriously. you wanna find that bug?
	//********************************
	void RERunGuns()
	{
		//oh, didjerthink it were to go the other way round? Aye, I could be seeing why ye might.
		TMultiMap<const Arty::ArtilleryTime, FGunKey> TimeSortedFireEvents_ReplaceWithTripleBuffer;
		UCanonicalInputStreamECS* MyBrother = GetWorld()->GetSubsystem<UCanonicalInputStreamECS>();
		if (ActionsToReconcile && ActionsToReconcile.IsValid())
		{
			while (!ActionsToReconcile->IsEmpty())
			{
				auto toLoad = *ActionsToReconcile->Peek();
				//I hate tmap. and their multimap is a barn fire.
				TimeSortedFireEvents_ReplaceWithTripleBuffer.Add(toLoad.second, toLoad.first);
				ActionsToReconcile->Dequeue();
			}
		}
		//map is ordered in C++. Real C++, I mean.
		for (auto x : TimeSortedFireEvents_ReplaceWithTripleBuffer)
		{
			throw;
		}
	};

public:
	//DUMMY FOR NOW.
	//TODO: IMPLEMENT THE GUNMAP FROM INSTANCE UNTO CLASS
	//TODO: REMEMBER TO SAY AMMO A BUNCH
	FGunKey GetNewGunKey(FString GunDefinitionID, FireControlKey MachineKey);


	std::atomic_bool UseNetworkInput;
	bool missedPrior = false;
	bool burstDropDetected = false;

private:
	static inline long long monotonkey = 0;
	//If you're trying to figure out how artillery works, read the busy worker knowing it's a single thread coming off of Dispatch.
	//this handles input from bristlecone, patching it into streams from the CanonicalInputStreamECS (ACIS), using the ACIS to perform mappings,
	//and processing those streams using the pattern matcher. right now, we also expect it to own rollback and jolt when that's implemented.
	//
	// 
	// This is quite a lot and for performance or legibility reasons there is a good chance that this thread will
	//need to be split up. On the other hand, we want each loop iteration to run for around 2-4 milliseconds.
	//this is quite a bit.
	FArtilleryBusyWorker ArtilleryAsyncWorldSim;
	TUniquePtr<FRunnableThread> WorldSim_Thread;
};
