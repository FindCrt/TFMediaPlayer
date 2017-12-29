#version 300 es

layout (location = 0) in mediump vec3 position;
layout (location = 1) in mediump vec2 inTexcoord;

out mediump vec2 texcoord;

void main()
{
    gl_Position = vec4(position, 1.0);
    texcoord = inTexcoord;
}
