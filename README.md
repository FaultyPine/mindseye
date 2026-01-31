# mindseye


## Building

Must be on windows.
run `build.bat`

### Current focus
- implement DynArray serializers
- me_asset_indexer
	- maps MAID to filesystem paths
	- asset loader needs to be able to create "blank/default" assets of a given type. (default-construct + write to disk) so we can insert the MAID into the definition file
	- everything sorts out once the MAID is in the masset
	- future: includes asset dependencies, asset metadata (timestamp, name, type, etc), indexes for all these things for fast lookup
 - implement MAID serialization - allow assets to reference other assets in a serialization-friendly way
	- I think what i'm settling on, or what i've just thought of to be the best way
		is to group together an Eye and MAID structure in 1 structure, and have that in the EntityData or whatever
		that way, we can serialize out those structures with the MAID portion which can map to the asset on disk
		but can also be used for runtime by loading whatever the MAID points to into the Eye
	- I keep feeling uneasy about how i'm managing paths on disk to assets
		Idea - and i should verify if Axe does this too, and maybe also how Esoterica does it
		is to have a "data directory" where all the asset files live. Then scan that on startup and cache a mapping of path <-> asset ID. Each asset file (I.E. .scn) should have a guid in them
- refactor so instead of straight loading gltf, we "import" gltf and turn it into massets, then load(/compile) those
	- meAssetCreate would do the check for .gltf in the filename, and do it there

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
- PCH
- render 2d squares and have em move around
- flesh out custom serialization format, implement for all current assets, like meshes, shaders, textures, and have them load through that data
    - i.e. a meScene asset on disk refers to a collection of "serialized entities" which contain materials, meshes, transforms
    - for this, need to be able to serialize a reference to another asset. This is equivalent to axe's .type system having a sno in it
    - after this, we will have the foundation to build a proper "asset compiler", so game just reads in compiled stuff
        - stretch idea: have compilation be a separate process (literally) that the game client asks for compiled stuff, I.E. bill + compilation server


- engine-wide savestates
	- user can only "request" a save, that gets serviced at a fixed point after the frame (can't save in middle of frame)
	- stuff that needs to be saved/delt with:
		- mem (arenas and such)
		- engine ctx/systems 
		- resource pools
		- renderer
			- first impl could be to clear everything out of bgfx and let "lazy/on-demand loading" handle it
		- OS stuff
			- how do i handle file handles and that kinda thing...?
			- virtualize all these funcs with hooks?
			- or just keep a mapping...

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
- input (gamepad & kbm)
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
A purposely single-threaded simulation to ensure determinism. 
Potentially could allow users to do whatever they want with threads, but at their own risk of "moment-to-moment debugging" desyncs in replays from any race conditions. though this might be good, since if multiple runs of the same replay desync we know there's a race condition


Stretch goal: "reversible" physics/simulation?
- idea: imagine a simple gear spinning clockwise. This "physics simulation" is very simple, just rotating the object by some amount in a certain direction
    this is a "reversible" simulation - in that it is very easy to imagine just inverting the direction of the rotation to simulate backwards.
    Could this concept be extrapolated to more complex senarios? Large parts of a given game/physics/etc simulation may be deterministic. For those parts,
    making it "reversible" would mean creating equivalent logic to simulate backward. Since many simulations end up inevitably doing "destructive" operations,
    that is an operation that fully overwrites some state that cannot be derived from future states, non-deterministic events would need to be recorded during forward simulation, and used while doing backward simulation. 



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

### Game ideas
walking simulator-esc. No combat/levels/objectives/etc. Just telling a story
throw in some non euclidean portal nonsense
- View planes on all sides of camera slice objects persistently. Imagine a cube on the ground - looking to the side so the cube is half off the screen, then looking back at the full cube, you'd see half of the cube, as if it was squished against your view
	Could use the view planes to push objects, cut things away.... "heavy" objects might prevent you from looking away from them
- coop warioware
- Tower defense, 3rd person, 3d. Interesting mechanic/experience is enemies do not follow predefined paths
	like in every other tower defense game. They spawn and pathfind to their destination (what you're defending)
	great design space from that: towers are no longer limited to "dealing damage" as their main purpose
	towers can redirect enemies! imagine a large open space enemies move towards your "crystal". You can place
	towers that "funnel" enemies in, making your own choke points for damage-dealing towers to attack
	Certain towres can even "filter" enemies, so strong ones end up somewhere and weaker ones end up somehwere else
	Portal springboard tower that flings enemies across the map. Imagine Factorio-esc rube goldberg machines that facilitate
	your towers. Lots of emergent gameplay from some simple primitive towers.