cmake_minimum_required( VERSION 2.6 )
project( ES3_Book )

include_directories( External/Include )

find_library( OPENGLES3_LIBRARY GLESv2 "OpenGL ES v3.0 library")
find_library( EGL_LIBRARY EGL "EGL 1.4 library" )
find_package( X11 )
 
SUBDIRS( Chapter_1
         Chapter_2)	
		