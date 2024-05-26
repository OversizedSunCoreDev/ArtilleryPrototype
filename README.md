# Artillery: Guns & Abilities Plugin for Lions
Complexity and access management primitives married to a core set of basic weapon abilities used to bind to a weapon or character actor in a compositional and elegant way, compatible with GAS. Artillery is built on top of GAS, and is generally a fairly thin layer over it, providing convenience functions and ECS-like capabilities. Critically, Artillery is also used for making a few changes to ensure that we can keep debug complexity in the networked case to a minimum.   
  
GAS is used in fortnite, among other major titles, including a number of Vampire Survivor successors. Artillery will also serve to insulate our other tech from spaghetti usage of GAS, in case we do decide to use another tooling for ability and gun composing. [This guide](https://github.com/tranek/GASDocumentation) is a really useful place to start for understanding GAS. We're using it very slightly differently, in a more composition oriented way that'll hopefully ease the dependency management and granularity issues we ran into at Riot if we go with using Game Feature Plugins.
  
## Networking  
This plugin will:   
- Disable replication and RPCs for all played or used abilities TO the server.   
- Accept push updates from the server. As a result, replication FROM the server must still work.   
- Integrate with the reconciliation logic needed to do an eventually consistent server auth game. See 2.   
- Prevent known issues around latency and cooldown misbehavior in the normal up-replication by offlining it entirely if needed.   
- Ensure integration with Iris, which will be our replication provider for Lions.   
   
## Design   
This plugin has five main design goals:  
- All data is managed through Data-Driven Gameplay Elements, Registries or Configs.
- Abilities are kept small and atomic, intended for use by composition rather than as monolithic abilities.  
- Map simply and elegantly to designs that could be found in ANY OR ALL OF: borderlands 2, Vampire Survivors, Destiny 2.  
- Cleanly express weapon-centric design patterns AND character-centric design patterns. When picking, prefer weapons.  
- Avoid tight coupling between characters, abilities, weapons, spawnables, and pickups. Failure to do this will be excruciating later.
- Where necessary, integrate with Thistle via a hard API boundary and use of Cues. Minimize this.
- Either own or explicitly do not own animation and game feel of abilities and guns.
   
  
To meet these needs, we anticipate a mix of components and gameplay abilities, with relatively few tags and relatively few cues. Cues should, as much as possible, be used only for presentation layer effects. If a cue triggers something that has a physics engine interaction, it could produce some very very unpleasant bugs during the process of attempting reconciliation and replication. The GAS library goes to great pains to prevent this, but fundamentally, evented systems are difficult to express in a way that is deterministically ordered without additional primitives that would overcomplicate GAS.  
  
Finally, this library aims to use GAS in an idiomatic way. It's used extensively for games very much like ours, by very good designers and very smart engineers. It is not safe to assume that the design is bad because it has unusual properties. On FLX at Riot, we lost months due to people skipping GAS in favor of hand-rolling a nearly identical system. I'd like to not repeat that mistake.

## Reference Matter
- [Data Driven Gameplay](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-driven-gameplay-elements-in-unreal-engine?application_version=5.4)
- [GAS, Top-level Docs](https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/GameplayAbilitySystem/)
- [Data Registries](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-registries-in-unreal-engine)
- [Unofficial Companion](https://github.com/tranek/GASDocumentation) - probably THE best actual guide to how GAS and why GAS
- [Procedural Animation With Character](https://www.youtube.com/watch?v=KPoeNZZ6H4s) - useful for missiles, turning turrets, etc.
- [Momentum in Sonic](https://www.youtube.com/watch?v=w1CEN5gVs5Q)
- [JUICE IT OR LOSE IT](https://www.youtube.com/watch?v=Fy0aCDmgnxg)
- [Tech Design In Animation](https://www.youtube.com/watch?v=ueEmiDM94IE)
