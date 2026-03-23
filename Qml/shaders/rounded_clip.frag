#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 u_size;
    float u_radius;
};

layout(binding = 1) uniform sampler2D source;

float roundedBoxSdf(vec2 p, vec2 halfExtent, float radius)
{
    vec2 q = abs(p) - halfExtent + vec2(radius);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

void main()
{
    vec2 centered = qt_TexCoord0 * u_size - (u_size * 0.5);
    vec2 halfExtent = (u_size * 0.5) - vec2(u_radius);
    float d = roundedBoxSdf(centered, halfExtent, u_radius);
    float aa = 1.0;
    float mask = 1.0 - smoothstep(0.0, aa, d);
    if (mask <= 0.0) {
        discard;
    }
    vec4 src = texture(source, qt_TexCoord0);
    fragColor = vec4(src.rgb * mask, src.a * mask) * qt_Opacity;
}
