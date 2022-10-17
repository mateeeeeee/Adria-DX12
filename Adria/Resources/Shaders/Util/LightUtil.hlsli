#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct Light
{
    float4 ss_position;
    float4 position;
    float4 direction;
    float4 color;
    float range;
    int type;
    float outer_cosine;
    float inner_cosine;
    int casts_shadows;
    int use_cascades;
    int active;
    float volumetric_strength;
    int   volumetric;
    int   lens_flare;
    int   god_rays;
    float godrays_decay;
    float godrays_weight;
    float godrays_density;
    float godrays_exposure;
    int   sscs;
    float sscs_thickness;
    float sscs_max_ray_distance;
    float sscs_max_depth_distance;
};  
struct StructuredLight
{
    float4 position;
    float4 direction;
    float4 color;
    int active;
    float range;
    int type;
    float outer_cosine;
    float inner_cosine;
    int casts_shadows;
    int use_cascades;
    int padd;
};
static Light CreateLightFromStructured(in StructuredLight structured_light)
{
    Light l = (Light) 0;
    l.casts_shadows = structured_light.casts_shadows;
    l.color = structured_light.color;
    l.direction = structured_light.direction;
    l.inner_cosine = structured_light.inner_cosine;
    l.outer_cosine = structured_light.outer_cosine;
    l.position = structured_light.position;
    l.range = structured_light.range;
    l.type = structured_light.type;
    l.use_cascades = structured_light.use_cascades;
    
    l.godrays_decay = 0;
    l.godrays_density = 0;
    l.godrays_exposure = 0;
    l.godrays_weight = 0;
    l.ss_position = 0;
    l.sscs = 0;
    l.volumetric_strength = 0;
    
    return l;
}

static float DoAttenuation(float distance, float range)
{
    float att = saturate(1.0f - (distance * distance / (range * range)));
    return att * att;
}
static float4 DoDiffuse(Light light, float3 L, float3 N)
{
    float NdotL = max(0, dot(N, L));
    return light.color * NdotL;
}
static float4 DoSpecular(Light light, float shininess, float3 L, float3 N, float3 V)
{
    // Phong lighting.
    float3 R = normalize(reflect(-L, N));
    float RdotV = max(0, dot(R, V));
 
    // Blinn-Phong lighting
    //float3 H = normalize(L + V);
    //float NdotH = max(0, dot(N, H));
 
    return light.color * pow(RdotV, shininess);
}
struct LightingResult
{
    float4 Diffuse;
    float4 Specular;
};
 
static LightingResult DoPointLight(Light light, float shininess, float3 V, float3 P, float3 N)
{
    LightingResult result;
    light.position.xyz /= light.position.w;
 
    float3 L = light.position.xyz - P;
    float distance = length(L);
    L = L / distance;
    
    N = normalize(N);
 
    float attenuation = DoAttenuation(distance, light.range);
 
    result.Diffuse = DoDiffuse(light, L, N) * attenuation;
    result.Specular = DoSpecular(light, shininess, V, L, N) * attenuation;
 
    return result;
}
static LightingResult DoDirectionalLight(Light light, float shininess, float3 V, float3 N)
{
    LightingResult result;
    
    N = normalize(N);
 
    float3 L = -light.direction.xyz;
    L = normalize(L);
 
    result.Diffuse = DoDiffuse(light, L, N);
    result.Specular = DoSpecular(light, shininess, V, L, N);
 
    return result;
}
static LightingResult DoSpotLight(Light light, float shininess, float3 V, float3 P, float3 N)
{
    LightingResult result;
    
    //light.position.xyz /= light.position.w;
    
    N = normalize(N);

    float3 L = light.position.xyz - P;
    float distance = length(L);
    L = L / distance;
 
    float attenuation = DoAttenuation(distance, light.range);
    
    float3 normalized_light_dir = normalize(light.direction.xyz);
    float cosAng = dot(-normalized_light_dir, L);
    
    float conAtt = saturate((cosAng - light.outer_cosine) / (light.inner_cosine - light.outer_cosine));
    conAtt *= conAtt;

 
    result.Diffuse = DoDiffuse(light, L, N) * attenuation * conAtt;
    result.Specular = DoSpecular(light, shininess, V, L, N) * attenuation * conAtt;
 
    return result;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

//PBR

static const float PI = 3.14159265359;
static float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001); // prevent divide by zero for roughness=0.0 and NdotH=1.0
}
static float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
static float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
static float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}
static float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

static float3 DoSpotLightPBR(Light light, float3 positionVS, float3 normalVS, float3 V, float3 albedo, float metallic, float roughness)
{
   
    /*
    */
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);
    
    
     // calculate per-light radiance
    float3 L = normalize(light.position.xyz - positionVS);
    float3 H = normalize(V + L);
    float distance = length(light.position.xyz - positionVS);
    float attenuation = DoAttenuation(distance, light.range);
    
    float3 normalized_light_dir = normalize(light.direction.xyz);
    float cosAng = dot(-normalized_light_dir, L);
    float conAtt = saturate((cosAng - light.outer_cosine) / (light.inner_cosine - light.outer_cosine));
    conAtt *= conAtt;
    
    float3 radiance = light.color.xyz * attenuation * conAtt;
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normalVS, H, roughness);
    float G = GeometrySmith(normalVS, V, L, roughness);
    float3 F = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);
           
    float3 nominator = NDF * G * F;
    float denominator = 4 * max(dot(normalVS, V), 0.0) * max(dot(normalVS, L), 0.0);
    float3 specular = nominator / max(denominator, 0.001); // prevent divide by zero for NdotV=0.0 or NdotL=0.0
        
        // kS is equal to Fresnel
    float3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
    kD *= 1.0 - metallic;

        // scale light by NdotL
    float NdotL = max(dot(normalVS, L), 0.0);

        // add to outgoing radiance Lo
    float3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;
    
    return Lo;
}
static float3 DoPointLightPBR(Light light, float3 positionVS, float3 normalVS, float3 V, float3 albedo, float metallic, float roughness)
{
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);
    
    
     // calculate per-light radiance
    float3 L = normalize(light.position.xyz - positionVS);
    float3 H = normalize(V + L);
    float distance = length(light.position.xyz - positionVS);
    float attenuation = DoAttenuation(distance, light.range);
    float3 radiance = light.color.xyz * attenuation;
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normalVS, H, roughness);
    float G = GeometrySmith(normalVS, V, L, roughness);
    float3 F = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);
           
    float3 nominator = NDF * G * F;
    float denominator = 4 * max(dot(normalVS, V), 0.0) * max(dot(normalVS, L), 0.0);
    float3 specular = nominator / max(denominator, 0.001); // prevent divide by zero for NdotV=0.0 or NdotL=0.0
        
        // kS is equal to Fresnel
    float3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
    kD *= 1.0 - metallic;

        // scale light by NdotL
    float NdotL = max(dot(normalVS, L), 0.0);

        // add to outgoing radiance Lo
    float3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;
    
    return Lo;
}
static float3 DoDirectionalLightPBR(Light light, float3 positionVS, float3 normalVS, float3 V, float3 albedo, float metallic, float roughness)
{
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);
    
    
     // calculate per-light radiance
    
    float3 L = normalize(-light.direction.xyz);
    float3 H = normalize(V + L);
    
    float3 radiance = light.color.xyz;
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normalVS, H, roughness);
    float G = GeometrySmith(normalVS, V, L, roughness);
    float3 F = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);
           
    float3 nominator = NDF * G * F;
    float denominator = 4 * max(dot(normalVS, V), 0.0) * max(dot(normalVS, L), 0.0);
    float3 specular = nominator / max(denominator, 0.001); // prevent divide by zero for NdotV=0.0 or NdotL=0.0
        
        // kS is equal to Fresnel
    float3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
    kD *= 1.0 - metallic;

        // scale light by NdotL
    float NdotL = max(dot(normalVS, L), 0.0);

        // add to outgoing radiance Lo
    float3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;
    
    return Lo;
}