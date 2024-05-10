Prefabricator for UE5
=====================

Create Prefabs in Unreal Engine 5

Forked from prefabricator-ue4 and updated to support world partition and actively maintained for UE5 features and support

Original repo website: https://prefabricator.dev

Changes:
- Fixed crash when used in world partition maps
- Added PA_ and PAC_ prefixes to prefab assets
- Added option to create a prefab with a specific pivot by creating it with an actor named "PivotActor"
- Fixed some uninitialized struct members that fail UE tests
- Fix some missing includes
- Fix issues with editor delegates (mostly in niagara systems) getting serialized in prefabs
- Fix prefabs not saving when you create the asset which can cause unable to load asset errors on the child actors placed in maps + source control
- Fix prefabs loading during map loading where we're not allowed to mark packages as dirty (led to unloaded ghost actors in world partition)

Known Issues:
- Prefabs will be created inside your currently open folder in the content browser, that's not ideal
- Crash from prefabs created inside of level instance map editor
- Crash if you nest a prefab inside itself
- Meshes with custom primitive data don’t refresh properly in prefabs, the data is there but not visible in the mesh visuals/materials until you edit it
- Instanced static meshes cannot be used in prefabs
- Foliage through the foliage tool cannot be used inside of prefabs (but can be placed on prefabs that are placed in the level)
- Names of actors within prefabs don’t update unless you hit save on one of them then every one in the world updates
- Updating one actor in a prefab will cause a recreate on all actors in that prefab

