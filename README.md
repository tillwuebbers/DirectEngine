# DirectEngine
Toy engine written in DirectX12

# Build
- Visual Studio 2019+
- Initialize Submodules
- Download XAudio 2.9 from here: https://www.nuget.org/packages/Microsoft.XAudio2.Redist/1.2.9
- Copy the contents of the native/build folder to the xaudio2 directory
- Go into the openxr-sdk dir, create build folder, cmake ..
- Open the created solution and build the loader
- Potentially have to copy the lib? (TODO)
- Go into reactphysics3d, create build folder, cmake ..
- Open reactphysics solution and run the install target
- Adapt path in cmake or something (why is this all so awful)
- Now you can run and build the main CMake project from within VS
