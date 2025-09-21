# mindseye


## Building

Must be on windows.
then `build.bat`


## Details

- sound: [soloud](https://solhsa.com/soloud/)
- physics: [jolt](https://github.com/jrouwe/JoltPhysics)
- using blender as the editor - exporting to custom format

- a bunch of this engine is yoinked from my previous engine project, tiny engine. Despite a lot of it being the same, i want to keep tiny engine as it is right now. It's a great snapshot in time of my first game programming efforts, and starting fresh feels right, even if a lot of it is similar.

## TODO

- going to sprint towards MVP of having a scene (USD? gltf?) render. Then will do (possibly many) cleanup/hardening passes after

- REALLY IMPORTANT TODO:
    - instead of passing meAllocator* around, I should pass an allocator handle
	- which is internally u32 and dereferences to the allocator itself.

### Current focus
- ~~RWlock~~
- REFLECTION (C lexer? Metadesk? Clang plugin?)
	- Use same technique as Esoterica engine. Clang.
	- first weird roadblock with clang parsing. The compile commands json is invalid when there's just one file. Made a bug on clang github
	- second thing I need to solve: I can't use the mindseye compilation database because the reflector needs to run before mindseye is compiled, but if mindseye isn't compiled, i don't have the compilation database. Chicken & egg.
		- Maybe just use the reflector's compilation database? since im already using mindseye stuff there, the args will work. Mmmm <- that's probably a bad idea.
		- Esoterica doesnt have this issue because he uses the sln, which is always there.
	- can't use compilation database, sadly, for above reason. Instead, i'm just having the build system spit out it's mindseye compilation command, and parsing that manually.
- Scene loading. 
	- map a MAID to a filesystem path, or an arbitrary "actual" asset identifier that a loading system can use to actually load the thing fr
	- Need a scene/asset description file. Going to use ini. To get data in/parse that description file, need reflection
- game/engine hot reloading
- have engine systems register themselves through a static event the engine core dispatches. each engine system needs to define the other engine systems it will touch (rw/ro), and has a bitset for those. Then, all systems aren't allowed to use GetEngineCtx, they can only access the systems they explicitly define in their initialization. Reflection not required for this, but it would make it way cleaner

### General Roadmap
- asset system
	- ~~job system to support above loading~~
- load basic example scene (USD?)
- render basic example scene
    - Dead simple blinnphong. Not trying to flesh anything out yet. Future - lightmapping, GDR, meshlets & mesh shaders
- code, asset, shader hot reloading
- input (gamepad & kbm)
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

*Core Engine*


Stretch goal: "reversible" physics/simulation?
- idea: imagine a simple gear spinning clockwise. This "physics simulation" is very simple, just rotating the object by some amount in a certain direction
    this is a "reversible" simulation - in that it is very easy to imagine just inverting the direction of the rotation to simulate backwards.
    Could this concept be extrapolated to more complex senarios? Large parts of a given game/physics/etc simulation may be deterministic. For those parts,
    making it "reversible" would mean creating equivalent logic to simulate backward. Since many simulations end up inevitably doing "destructive" operations,
    that is an operation that fully overwrites some state that cannot be derived from future states, non-deterministic events would need to be recorded during forward simulation, and used while doing backward simulation. 


Engine design to support above:




### Extra stuff i want to look into
- procedural animation combined with flocking
    - imagining a swarm of small "feather" meshes all combining like a pointcloud to create a procedurally animated huge hawk boss
    - https://youtu.be/hCQCP-5g5bo?si=Eufg0dVjp3XeLQaw&t=716
- https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/
- meshlet compression (research said up to 60% savings from normal mesh storage holy crap)
https://github.com/google/draco - mesh compression
https://github.com/KhronosGroup/KTX-Software - texture compression

### Game ideas
walking simulator-esc. No combat/levels/objectives/etc. Just telling a story
throw in some non euclidean portal nonsense
- View planes on all sides of camera slice objects persistently. Imagine a cube on the ground - looking to the side so the cube is half off the screen, then looking back at the full cube, you'd see half of the cube, as if it was squished against your view
	Could use the view planes to push objects, cut things away.... "heavy" objects might prevent you from looking away from them
- coop warioware

