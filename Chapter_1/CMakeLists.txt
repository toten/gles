add_executable( Chapter_1 egl.cpp )
target_link_libraries( Chapter_1 ${OPENGLES3_LIBRARY} ${EGL_LIBRARY} ${X11_LIBRARIES} )