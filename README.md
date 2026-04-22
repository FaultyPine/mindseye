# mindseye


## Building

Must be on windows.
run `build.bat`

### Current focus

Finishing the asset system todo:

- ~~implement "asset templates" and "asset instances" separate resource pools~~
    - this also now means a meAsset can refer to a asset template OR an instance asset, which i think is a good thing
- "asset instances" should start as copies of their asset template
    -  i have no actual use for this rn, but i will need it later.
- enforce some read-only-ness to asset templates, unless it's explicitly specified that we're in "live edit" mode


In other engines, there's a "disk asset" structure, that is loaded, and is typically read-only(ish). Then when you want an *instance* of that asset in the world, you copy that disk asset structure into a completely different structure which is your "runtime" asset, and can have different data than the disk one.
In those engines, you can only save the disk assets, you never "save" the runtime ones.
In this engine, i'm trying out what it would b like to not have that separation - to have disk & runtime assets share the same type.
So, that means i'll need at least 2 instances of the asset type in memory. One read-only which is the representation of the asset on disk, and can be saved... and N "runtime" instances of that type, all of which start off as copies of the disk asset.
This can be implemented as 2 internal pools per resource pool, one for readonly editor stuff, and one for the runtime stuff.
That way, i can "load" the ro editor resource, then copy it for multiple instances of that resource in the scene

Editor thoughts:
Editor should never be able to "edit" runtime resources - only disk assets.
Tricky piece: what happens when you click to edit an Entity in the world? Since that could be an instance of an Entity on disk... 
    In this case, there needs to be a way to distinguish user intention - do you want to edit the actual Entity definition, which may affect all instances? Or do you want to edit just this instance in the current scene?
    If you want just this instance, we need a way to serialize "overridden" parts of a type - so like a Scene would have a reference to an Entity on disk, but it could "override" the position - different than the position that's on disk.


- Serializing separate files
    - like rn, when we serialize an Entity, it's content is put in the scene file.
        BOOKMARK: it should be in a separate test.ent.masset, and the scene one should just have an id that refers to that one
            scene now holds id. Need to ensure referenced entity is serialized
                Instead of "save scene" button, it should be a "save all" button. assetindex should track "dirty" assets, and save em all
    - implement reflection & asset loader registration for other asset types (texture, mesh, material, shader)
    - i'd like to be able to create a mesh asset, and assign it to an entity
    - I'd like to be able to create a Sprite asset, and assign it to an entity
- opening a saved scene that is the same scene as the current open one doesnt work
- put some text in the toolbar or something for the Current Open Scene Name
- implement "scoped asset locks" on an arbitrary MAID/meAsset

TODO: get rid of portable-file-dialogs. It pulls in a bunch of stl stuff.

- refactor so instead of straight loading gltf, we "import" gltf and turn it into massets, then load(/compile) those
	- meAssetCreate would do the check for .gltf in the filename, and do it there

- TODO: look into https://github.com/microsoft/Xbox-ATG-Samples/blob/main/XDKSamples/System/MemoryBanks/MemoryBank.h#L49
        for use with the asset system. The ideas i've had about multiple virtual addresses mapped to the same physical addr are implemented there

- renderer:
	- ~~fix plane mesh generation~~
	- ~~render a texture on the plane~~
	- ~~put a shader on the plane~~
	- 2d Sprites
    - 3d lit geo
    - shadows
    - dbg render modes


Scene architecture???
    - mmm ecs....
    - "scene graph"

=== Frame Architecture ===

	- Create a "Frame" object that stores all data that is scoped per-frame
		- there'll also be a "frame" arena.
		- Set up an asset so the game & engine is forbidden to use the scene allocator *during* a frame
		- This means all loading, and any operations that need to persist stuff across frames needs to happen at the end/beginning of the frame
	- FrameSimInput structure that holds all inputs used for the simulation of 1 tick
		- recorded
	- FrameSimOutput structure that holds the "state" of a frame that has been ticked
		- this structure, and the "frame arena" should hold all per-frame data.
		- for savestates, this is all we'd need to save. The engine should be able to re-construct the rest (loaded assets)
	- FrameSimOutput is the input to a Render frame, and the renderer should be able to arbitrarily render any FrameSimOutput
	- 

============================

### General Roadmap
- render 2d squares and have em move around
- flesh out custom serialization format, implement for all current assets, like meshes, shaders, textures, and have them load through that data
    - i.e. a meScene asset on disk refers to a collection of "serialized entities" which contain materials, meshes, transforms
    - for this, need to be able to serialize a reference to another asset. This is equivalent to axe's .type system having a sno in it
    - after this, we will have the foundation to build a proper "asset compiler", so game just reads in compiled stuff
        - stretch idea: have compilation be a separate process (literally) that the game client asks for compiled stuff, I.E. bill + compilation server
- scene graph
- NVRHI renderer backend & slang?

- engine-wide savestates
	- user can only "request" a save, that gets serviced at a fixed point after the frame (can't save in middle of frame)
	- stuff that needs to be saved/delt with:
		- mem (arenas and such)
		- engine ctx/systems 
		- resource pools

		- all of those ^ have their data in the main engine arenas

		- renderer
			- will be stateless. Each frames input structure is traversed, and resources are loaded lazily if they aren't already loaded.
				it is up to each resource system to implement an lru cache and define purge behavior
		- OS stuff
            - files:
                - virtualize
                    - another level of indirection in the os layer. "file handles" that the engine uses aren't actual file handles (virtual).
                    from savestate -> current point in time, all those opened file handles are known, and are wiped out when we restore a savestate
                    then when we read from that virtual file handle again (or maybe a lazy check on all file operations), we re-open the file from the cached path
            - threads:
                - Disallow one-off threads. Only threadpool threads allowed. With this rule, don't need to worry about threads in savestates at all
            - mappings (shared mem, etc)
                - same as files maybe? Have a engine structure we use instead of the raw os primitives (which i would've done anyway) which always does a lazy "am i valid" check on all operations and can reinitialize from some cached metadata about the mapping
            - graphics: all handled by "stateless" renderer, rendering-related memory should not be included in regular savestate stuff

				
					


savestate-related thought experiment:
*everything* in the engine is serializable, so we can write the entire state of the engine to disk and load it back up again
Relative data structures:
	- use {((s64)&this) - (s64)this } pointer trick
	- https://jorenjoestar.github.io/post/serialization_for_games/
	- taking this concept further: 
		instead of an arbitrary offset in all of the program's memory, a relative ptr/data structure can be
		relative to itself, but ALSO relative to some other pre-defined "allocator root" or something
		So like, you could have a RelPtr<SomeType>(myPointer) which by default is relative to itself, see trick above
		But one could specialize RelPtr<SomeType> if we know SomeType should always be allocated from a dedicated pool,
		and in that case the "offset" would be relative to the start of that pool.
    - simpler idea: for development engine configs (non-shipping), just disable ALSR
        then, need a way to "contain" all engine state = system allocations (base arenas) + os state (open handles?)



- game/engine hot reloading
- general purpose allocators
	- string allocator
	- tcmalloc or rpmalloc as "default" allocator
- Entity/Object model
	- mostly ecs, but
		- entities are objects with a transform, a list of entity components, and a list of entity systems that can act on those components
			components themselves cannot interact with other components
			systems cannot interact with other systems
			there's also world systems that can act on all components in a scene 
- Implement Relational mappers
	- OneToMany, OneToOne (normal stdmap), ManyToOne, ManyToMany
- render basic example scene
    - Dead simple blinnphong. Not trying to flesh anything out yet. Future - lightmapping, GDR, meshlets & mesh shaders
- code, asset, shader hot reloading
- gamepad input 
- using blender as the editor - exporting to my format
- UI (Clay?)
- physics
- audio

## R&R

Idea is to make this an engine that supports, as a first class citizen,
a full record-and-replay (r&r) feature. This means complete determinism throughout the engine
and all engine features will be built with this feature in mind. 

*Assets*: 
seperated into read-only and writable to support r&r. Readonly assets are "deterministic". Writable assets would need special functionality to be properly rolled back and re-written to during resimulation.
Could also use a heavy-handed approach where writing to assets is fully disallowed during regular application loops. Writes to assets would need special consideration from the recording/replaying systems
Further thought:
for simplicity, don't distinguish rw/ro. All disk reads are copied into the event log, and during replay we just give you a pointer to the mapped file content in that event log. all "file" operations/storage is owned by the engine, so it could do this abstraction. 
For perf - weird idea: engine creates 2 mappings on the file - one for the engine/user to start using which is COW. Another for the engine to copy into the event log which is readonly. Once engine copies it, readonly mapping goes away, and we atomically replace the COW mapped pointer with a regular writable pointer to the file, and (syncro) copy the content of the COW mapped pointer into the actual file. That way, the engine can do the event log file copy "for free" without blocking the rest of the engine.

*Rendering*: 
Takes in a readonly gamestate and passes it to the user's rendering system.
A required feature of a renderer in this engine is to be *stateless*. 
Meaning it can take in any arbitrary gamestate and render it. Cannot rely on previous frames and any
initialization of things like gpu memory and whatnot must be done lazily, and with proper consideration
to support, for instance, rendering frame X, then rendering frame X+20, then frame X-20.
- "closed form" particle system: Particles are "stateless". can all be computed by a random seed & time value
	- I.E. instead of pos + velocity = ^pos      pos = start_pos + abs(sin(seed + time)) or similar. 
		doesn't have to be "simple", these can become complex physics equations that respect gravity and collisions and all that
		point is the particle state is recalculated *from scratch* every frame
		this way, particles don't need to be recorded at all. Braid does this.

*Game Simulation*:
A purposely single-threaded simulation to ensure determinism?
Potentially could allow users to do whatever they want with threads, but at their own risk of "moment-to-moment debugging" desyncs in replays from any race conditions. though this might be good, since if multiple runs of the same replay desync we know there's a race condition


Stretch goal: "reversible" physics/simulation?
- idea: imagine a simple gear spinning clockwise. This "physics simulation" is very simple, just rotating the object by some amount in a certain direction
    this is a "reversible" simulation - in that it is very easy to imagine just inverting the direction of the rotation to simulate backwards.
    Could this concept be extrapolated to more complex senarios? Large parts of a given game/physics/etc simulation may be deterministic. For those parts,
    making it "reversible" would mean creating equivalent logic to simulate backward. Since many simulations end up inevitably doing "destructive" operations,
    that is an operation that fully overwrites some state that cannot be derived from future states, non-deterministic events would need to be recorded during forward simulation, and used while doing backward simulation. 
    I think doing this "reversible" thing is probably way more trouble than it's worth. Lots of extra implementation/maintenence
    Doing snapshots/deltas with compression in the replay log is probably way more robust.


### Extra stuff i want to look into
- procedural animation combined with flocking
    - imagining a swarm of small "feather" meshes all combining like a pointcloud to create a procedurally animated huge hawk boss
    - https://youtu.be/hCQCP-5g5bo?si=Eufg0dVjp3XeLQaw&t=716
- https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/
- meshlet compression (research said up to 60% savings from normal mesh storage holy crap)
https://github.com/google/draco - mesh compression
https://github.com/KhronosGroup/KTX-Software - texture compression
- implicit surfaces/metaballs
	- combined with modern amplification/mesh shaders, this could be really interesting for rendering?
		- ^ would basically be marching cubes on the GPU

