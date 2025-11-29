$input a_position, a_normal, a_texcoord0
$output v_pos, v_view, v_normal, v_color0, v_texcoord0

#include "common.sh"

uniform vec4 u_time;

void main()
{
	vec3 pos = a_position;

	float sx = sin(pos.x*32.0+u_time.x*4.0)*0.5+0.5;
	float cy = cos(pos.y*32.0+u_time.x*4.0)*0.5+0.5;
	vec3 displacement = vec3(sx, cy, sx*cy);
	vec3 normal = a_normal.xyz;

	//pos = pos + normal*displacement*vec3(0.06, 0.06, 0.06);

	gl_Position = mul(u_modelViewProj, vec4(pos, 1.0) );
	v_pos = pos.xyz;
	v_view = mul(u_modelView, vec4(pos, 1.0) ).xyz;

	v_normal = normal;

	float len = length(displacement)*0.4+0.6;
	v_color0 = vec4(len, len, len, 1.0);
	v_texcoord0 = a_texcoord0;

}




/*
Predefined uniforms (declared in bgfx_shader.sh):

u_viewRect vec4(x, y, width, height) - view rectangle for current view, in pixels.

u_viewTexel vec4(1.0/width, 1.0/height, undef, undef) - inverse width and height

u_view mat4 - view matrix

u_invView mat4 - inverted view matrix

u_proj mat4 - projection matrix

u_invProj mat4 - inverted projection matrix

u_viewProj mat4 - concatenated view projection matrix

u_invViewProj mat4 - concatenated inverted view projection matrix

u_model mat4[BGFX_CONFIG_MAX_BONES] - array of model matrices.

u_modelView mat4 - concatenated model view matrix, only first model matrix from array is used.
*/