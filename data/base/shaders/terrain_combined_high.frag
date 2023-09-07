// Version directive is set by Warzone when loading the shader
// (This shader supports GLSL 1.20 - 1.50 core.)

// Texture arrays are supported in:
// 1. core OpenGL 3.0+ (and earlier with EXT_texture_array)
//#if (!defined(GL_ES) && (__VERSION__ < 130))
//#extension GL_EXT_texture_array : require // only enable this on OpenGL < 3.0
//#endif
// 2. OpenGL ES 3.0+
#if (defined(GL_ES) && (__VERSION__ < 300))
#error "Unsupported version of GLES"
#endif

// constants overridden by WZ when loading shaders (do not modify here in the shader source!)
#define WZ_MIP_LOAD_BIAS 0.f
#define WZ_SHADOW_MODE 1
#define WZ_SHADOW_FILTER_SIZE 3
#define WZ_SHADOW_CASCADES_COUNT 3
//

#define WZ_MAX_SHADOW_CASCADES 3

#if (!defined(GL_ES) && (__VERSION__ >= 130)) || (defined(GL_ES) && (__VERSION__ >= 300))
#define NEWGL
#define texture2DArray(tex,coord,bias) texture(tex,coord,bias)
#else
#define texture(tex,uv,bias) texture2D(tex,uv,bias)
#endif

uniform sampler2D lightmap_tex;

// ground texture arrays. layer = ground type
uniform sampler2DArray groundTex;
uniform sampler2DArray groundNormal;
uniform sampler2DArray groundSpecular;
uniform sampler2DArray groundHeight;

// array of scales for ground textures, encoded in mat4. scale_i = groundScale[i/4][i%4]
uniform mat4 groundScale;

// decal texture arrays. layer = decal tile
uniform sampler2DArray decalTex;
uniform sampler2DArray decalNormal;
uniform sampler2DArray decalSpecular;
uniform sampler2DArray decalHeight;

// shadow map
uniform sampler2DArrayShadow shadowMap;

uniform mat4 ViewMatrix;
uniform mat4 ShadowMapMVPMatrix[WZ_MAX_SHADOW_CASCADES];
uniform vec4 ShadowMapCascadeSplits;
uniform int ShadowMapSize;

// sun light colors/intensity:
uniform vec4 emissiveLight;
uniform vec4 ambientLight;
uniform vec4 diffuseLight;
uniform vec4 specularLight;

uniform vec4 sunPos; // in modelSpace, normalized

// fog
uniform int fogEnabled; // whether fog is enabled
uniform float fogEnd;
uniform float fogStart;
uniform vec4 fogColor;

in vec2 uvLightmap;
in vec2 uvDecal;
in vec2 uvGround;
in float vertexDistance;
flat in int tile;
flat in uvec4 fgrounds;
in vec4 fgroundWeights;
// Light in tangent space:
in vec3 groundLightDir;
in vec3 groundHalfVec;
in mat2 decal2groundMat2;
// For Shadows
in vec3 fragPos;
in vec3 fragNormal;

#ifdef NEWGL
out vec4 FragColor;
#else
// Uses gl_FragColor
#endif

#include "terrain_combined_frag.glsl"

vec3 getGroundUv(int i) {
	uint groundNo = fgrounds[i];
	return vec3(uvGround * groundScale[groundNo/4u][groundNo%4u], groundNo);
}

vec3 getGround(int i) {
	return texture2DArray(groundTex, getGroundUv(i), WZ_MIP_LOAD_BIAS).rgb * fgroundWeights[i];
}

struct BumpData {
	vec4 color;
	vec3 N;
	float gloss;
};

void getGroundBM(int i, inout BumpData res) {
	vec3 uv = getGroundUv(i);
	float w = fgroundWeights[i];
	res.color += texture2DArray(groundTex, uv, WZ_MIP_LOAD_BIAS) * w;
	vec3 N = texture2DArray(groundNormal, uv, WZ_MIP_LOAD_BIAS).xyz;
	if (N == vec3(0.f)) {
		N = vec3(0.f,0.f,1.f);
	} else {
		N = normalize(N * 2.f - 1.f);
	}
	res.N += N * w;
	res.gloss += texture2DArray(groundSpecular, uv, WZ_MIP_LOAD_BIAS).r * w;
}

vec3 blendAddEffectLighting(vec3 a, vec3 b) {
	return min(a + b, vec3(1.0));
}

vec4 doBumpMapping(BumpData b, vec3 lightDir, vec3 halfVec) {
	vec3 L = normalize(lightDir);
	float lambertTerm = max(dot(b.N, L), 0.0); // diffuse lighting
	// Gaussian specular term computation
	vec3 H = normalize(halfVec);
	float blinnTerm = clamp(dot(b.N, H), 0.f, 1.f);
	blinnTerm = lambertTerm != 0.0 ? blinnTerm : 0.0;
	blinnTerm = pow(blinnTerm, 16.f);
	float visibility = getShadowVisibility();
	vec4 lightmap_vec4 = texture(lightmap_tex, uvLightmap, 0.f);

	float adjustedTileBrightness = pow(lightmap_vec4.a, 1.5f); // ... * tile brightness / ambient occlusion (stored in lightmap.a)

	vec4 light = (ambientLight + visibility*diffuseLight*lambertTerm) * adjustedTileBrightness;
	light.rgb = blendAddEffectLighting(light.rgb, (lightmap_vec4.rgb / 1.5f)); // additive color (from environmental point lights / effects)
	light.a = 1.f;

	vec4 light_spec = (visibility*specularLight*blinnTerm*lambertTerm) * adjustedTileBrightness;
	light_spec.rgb = blendAddEffectLighting(light_spec.rgb, (lightmap_vec4.rgb / 2.f)); // additive color (from environmental point lights / effects)
	light_spec *= b.gloss;

	vec4 res = (b.color*light) + light_spec;

	return vec4(res.rgb, b.color.a);
}

vec4 main_bumpMapping() {
	BumpData bump;
	bump.color = vec4(0.f);
	bump.N = vec3(0.f);
	bump.gloss = 0.f;
	getGroundBM(0, bump);
	getGroundBM(1, bump);
	getGroundBM(2, bump);
	getGroundBM(3, bump);

	if (tile >= 0) {
		vec3 uv = vec3(uvDecal, tile);
		vec4 decalColor = texture2DArray(decalTex, uv, WZ_MIP_LOAD_BIAS);
		float a = decalColor.a;
		// blend color, normal and gloss with ground ones based on alpha
		bump.color = (1.f - a)*bump.color + a*vec4(decalColor.rgb, 1.f);
		vec3 n = texture2DArray(decalNormal, uv, WZ_MIP_LOAD_BIAS).xyz;
		if (n == vec3(0.f)) {
			n = vec3(0.f,0.f,1.f);
		} else {
			n = normalize(n * 2.f - 1.f);
			n = vec3(n.xy * decal2groundMat2, n.z);
		}
		bump.N = (1.f - a)*bump.N + a*n;
		bump.gloss = (1.f - a)*bump.gloss + a*texture2DArray(decalSpecular, uv, WZ_MIP_LOAD_BIAS).r;
	}
	return doBumpMapping(bump, groundLightDir, groundHalfVec);
}

void main()
{
	vec4 fragColor = main_bumpMapping();

	if (fogEnabled > 0)
	{
		// Calculate linear fog
		float fogFactor = (fogEnd - vertexDistance) / (fogEnd - fogStart);
		fogFactor = clamp(fogFactor, 0.0, 1.0);

		// Return fragment color
		fragColor = mix(fragColor, vec4(fogColor.xyz, fragColor.w), fogFactor);
	}

	#ifdef NEWGL
	FragColor = fragColor;
	#else
	gl_FragColor = fragColor;
	#endif
}
