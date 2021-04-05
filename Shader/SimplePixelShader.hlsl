// Copyright 2021 Yoshito Nakaue

struct Input
{
	float4 pos : POSITION;
	float4 svpos : SV_POSITION;
};

float4 SimplePS(Input input) : SV_TARGET
{
	return float4(1.0f, (float2(1, 0) + input.pos.xy) * 0.5f, 1.0f);
}
