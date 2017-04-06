
#include "shader-common.hlsli"
float u_subsample = 4;

Texture2D<float4> t_UnfilteredRT : register(t0);
RWTexture2D<float4> renderTarget:register(u0);

SamplerState s_Sampler : register(s0);
float EPS = 1.0e-8;


float3 shading(float2 pixelPos) {
	float3 c = t_UnfilteredRT.SampleLevel(s_Sampler, pixelPos, 0).rgb;
	return c;
}

float3 shadingSSAA(float2 pixelPos) {
	float3 rgb = float3(0.0, 0.0, 0.0);
	for (int i = 0; i < u_subsample; i++) {
		for (int j = 0; j < u_subsample; j++) {
			float2 subpixel = pixelPos * u_subsample + float2(i, j);
			rgb += shading(subpixel);
		}
	}
	rgb /= (u_subsample * u_subsample);

	return rgb;
}

float3 shadingMSAA(float2 pixelPos) {

	return shading(pixelPos);

	// Edge test
	float zValue = t_UnfilteredRT.SampleLevel(s_Sampler, pixelPos * u_subsample, 0).w;
	bool isEdge = false;
	for (int i = 0; i < u_subsample; i++) {
		for (int j = 0; j < u_subsample; j++) {
			float2 subpixel = pixelPos * u_subsample + float2(i, j);
			float zSub = t_UnfilteredRT.SampleLevel(s_Sampler, subpixel,subpixel).w;
			if (abs(zValue - zSub) > 0.5e-4) {
				isEdge = true;
				break;
			}
		}

		if (isEdge) {
			break;
		}
	}
	// Shading
	if (isEdge) {
		return shadingSSAA(pixelPos);
	}
	else {
		return shading(pixelPos * SUBSUMPLE);
	}
}
[numthreads(8,8, 1)]
void main(
	in int2 i_threadIdx : SV_GroupThreadID,
	in int2 i_globalIdx : SV_DispatchThreadID
) {
	float w, h;
	renderTarget.GetDimensions(w, h);
	float2 uv = float2(i_globalIdx.x/w, i_globalIdx.y/h);

	float2 pixelPos = i_globalIdx;

	float4 rgb = float4(shadingMSAA(uv),1);

	renderTarget[pixelPos]= rgb;
}
