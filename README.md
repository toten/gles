# gles

Chapter-1 EGL

Chapter-Special Tangent remap

#build
mkdir build
cd build
cmake ..

If egl or gles library is not found, set CMAKE_LIBRARY_PATH specifically, e.g.:
export CMAKE_LIBRARY_PATH=/usr/lib/nvidia-384
or in cmake command line, e.g.:
cmake .. -DEGL_LIBRARY=/usr/lib/x86_64-linux-gnu/libEGL.so.1 -DOPENGLES3_LIBRARY=/usr/lib/x86_64-linux-gnu/libGLESv2.so.2

if X11 header or library is missing:
sudo apt install libx11-dev

