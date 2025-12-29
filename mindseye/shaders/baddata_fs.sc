$input v_pos, v_view
#include "common.sh"

void main()
{
    vec3 cameraPositionWorld = vec3(u_invView[3][0], u_invView[3][1], u_invView[3][2]);
	vec3 cameraForwardWorld = normalize(vec3(u_invView[2])); // Third column (Z-axis)
	vec3 lightDir = normalize(vec3(0.0, 1.5, -1.0));

	vec3 color = vec3(255, 105, 180) / 255.0; // hot pink
	
	gl_FragColor.rgb = color;
	gl_FragColor.w = 1.0;
}