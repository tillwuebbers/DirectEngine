# DirectEngine
Toy game engine for DirectX 12. Made for my own learning and experimentation.

## Features
- Custom memory allocation scheme using the Memory Arena pattern
- glTF model loading and rendering
- PBR materials
- Skeletal animation
- Portal rendering

##
![image](https://github.com/tillwuebbers/DirectEngine/assets/43892883/72e8e459-1aab-4178-b994-8ac86bda44f6)
![image](https://github.com/tillwuebbers/DirectEngine/assets/43892883/8217586a-2a7c-4ece-a5c1-edf5e686324c)

## Build Notes
- Visual Studio 2019+
- Initialize Submodules
- Download XAudio 2.9 from here: https://www.nuget.org/packages/Microsoft.XAudio2.Redist/1.2.9
- Copy the contents of the native/build folder to the xaudio2 directory
- Go into the openxr-sdk dir, create build folder, cmake ..
- Open the created solution and build the loader
- Now you can run and build the main CMake project from within VS
