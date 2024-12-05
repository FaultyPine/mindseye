# mindseye


## Building

Must be on windows.
run `generate.bat`
then `build.bat`



## Details

- sound: [soloud](https://solhsa.com/soloud/)
- physics: [jolt](https://github.com/jrouwe/JoltPhysics)
- using blender as the editor - exporting to custom format


## TODO/goals


- bring over good bits of tiny engine
    - math, mem, containers, logging, defines
- 

### Stuff i want to look into
- https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/
- meshlet compression (research said up to 60% savings from normal mesh storage holy crap)


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
Potentially could allow users to do whatever they want with threads, but at their own risk of desyncs in replays.
