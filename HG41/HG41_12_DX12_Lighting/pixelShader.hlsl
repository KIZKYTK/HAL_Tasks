


Texture2D<float4> texture0 : register(t0);
SamplerState sampler0 : register(s0);


struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
    float4 Diffuse  : COLOR;
};



float4 main(PS_INPUT input) : SV_TARGET
{
    float3 lightDirection = float3(1.0, -1.0, 1.0);
    lightDirection = normalize(lightDirection);
    
    float light = saturate(0.5 - dot(input.Normal.xyz, lightDirection) * 0.5);
    
    float4 color = texture0.Sample(sampler0, input.TexCoord);
    
    color.rgb *= light;
    
    return color;
}
