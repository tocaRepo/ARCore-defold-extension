#version 140
in mediump vec2 var_texcoord0;
out vec4 out_fragColor;
uniform mediump sampler2D texture_sampler;
uniform fs_uniforms {
    mediump vec4 uv0;
    mediump vec4 uv1;
    mediump vec4 uv2;
};
void main() {
    vec2 image_uv = uv0.xy + var_texcoord0.x * (uv1.xy - uv0.xy)
                              + var_texcoord0.y * (uv2.xy - uv0.xy);
    out_fragColor = vec4(texture(texture_sampler, image_uv).rgb, 1.0);
}
