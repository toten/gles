const char* normal_maps_light_fs_source = R"(
#version 310 es

precision mediump float;

in vec2 out_uv;

out vec4 frag_color;

uniform sampler2D angular_normal_map;
uniform sampler2D light_map;

uniform float angle_delta;

void main()
{
    vec4 an = texture(angular_normal_map, out_uv);
    float angle = an.x;
    float dist_y = an.y;
    float dist_z = an.z;

    if (dist_y < 0.5f)
    {
        angle = 1.0f - angle;
    }

    angle += angle_delta;
    if (angle > 1.0f)
    {
        angle -= 1.0f;
    }

    frag_color = vec4(texture(light_map, vec2(angle, dist_z)).rgb, 1.0f);
}
)";