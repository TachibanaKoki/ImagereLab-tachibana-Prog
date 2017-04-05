
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

float3 shadingSSAA(float2 pixelPos,float2 aspect) {
	float3 rgb = float3(0.0, 0.0, 0.0);
	for (int i = 0; i < u_subsample; i++) {
		for (int j = 0; j < u_subsample; j++) {
			float2 subpixel = pixelPos + float2(i / aspect.x, j / aspect.y);
			rgb += shading(subpixel);
		}
	}
	rgb /= (u_subsample * u_subsample);

	return rgb;
}

float3 shadingMSAA(float2 pixelPos,float2 aspect) {

	// Edge test
	float zValue = t_UnfilteredRT.SampleLevel(s_Sampler, pixelPos * u_subsample, 0).w;
	bool isEdge = false;
	for (int i = 0; i < u_subsample; i++) {
		for (int j = 0; j < u_subsample; j++) {
			float2 subpixel = pixelPos  + float2(i/aspect.x, j/aspect.y);
			float zSub = t_UnfilteredRT.SampleLevel(s_Sampler, subpixel, subpixel).w;
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
		return shadingSSAA(pixelPos,aspect);
	}
	else {
		return shading(pixelPos);
	}
}
[numthreads(16, 16, 1)]
void main(
	in int2 i_threadIdx : SV_GroupThreadID,
	in int2 i_globalIdx : SV_DispatchThreadID
) {
	float w, h;
	renderTarget.GetDimensions(w, h);
	float2 uv = float2(i_globalIdx.x / w, i_globalIdx.y / h);

	float2 pixelPos = i_globalIdx;
	float2 aspect = float2(w,h);

	float4 rgb = float4(shadingMSAA(uv,aspect), 1);

	//float a = t_UnfilteredRT.SampleLevel(s_Sampler,uv, 0).r;

	renderTarget[pixelPos] = rgb;
}
