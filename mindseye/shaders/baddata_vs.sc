$input a_position, a_normal, a_texcoord0
$output v_pos, v_view, v_normal, v_color0, v_texcoord0

#include "common.sh"

void main()
{
	vec3 pos = a_position;
	vec3 normal = a_normal.xyz;
	gl_Position = mul(u_modelViewProj, vec4(pos, 1.0) );
	v_view = mul(u_modelView, vec4(pos, 1.0) ).xyz;
	v_normal = normal;
	v_texcoord0 = a_texcoord0;
	v_color0 = vec4(1.0, 1.0, 1.0, 1.0);
	v_pos = pos.xyz;
}
