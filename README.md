# mindseye


## Building

Must be on windows.
run `build.bat`

### Current focus
// BOOKMARK: 
// - ~~refactor DynArray to be a real struct, so I can mereflect it~~ done
// ~~get a list of types for templated fields and write that list into the generated header as a .templatedTypes = {...} ~~
// ~~autogenerate a file with all the source includes~~
// - add capability for mereflected stuff to specify a serializer function in the macro.
// - implement DynArray serializers with above ^
// - move other types to use this ^ I.E. stringview, string, mespan?


- render 2d squares and have em move around
- flesh out custom serialization format, implement for all current assets, like meshes, shaders, textures, and have them load through that data
    - i.e. a meScene asset on disk refers to a collection of "serialized entities" which contain materials, meshes, transforms
    - for this, need to be able to serialize a reference to another asset. This is equivalent to axe's .type system having a sno in it
        - for me, this is when a serialized struct has an MAID member
		- difference between MAID and Eye is MAID is an asset identifier, whereas Eye is a runtime-only concept
			- TODO: replace current "Eye" usage with MAID somehow
				- i think the "core" mistake was using Eye in meResourcePool
					replacing that with MAID i think is the right call
        - i'd like to add something to the mereflect macro where you can add a function for serialize/deserialize from the macro itself
		- TODO: to be able to have a serializable list of entities in the scene, we need to serialize DynArray, which feels weird
			- might be time for that dynarray refactor i've wanted to do - turning it into a more official type rather than implicitly working on a pointer


    - after this, we will have the foundation to build a proper "asset compiler", so game just reads in compiled stuff
        - stretch idea: have compilation be a separate process (literally) that the game client asks for compiled stuff, I.E. bill + compilation server

- game/engine hot reloading
- have engine systems register themselves through a static event the engine core dispatches. each engine system needs to define the other engine systems it will touch (rw/ro), and has a bitset for those. Then, all systems aren't allowed to use GetEngineCtx, they can only access the systems they explicitly define in their initialization. Reflection not required for this, but it would make it way cleaner
	 this needs more thought, because an actual "static" event has undefined initialization order.
- general purpose allocators
	- string allocator
	- tcmalloc or rpmalloc as "default" allocator

### General Roadmap
- asset system
	- ~~job system to support above loading~~
- ~~load basic example scene (USD?)~~
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