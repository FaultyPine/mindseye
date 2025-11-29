$input v_pos, v_view, v_normal, v_color0, v_texcoord0
#include "common.sh"
SAMPLER2D(texDiffuse, 0);

vec2 blinn(vec3 _lightDir, vec3 _normal, vec3 _viewDir)
{
	float ndotl = dot(_normal, _lightDir);
	vec3 reflected = _lightDir - 2.0*ndotl*_normal; // reflect(_lightDir, _normal);
	float rdotv = dot(reflected, _viewDir);
	return vec2(ndotl, rdotv);
}

vec4 lit(float ndotl, float rdotv, float m)
{
	float diff = max(0.0, ndotl);
	float spec = step(0.0, ndotl) * max(0.0, rdotv * m);
	return vec4(1.0, diff, spec, 1.0);
}


void main()
{
    vec3 cameraPositionWorld = vec3(u_invView[3][0], u_invView[3][1], u_invView[3][2]);
	vec3 cameraForwardWorld = normalize(vec3(u_invView[2])); // Third column (Z-axis)
	vec3 lightDir = normalize(vec3(0.0, 1.5, -1.0));
	vec3 normal = normalize(v_normal);
	vec3 view = normalize(v_view);
	vec2 bln = blinn(lightDir, normal, view);
	vec4 lc = lit(bln.x, bln.y, 1.0);
	float specContrib = lc.z;
	float diffuseContrib = lc.y;
	float ambientStrength = 0.2;

	vec3 color = vec3(255, 105, 180) / 255.0; // hot pink
	vec3 litCol = vec3(0.07, 0.06, 0.08) + color*diffuseContrib + pow(specContrib, 128.0);
	litCol += ambientStrength;
	litCol = toReinhard(litCol);

	litCol = toGamma(litCol);
	vec4 diffuseSample = texture2D(texDiffuse, v_texcoord0.xy);

	gl_FragColor.xyz = mix(litCol, diffuseSample.xyz, 0.5);
	gl_FragColor.w = 1.0;
}