$input a_position
$output v_pos, v_view

#include "common.sh"

void main()
{
	vec3 pos = a_position;
	gl_Position = mul(u_modelViewProj, vec4(pos, 1.0) );
	v_view = mul(u_modelView, vec4(pos, 1.0) ).xyz;
	v_pos = pos.xyz;
}
