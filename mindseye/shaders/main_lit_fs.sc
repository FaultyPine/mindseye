$input v_pos, v_view, v_normal, v_color0, v_texcoord0

#include "common.sh"
#include "editor.sh"

SAMPLER2D(texDiffuse, 0);
uniform vec4 albedoTint = vec4(1);

const vec3 lightDirection = vec3(-0.45, 0.85, 0.35); // worldspace direction toward the light
const vec3 lightColor = vec3(0.82, 0.76, 0.64);
const vec3 groundAmbientColor = vec3(0.18, 0.16, 0.14);
const vec3 skyAmbientColor = vec3(0.40, 0.46, 0.54);
const float specularShininess = 48.0;
const float specularStrength = 0.12;
const float specularLightThreshold = 0.001;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 lightDir = normalize(lightDirection);
    vec3 viewDir = normalize(v_view);
    vec3 albedo = texture2D(texDiffuse, v_texcoord0).rgb * albedoTint.rgb;
    if (IsObjectSelected())
    {
        albedo.g *= 1.8;
    }

    // remap normal.y [-1, 1] -> [0, 1] to blend ground and sky lighting
    float skyBlend = normal.y * 0.5 + 0.5;
    vec3 ambient = mix(groundAmbientColor, skyAmbientColor, skyBlend);

    float diffuseAmount = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = lightColor * diffuseAmount;

    // blinn phong
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float specularAmount = pow(max(dot(normal, halfwayDir), 0.0), specularShininess);
    specularAmount *= step(specularLightThreshold, diffuseAmount);
    vec3 specular = vec3_splat(specularAmount * specularStrength);

    vec3 color = albedo * (ambient + diffuse) + specular;
    gl_FragColor = vec4(color, 1.0);
}
