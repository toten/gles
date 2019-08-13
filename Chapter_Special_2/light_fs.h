const char* light_fs_source = R"(
#version 310 es

precision highp float;

in vec2 out_uv;

out vec4 frag_color;

const vec3 directional_light = normalize(vec3(1.0f, 1.0f, 1.0f));

void main()
{
    float z = out_uv.y * 2.0f - 1.0f;
    float l = pow(1.0f - z*z, 0.5f);

    const float pi_2 = atan(0.0f, -1.0f) * 2.0f;
    float angle = (out_uv.x - 0.5f) * pi_2;
    float x = cos(angle) * l;
    float y = sin(angle) * l;

    frag_color = vec4(vec3(dot(directional_light, vec3(x, y, z))), 1.0f);
}
)";