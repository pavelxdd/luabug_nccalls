# luabug_nccalls

A small C program which reproduces a bug in Lua when a negative number is assigned to `L->nCcalls` (unsigned short), so it becomes `>65000`.

To build without a patch:

````bash
cmake . -Bbuild
cmake --build build
./build/luabug_nccalls
````

To build with a patch:
````bash
cmake . -Bbuild -DLUA_PATCH=1
cmake --build build
./build/luabug_nccalls
````
