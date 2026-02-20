$input v_pos, v_texcoord0, v_view
#include "common.sh"
SAMPLER2D(texDiffuse, 0);

void main()
{
	vec4 tex = texture2D(texDiffuse, v_texcoord0.xy);
	gl_FragColor.rgb = tex.rgb;
	gl_FragColor.w = 1.0;
}