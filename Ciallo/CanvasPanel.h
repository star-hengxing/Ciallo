﻿#pragma once

#include "Drawing.h"
#include "Toolbox.h"

class CanvasPanel
{
public:
	CanvasPanel();
	Drawing* ActiveDrawing = nullptr;
	float DrawingRotation = 0.0f;
	float Zoom = 1.0f;
	glm::vec2 Scroll{0.0f, 0.0f};
	glm::vec2 MousePosOnDrawing;

	std::unique_ptr<Toolbox> Toolbox;

	void DrawAndRunTool();
};
