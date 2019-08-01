
const char* normal_maps_light_vs_source = R"(
#version 310 es

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uv;

out vec2 out_uv;

uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 projection_matrix;

void main()
{
    mat4 mvp = projection_matrix * view_matrix * model_matrix;
    gl_Position = mvp * vec4(pos, 1.0f);

    out_uv = uv;
}
)";