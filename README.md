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

