# mindseye


## Building

Must be on windows.
then `build.bat`



## Details

- sound: [soloud](https://solhsa.com/soloud/)
- physics: [jolt](https://github.com/jrouwe/JoltPhysics)
- using blender as the editor - exporting to custom format


## TODO/goals


- bring over good bits of tiny engine
    - ~~math~~, ~~mem~~, ~~containers~~, ~~logging~~, ~~defines~~
- ~~refactor containers to use a general allocator scheme.~~
- open an os window
- begin vulkan renderer
    - honestly..... should i just use bgfx or something? As fun as graphics prog is, The point of this engine isn't to do a whole bunch of graphics programming
    and if im gonna do vulkan most of my time with this engine will be spent there inevitably....
    - maybe design a "frontend" and have bgfx as my backend which i could swap out for a custom thing if i feel like diving into that

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
- https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/
- meshlet compression (research said up to 60% savings from normal mesh storage holy crap)


### Game
A short&sweet experience centered around the concepts in Courage To Create by Rollo May
walking simulator-esc. No combat/levels/objectives/etc. Just telling a story & discussing creativity
throw in some non euclidean portal nonsense
