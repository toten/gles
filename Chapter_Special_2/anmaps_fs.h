const char* angular_normal_maps_fs_source = R"(
#version 310 es

precision highp float;

in vec2 out_uv;
in vec3 out_tangent;
in vec3 out_bitangent;

out vec4 frag_color;

uniform sampler2D normal_map;

const float MAX_HEIGHT = 0.9f;

void main()
{
    vec3 normal_texel = texture(normal_map, out_uv).bgr;
    vec3 normal_vector = (normal_texel - vec3(0.5f)) * 2.0f;

    vec3 normal = cross(out_tangent, out_bitangent);

    mat3 tangent_space = mat3(out_tangent, out_bitangent, normal);
    vec3 normal_mapped = tangent_space * normal_vector;

    float z_dist = (normal_mapped.z + 1.0f) * 0.5f;
    float angle = 0.0f;

    const float pi_2 = atan(0.0f, -1.0f) * 2.0f;

    if (normal_mapped.z < MAX_HEIGHT && normal_mapped.z > -MAX_HEIGHT)
    {
        angle = atan(normal_mapped.y, normal_mapped.x);
        if (angle < 0.0f)
        {
            angle += pi_2;
        }
        angle /= pi_2;
    }

    frag_color = vec4(angle, z_dist, 0.0f, 1.0f);
}
)";