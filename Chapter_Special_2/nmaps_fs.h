const char* normal_maps_fs_source = R"(
#version 310 es

precision mediump float;

in vec2 out_uv;
in vec3 out_tangent;
in vec3 out_bitangent;

out vec4 frag_color;

uniform sampler2D normal_map;

const vec3 directional_light = normalize(vec3(1.0f, 1.0f, 1.0f));

void main()
{
    vec3 normal_texel = texture(normal_map, out_uv).bgr;
    vec3 normal_vector = (normal_texel - vec3(0.5f)) * 2.0f;

    vec3 normal = cross(out_tangent, out_bitangent);

    mat3 tangent_space = mat3(out_tangent, out_bitangent, normal);
    vec3 normal_mapped = tangent_space * normal_vector;

    frag_color = vec4(vec3(dot(directional_light, normal_mapped)), 1.0f);
}
)";