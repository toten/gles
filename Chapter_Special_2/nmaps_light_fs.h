const char* normal_maps_light_fs_source = R"(
#version 310 es

precision mediump float;

in vec2 out_uv;

out vec4 frag_color;

uniform sampler2D angular_normal_map;
uniform sampler2D light_map;

void main()
{
    vec2 an = texture(angular_normal_map, out_uv).xy;

    frag_color = vec4(texture(light_map, an).rgb, 1.0f);
}
)";