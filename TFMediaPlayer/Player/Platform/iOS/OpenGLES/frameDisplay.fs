#version 300 es

precision mediump float;

in vec2 texcoord;

out vec4 FragColor;

uniform sampler2D yPlaneTex;
uniform sampler2D uPlaneTex;
uniform sampler2D vPlaneTex;


void main()
{
    // (1) y - 16 (2) rgb * 1.164
    vec3 yuv;
    yuv.x = texture(yPlaneTex, texcoord).r;
    yuv.y = texture(uPlaneTex, texcoord).r - 0.5f;
    yuv.z = texture(vPlaneTex, texcoord).r - 0.5f;
    
    mat3 trans = mat3(1, 1 ,1,
                      0, -0.34414, 1.772,
                      1.402, -0.71414, 0
                      );
    
    FragColor = vec4(trans*yuv, 1.0);
}
