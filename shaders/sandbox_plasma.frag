// Estilo GLSL Sandbox: declara sus propios uniforms y usa main().
// GLSLPaper detecta las declaraciones y no las duplica.
#ifdef GL_ES
precision mediump float;
#endif

uniform float time;
uniform vec2  mouse;
uniform vec2  resolution;

void main( void ) {
    vec2 p = ( gl_FragCoord.xy / resolution.xy ) - 0.5;
    p.x *= resolution.x / resolution.y;

    float d = length( p - (mouse - 0.5) );
    float v = sin( d * 20.0 - time * 3.0 );
    v += sin( p.x * 10.0 + time );

    vec3 col = 0.5 + 0.5 * cos( vec3(0.0, 2.0, 4.0) + v * 2.0 );
    gl_FragColor = vec4( col, 1.0 );
}
