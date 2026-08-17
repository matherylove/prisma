// Plasma clasico - GLSL 1.10
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    vec2 p  = (uv - 0.5) * vec2(iResolution.x / iResolution.y, 1.0);

    float t = iTime;
    float v = sin(p.x * 8.0 + t) + sin(p.y * 8.0 - t * 0.7);
    v += sin(length(p) * 12.0 - t * 2.0);

    vec3 col = 0.5 + 0.5 * cos(vec3(0.0, 2.0, 4.0) + v * 1.5 + t * 0.2);
    fragColor = vec4(col, 1.0);
}
