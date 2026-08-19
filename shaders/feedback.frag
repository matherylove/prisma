// Usa 'backbuffer' (el fotograma anterior) al estilo GLSL Sandbox.
#ifdef GL_ES
precision mediump float;
#endif

uniform float time;
uniform vec2  resolution;
uniform sampler2D backbuffer;

void main( void ) {
    vec2 uv = gl_FragCoord.xy / resolution.xy;

    // realimentacion: encogemos y giramos ligeramente el frame anterior
    vec2 c = uv - 0.5;
    float a = 0.01;
    vec2 r = vec2( c.x * cos(a) - c.y * sin(a),
                   c.x * sin(a) + c.y * cos(a) ) * 0.99 + 0.5;

    vec3 prev = texture2D( backbuffer, r ).rgb * 0.97;

    float d = length( c - vec2( sin(time) * 0.3, cos(time * 1.3) * 0.3 ) );
    vec3 dot_ = vec3( 1.0, 0.6, 0.2 ) * smoothstep( 0.05, 0.0, d );

    gl_FragColor = vec4( prev + dot_, 1.0 );
}
