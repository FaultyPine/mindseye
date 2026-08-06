# mindseye


## Building

Must be on windows.
run `build.bat`

### Current features
- clang reflection
- asset system (uses reflection data)
- compile-time arbitrary code execution

### Current focus

Finishing the asset system todo:
- refactor so instead of straight loading gltf, we "import" gltf and turn it into massets, then load(/compile) those
- binary serialization 
    - start on a "asset compilation" pipeline. 
	- meAssetCreate would do the check for .gltf in the filename, and do it there
- enforce some read-only-ness to asset templates
- implement "scoped asset locks" on an arbitrary MAID/meAsset
    - ref count meAssets, LRU cache?
- make a unique_ptr equivalent so a struct can clearly "own" some heap data.
    - maybe also a ptr wrapper that contains a builtin "free" callback - maybe meSpanOwning should do this

- Implement proper job system
    - should be lock free and use work stealing algo
    - should be able to build & submit a "graph" of work - the dependencies of which are resolved and the work is scheduled in parallel based on that graph
    - need "job queue types" - basically separate buckets of work that can be synchronized differently. I.E. background async work, asset compilation, frame tasks, multi-frame - async tasks, etc.
    - A job should be able to enqueue more jobs inside itself
    - a job should be able to enqueue child jobs from itself
    - A job should be able to yield or return out as "suspended". I.E. if that job needs to wait for some other child job to complete before it finishes.
    - Need to be able to wait on a job, or on a full "graph"/batch of submitted jobs


TODO: get rid of portable-file-dialogs. It pulls in a bunch of stl stuff.
============================

### Roadmap
- asset system
- log to file - rotate log files
- console command system - use CompileRun to codegen function<->console command bindings (also use for cmdline args handlers?)
- renderer
- scene graph
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
                - Disallow one-off threads. Only threadpool threads allowed. savestates are always "requested" and will be serviced when threadpool is done with all current work
            - mappings (shared mem, etc)
                - same as files maybe? Have a engine structure we use instead of the raw os primitives (which i would've done anyway) which always does a lazy "am i valid" check on all operations and can reinitialize from some cached metadata about the mapping
            - graphics: all handled by "stateless" renderer, rendering-related memory should not be included in regular savestate stuff
- NVRHI renderer backend & slang?

				
					


- game/engine hot reloading
    - engine should load <user dll>-X.dll
    - reload = compile <user dll>.dll -> rename to <user dll>-(X+1).dll -> tell engine -> engine increments hot-reload count (X) and loads the new <user dll>-X.dll
    - with below savestate memory trick, don't need to do any pointer fixup or anything
- savestates:
    - TODO: make sure meMap is allocator-aware. Rn it's using std which does system allocs
    - Pass a single GameMemory* to user app. Internally that's just a memory-mapped file at a fixed VA.
        - investigate how to create a huge "virtual" memory-mapped file. I.E. 50gb virtual mapping, but we only use some, and only that used first portion is committed
    - all allocators sub-allocate from that one
    - maybe *everything* in the engine is serializable, so we can write the entire state of the engine to disk and load it back up again
- enforce user dll can't use system allocator, enforce user dll can't use any static memory... all memory must be sourced from the engine.
    - back this with a static analyzer? runtime analyzer on the loaded user module?
-Relative data structures?
	- {((s64)&this) - (s64)this } pointer trick
	- https://jorenjoestar.github.io/post/serialization_for_games/


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
- physics (Box3d)
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

