const char* light_fs_source = R"(
#version 310 es

precision highp float;

in vec2 out_uv;

out vec4 frag_color;

const float MAX_HEIGHT = 0.9f;

const vec3 directional_light = normalize(vec3(1.0f, 1.0f, 0.0f));

void main()
{
    float z = out_uv.y * 2.0f - 1.0f;
    float x = 0.0f;
    float y = 0.0f;

    if (z < MAX_HEIGHT && z > -MAX_HEIGHT)
    {
        const float pi_2 = atan(0.0f, -1.0f) * 2.0f;

        if (out_uv.x < 0.5f)
        {
            float angle = out_uv.x * pi_2;
            x = cos(angle);
            y = sin(angle);
        }
        else
        {
            float angle = (1.0f - out_uv.x) * pi_2;
            x = cos(angle);
            y = -sin(angle);
        }

        float l = pow(1.0f - z*z, 0.5f);
        x *= l;
        y *= l;
    }

    frag_color = vec4(vec3(dot(directional_light, vec3(x, y, z))), 1.0f);
}
)";