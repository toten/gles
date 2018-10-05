# gles

build egl:
gcc -o egl egl.cpp -I ./External/Include/ -L /usr/lib/nvidia-384 -L /usr/lib/x86_64-linux-gnu -lEGL -lGLESv2 -lX11
