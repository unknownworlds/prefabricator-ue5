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

Known Issues:
- Prefabs will be created inside your currently open folder in the content browser, that's not ideal
- Crash from prefabs created inside of level instance map editor
- Crash if you nest a prefab inside itself
- Meshes with custom primitive data don’t refresh properly in prefabs, the data is there but not visible in the mesh visuals/materials until you edit it
- Instanced static meshes cannot be used in prefabs
- Foliage through the foliage tool cannot be used inside of prefabs (but can be placed on prefabs that are placed in the level)
- Niagara systems in prefabs can cause crashes
