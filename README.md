# gekengine
Game Excelleration Kit

The Game Excelleration Kit, GEK, is a hobby project that I have been using to explore the latest C++ features, and the lastest in 3D graphics technology.  The engine itself is setup so that I can easily add new techniques without having to recompile the engine itself.  At it's core, the GEK engine uses plugins so that new features can easily be added without needing to recompile the base engine.  This also allows for switchable rendering backends so that multiple operating systems can be supported.  Currently, only Direct3D 11 is implemented, but additional support for Direct3D 12, OpenGL, and Vulkan, is in the works.

# Compiling

GEK can easily be compiled using CMake.  The GIT repository is setup to include all it's major dependencies as submodules.  The only additional dependency that is required is a DirectX/Windows SDK containing the Direct3D 11 API.  Checking out recursively will also check out the external submodules, and the CMake scripts are setup to generate the required projects and link against them as well.  So far only MSVC has been tested and verified, but additional support for alternate Windows compilers, as well as Linux and Mac, will be implemented and verified.

# License

GEK Engine is released under the MIT licence.
