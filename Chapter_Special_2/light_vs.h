
const char* light_vs_source = R"(
#version 310 es

layout(location = 0) in vec2 uv;

out vec2 out_uv;

void main()
{
    gl_Position = vec4(uv * 2.0f - vec2(1.0f), 0.0f, 1.0f);
    
    out_uv = uv;
}
)";