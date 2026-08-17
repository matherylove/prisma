// Tunel a cuadros - GLSL 1.10
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2  uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float a  = atan(uv.y, uv.x);
    float r  = length(uv);
    float d  = 0.30 / max(r, 0.001) + iTime * 0.5;

    float chk = mod(floor(a / 3.14159265 * 6.0) + floor(d * 4.0), 2.0);
    vec3  col = mix(vec3(0.08, 0.16, 0.35), vec3(0.95, 0.72, 0.28), chk);
    col *= smoothstep(0.0, 0.35, r);

    fragColor = vec4(col, 1.0);
}
