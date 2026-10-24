#version 140
in mediump vec3 var_position;
out vec4 out_fragColor;
void main() {
    float edge = max(abs(var_position.x), abs(var_position.z));
    float crosshair = step(min(abs(var_position.x), abs(var_position.z)), 0.013);
    vec3 color = mix(vec3(0.04, 0.45, 0.6), vec3(0.3, 1.0, 0.8), crosshair);
    if (edge > 0.087) color = vec3(1.0, 0.65, 0.15);
    out_fragColor = vec4(color, 1.0);
}
