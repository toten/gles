add_executable( Chapter_2 level.cpp )
target_link_libraries( Chapter_2 ${OPENGLES3_LIBRARY} ${EGL_LIBRARY} ${X11_LIBRARIES} )
SET(CMAKE_CXX_FLAGS "-std=c++0x")
