#pragma once

class StampBrushData
{
public:
	GLuint StampTexture = 0;
	float StampIntervalRatio = 0.0f;
	float NoiseFactor = 0.0f;
	float RotationRand = 1.0f;

	StampBrushData() = default;

	void SetUniforms();
	void DrawProperties();
};

