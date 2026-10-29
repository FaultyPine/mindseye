$input a_position, a_normal, a_texcoord0
$output v_pos, v_view, v_normal, v_color0, v_texcoord0

#include "common.sh"

void main()
{
    vec4 worldPosition = mul(u_model[0], vec4(a_position, 1.0));
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_pos = worldPosition.xyz;
    v_view = mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz - worldPosition.xyz;

    v_normal = mul(u_model[0], vec4(a_normal, 0.0)).xyz;
    v_color0 = vec4_splat(1.0);
    v_texcoord0 = a_texcoord0;
}
