

# Frame Architecture

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


# User app architecture
- "immediate mode game engine"
```
struct UserGameState
{
    EntityHandle player;
    EntityHandle enemies[50];
    DynArray<idk> someDynamicSizedData;
    // can cache things that need to last more than 1 frame here
};

update ()
{
    // by making sure the engine owns persistent state, we can snapshot it and rewind it easily
    UserGameState& gs = Engine::GetPersistent<UserGameState>();
    DynArrayCreateIfInvalid(gs.someDynamicSizedData, Engine::PersistentAllocator());

    // lazy - on first update, gs.player is "invalid" so it is created.
    // on subsequent updates, playerEnt holds the entity state from last frame
    // playerEnt actual entity data owned by the engine
    // user "owns" the handle to that, but in reality even that memory is engine-side (Engine::GetPersistent<SomeType> allocates spaces for user types with 'infinite' lifetime) 
    if (ScopedEntity playerEnt = PushEntity("Player", gs.player))
    {
        // modifications to entity data will be reconciled back into the engine
        // TODO: should it be reconciled:
            - after playerEnt goes out of scope
                simple. Means the user can react to their own state changes immediately
            - or after the user tick function?
                Potentially more complicated, but potentially more performance. Batched updates & parallelism becomes easy. Con: User doesn't have a great way to react immediately to their own state modifications. Maybe the "solution" there is that the user should just cache some persistent state to indicate whatever they need
        playerEnt.transform.position.y += PlayerGetBootHeight();

        // Idea: before compile time, we get asset index and code-gen a header of all the assets
        // so code can directly reference assets without fragile strings. 
        // If user renames asset in data, they also would need to rename it in code. Deleted asset = code doesn't compile.
        // data assets also means we can discover asset deps, so we don't run into the immediate mode problem of not knowing dependencies/child data
        ShaderID glowy_superpower_shader = Assets::glowy_superpower_shader;
        MaterialID playerMatID = IsPlayerSuperpowered(playerEnt) ? 
                DeclMaterial(glowy_superpower_shader, { .color = RGB(100, 50, 50) }) : 
                Assets::player_default_material;

        { 
            ScopedMaterial playerMat = PushMaterial(playerMatID);
            playerMat.dirtiness = IsPlayerDirty() ? 0.5 : 0.0;
            PushRenderable(playerEnt); // implicitly (?? idk if good idea) gets material from "top of the material stack"
        }
    }


}
```


# Work graph for all engine systems
sorta like how naughty dog's engine is broken up pretty fine into small jobs on fibers to utilize cpu as much as possible
what if we took the ECS idea of defining in data the dependencies of a "system" (job, in this case) and submitting to a graph resolver to schedule parallel work automatically
So like - engine is really comprised of loads of smallish jobs, all of which need to data-define what data they touch - globals and also local params (automatically discovered?)
Potential CON: more annoying to debug, because it's not just stepping through regular functions. Would ideally have a way to make sure "step into" on a job spawn defaults to putting a breakpoint in the actual job funcptr being passed in. I don't think there's any debuggers out there that can do this.

Might be best to first implement better thread context structures
- move threadlocal scratch arenas into this threadlocal omega structure
- thread should be able to trivially get it's "global index"
- Instead of the paradigm: "here's a task, put it on a thread", and if you want multiple tasks that divide up some work, the caller thread has to chunk that up and pass context into a task function.
    - better to do it more GPU-esc. Like, "here's a function, run it on N threads". And all N threads get a context structure with their "group index" and the "group threadcount (N)". Then like gpu programming, it's just about having the task itself mask out the work it shouldn't care about. Use barriers to syncronize and threadgroup indexing like i wrote above for "distributing" the work.
        - Really great post about this concept: https://www.dgtlgrove.com/p/multi-core-by-default

Example 1:
```
engine needs to load a scene. pushes 'scene load job', dependencies are
    - asset dependency table, (RO)
    - filesystem (RO)
that job discovers dependencies and reads scene data from disk (these aren't interdependent, could happen in parallel, graph resolver would do this)
after those two, it pushes a child job 'load scene deps' and another child job 'finalize scene'
'load scene deps' - uses asset dependencies and pushes child jobs to load each dep (which themselves load their own deps, etc)
    - filesystem (RO)
'finalize scene' - does any internal touchups, and adds the asset to the asset cache for other engine systems to read. Also kicks a scene thumbnail detached child job
    - asset cache (RW)

scene might ref an entity which refs an 'appearance' which refs a texture
'texture load job' - (optional)decompresses and loads texture data into memory and uploads to gpu
    - filesystem (RO)
    - asset cache (RW)
    - (not taken into account, just for clarity) gpu
```
Even actual engine frame sims/render could take advantage.
Example 2
```
all "systems" must data-declare what data they touch rw/ro
physics tick: RW entities transform, bounds, physics-related data (components?) 
render tick, 2 pass. gather pass: RO entities, copying tranform, bounds, appearance into internal structures (so cannot run parallel with stuff like physics tick ofc) | render pass: no external dependencies, operates on gathered data and outputs draw list or something
AI tick: could be 2 passes - gather pass: RO entities transform + ai-related data, gathers into AI-local caches/structures | think pass: generate decisions from gathered data (internally ro/rw, but doesn't need to touch external systems, so this pass could run in parallel with other systems easily) | act pass: RW entities transform/anims/ai-related data

you can see the idea: the physics render pass can run in parallel with AI think pass because their dependencies don't intersect. This sort of resolution would happen automatically.
There would also be validation on every access to "system-external" data to make sure we assert if you access something you didn't declare upfront.
EnTT flow graph + custom storage hook is one way to do that part that we do in Diablo.


```
Audit codebase so EngineContext is the ONLY global singleton. All global data should live in there, and nobody should raw-access the global itself (use GetEngineCtx() instead)
then i can add a validation check inside GetEngineCtx to assert if we try to access any global state in a job.
Would also need to think about how to handle this case: a job pushes a child job and passes some "local" state in which is really state owned by global EngineContext - hence bypassing the validation. Having trouble thinking of a way to prevent this at the api/validation level, without some sort of insane data flow analyzer on ast. Maybe just lots of docs and warnings is the best we can do? 

TODO: think of a good MVP system to convert to using this. 

#### Mock usage/api brainstorming
```

enum meAccessMode
{
    meReadOnly, meReadWrite
};

struct meJobAccess
{
    u64 typeToAccessHash;
    meAccessMode mode;
};

typedef void (*meJobRunner)(meJobContext*);

struct meJobDesc
{
    StringView name;
    meSpanTyped<meJobAccess> accesses;
    meJobRunner runner;
};

struct meJobCtx
{
    const meJobDesc* desc = nullptr;
    u32 graphJobId = U32_INVALID_ID;
    u32 childDepth = 0;
private:
    EngineContext* ctx = nullptr;
};

MEREFLECT(CompileRun)
void JobParserMain(meCompileRunContext* ctx)
{
    if (ctx->stage == meCompileRunStage_VisitDecl)
    {
        if (ctx->reflectOp != STRING_LIT("JobDecl"))
            return;

        // Assert cursor is a free function.
        // Read function name from ctx->cursor.
        // Parse ctx->macroContent:
        //    "JobDecl, RW meMaterialPool, RO meTexturePool"
        // Emit JD_FunctionName with static meJobAccess[].
    }
    else if (ctx->stage == meCompileRunStage_Finalize)
    {
        // Emit meRegisterGeneratedJobs().
    }
}

#define JOBDECL(...) MEREFLECT(JobDecl, __VA_ARGS__)

#define RO
#define RW
#define ACQUIRE(ctx, Type, Mode) \
    meJobAcquire<Type>(ctx, HashStringComptime(#Type), mode, #Type)

// the way ACQUIRE would work is it all goes through the meJobCtx
// you need a jobCtx to ACQUIRE something, so even if this job function calls a million nested functions deep, it needs to pass this ctx all the way down.
// When you SpawnChildJob(ctx, ...) it would increment some sort of counter/stack idx
// so each "level" of a job (parent/child/child-of-child/etc) has it's own dependencies

// - declare the dependencies in the signature of the function somewhere. Maybe in the JOBDECL(Dep1, Dep2, Dep3, etc)? Or just EnTT, parse it from the parameters. I.E. DoSomeWork(meJobCtx& ctx, Dep1 d, Dep2 f, Dep3 g) -> [d,f,g]. This feels most robust.

void meJobValidateAccess(meJobCtx& ctx, u64 typeHash, meAccessMode requested, const char* debugName)
{
    for (const meJobAccess& access : ctx.desc->accesses)
    {
        if (access.typeHash != typeHash)
            continue;

        if (access.mode == meReadWrite || requested == meReadOnly)
            return;
    }

    LOG_FATAL("Job %.*s accessed undeclared resource %s",
        STRING_VAARGS(ctx.desc->name), debugName);
    ME_ASSERT(false);
}

template <typename T>
T& meJobAcquire(meJobCtx& ctx, u64 typeHash, meAccessMode mode, const char* debugName)
{
    meJobValidateAccess(ctx, typeHash, mode, debugName);
    // this is the meat&potatoes. resolves a type to a hash (comptime) and looks up the storage for that type in the engine. This is basically bar-for-bar what ECS/EnTT does
    return meJobResolveStorage<T>(ctx.engine); 
}

meJobHandle meSpawnJob(meJobCtx& parentCtx, const meJobDesc& desc);
void meWaitForJob(meJobCtx& ctx, meJobHandle handle);

JOBDECL(RW meMaterialPool, RO meTexturePool)
void DoSomeWork(meJobCtx& ctx)
{
    // auto ctx = GetEngineCtx(); // this would assert!

    // validates that this job is allowed to access this
    auto& materialPool = ACQUIRE(ctx, meMaterialPool, meReadWrite);
    DoWhateverWork(materialPool);
    auto& texturePool = ACQUIRE(ctx, meTexturePool, meReadOnly);
    DoWhateverWork(texturePool);
    auto childJobHandle = meSpawnJob(ctx, GetJob(SomeOtherWork));
    // .....
    // could maybe WaitForChildJobHandles(&childJobHandle, 1) or do nothing to let it be "detached". maybe SpawnChildJob is [[nodiscard]] and the destructor of a childJobHandle indicates the job is out of scope, and if it wasn't explicitly waited on, we KNOW that child job can be marked as "detached".
}


void EngineFrame(EngineContext* engine)
{
    meJobGraph graph = {};
    meJobGraphBegin(graph, engine);

    meJobGraphAddRoot(graph, GetJob(mePhysicsTick));
    meJobGraphAddRoot(graph, GetJob(RenderGather));
    meJobGraphAddRoot(graph, GetJob(AIThink));
    meJobGraphAddRoot(graph, GetJob(AIPublish));

    meJobGraphRunBlocking(graph);
}


// generatedtypes/me_jobs.generated.cpp

static meJobAccess DoSomeWorkAccesses[] =
{
    { HashStringComptime("meMaterialPool"), meReadWrite },
    { HashStringComptime("meTexturePool"), meReadOnly },
};

meJobDesc JD_DoSomeWork =
{
    .name = STRING_LIT("DoSomeWork"),
    .accesses = meSpanTyped<meJobAccess>(DoSomeWorkAccesses),
    .runner = DoSomeWork,
};

static meJobAccess SomeOtherWorkAccesses[] =
{
    { HashStringComptime("meMeshPool"), meReadOnly },
    { HashStringComptime("meTexturePool"), meReadWrite },
};

meJobDesc JD_SomeOtherWork =
{
    .name = STRING_LIT("SomeOtherWork"),
    .accesses = meSpanTyped<meJobAccess>(SomeOtherWorkAccesses),
    .runner = SomeOtherWork,
};

struct meJobRegistry
{
    meMap<meJobRunner, meJobDesc> jobs;

    void Register(const meJobDesc& desc)
    {
        jobs[desc.runner] = desc;
    }
    // so we can spawn jobs/reference jobs soley by it's function ptr
    const meJobDesc& GetJob(meJobRunner runner)
    {
        return jobs.at(runner);
    }
};

void meRegisterGeneratedJobs(meJobRegistry& registry)
{
    registry.Register(JD_DoSomeWork);
    registry.Register(JD_SomeOtherWork);
}

```
