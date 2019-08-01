
const char* angular_normal_maps_vs_source = R"(
#version 310 es

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 tangent;
layout(location = 2) in vec3 bitangent;

out vec2 out_uv;
out vec3 out_tangent;
out vec3 out_bitangent;

uniform mat4 model_matrix;

void main()
{
    gl_Position = vec4(uv * 2.0f - vec2(1.0f), 0.0f, 1.0f);

    out_uv = uv;
    out_tangent = (model_matrix * vec4(tangent, 0.0f)).xyz;
    out_bitangent = (model_matrix * vec4(bitangent, 0.0f)).xyz;
}
)";