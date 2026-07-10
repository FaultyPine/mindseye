#pragma once

#include "asset/me_asset.h"
#include "scene/me_entity.h"

/*
The purpose of this system is to create an opaque way to "get some data in the game"
Being able to serialize/talk about data in this way is helpful for multiple editor systems

Here's the pitch: 
In engine's like Godot/Unity/Unreal, they all have a "timeline editor" tool. It can take some field of an object (think: transform.position.x) and do operations on that value.
It can also *serialize* a reference to that value! I.E. if you author a timeline animation that lerps position.x from 0 to 1, the engine might serialize
EXAMPLE:
```
[Keyframe@1]
time=0.1
value=0.0
keyed=0

[Keyframe@2]
time=1.0
value = 1.0
keyed=0

[Keys@0]
field="transform.position.x"
```
With something like that, it could understand at runtime how to transform that description of the interpolation into actual memory addresses inside of a "game object" 
(reflection knows the byte offset of transform.position.x)
And in that way, the engine is able to "get some data in the game" opaquely based on some data that was authored at edit-time.
Artists/designers can now refer to arbitrary pieces of gamestate in all their tooling. This is extremely powerful!

I want to ensure this is a "first class citizen" flow in the engine. It should be easy to write tools that can refer to ANY piece of game data.
Imagine a UI designer that wants to "read the current Quest description string" to display it in a UI element. 
That would be easy with a robust system like this. They would author some UI that would point at a "game data endpoint" that resolves to the data they want.


RuntimeDataProvider is meant as the "source" of game data. I.E. a designer wants to serialize a reference to some asset and modify/read a field in that asset.
Another example of a potential RuntimeDataProvider is ECS components. These are runtime-only, and maybe identified by a hash of the component name (would need versioning if the struct name changes).
Given the structure of the engine currently, with the builtin clang reflection system it wouldn't be hard to make sure ECS components have associated reflection data


*/


enum RuntimeDataProvider
{
    ME_PROVIDER_ASSET,
};

struct RuntimeDataBinding
{
    RuntimeDataProvider providerType;
    union
    {
        MAID assetId;
    };
    
};
