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
- Improve prefab update handling to better work with One File Per Actor. Prefabs will now only update Actors which need updating and won't touch those that don't need updating. Also they will no longer delete and recreate actors unless needed. 
- Fix crash when changing physics options on a prefab actor in editor
- Change prefab reloading logic. Reload prefab (default on loading the level) will not update any local instance changed actors unless they have changed in the prefab. Force reload prefab will force an update on all actors in the prefab and will lose your local changes to the instance. 
- Add tags to prefabs "Prefab" "NestedPrefab" and "PrefabChildActor" so they can be understood/read by other plugins. E.g for excluding selecting prefab children with mass selection tools. Will add on next update to the prefab. 
- Fix prefabs not hiding children when you press H to hide selected actors

Known Issues:
- Prefabs will be created inside your currently open folder in the content browser, that's not ideal
- Crash from prefabs created inside of level instance map editor
- Crash if you nest a prefab inside itself
- Meshes with custom primitive data don’t refresh properly in prefabs, the data is there but not visible in the mesh visuals/materials until you edit it
- Instanced static meshes cannot be used in prefabs
- Foliage through the foliage tool cannot be used inside of prefabs (but can be placed on prefabs that are placed in the level)
- Names of actors within prefabs don’t update unless you hit save on one of them then every one in the world updates


