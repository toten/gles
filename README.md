# gles

Chapter-1 EGL

build egl:
gcc -o egl egl.cpp -I ./External/Include/ -L /usr/lib/nvidia-384 -L /usr/lib/x86_64-linux-gnu -lEGL -lGLESv2 -lX11

Chapter-Special

Chapter-2 Per-Vertex Compute
2.1 Generic Vertex Attribute Index and attribute location in VS:
   1) layout(location = N) in format attribute_name
      a) "in" is to specify per-vertex input
      b) layout(location = N) is to specify attribute location.
         It's optional, underline system will allocate attribute location if not specified in VS.
   2) Binding between generic index and attribute location:
      a) Before linking: glBindAttribLocation(program, generic_index, attribute_name)
      b) (After linking ?) generic_index = glGetAttribLocation(program, attribute_name)

   Generic Vertex Attribute is used in glVertexAttrib or glVertexAttribPointer.
   Note: An OpenGL ES 3.0 implementation supports GL_MAX_VERTEX_ATTRIBS four-component vector vertex attributes.
