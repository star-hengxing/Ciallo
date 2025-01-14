#include "pch.hpp"
#include "Stroke.h"

#include "RenderingSystem.h"

Stroke::Stroke()
{
	GenBuffers();
}

Stroke::Stroke(Stroke&& other) noexcept
{
	*this = std::move(other);
}

Stroke& Stroke::operator=(Stroke&& other) noexcept
{
	if (this == &other)
		return *this;
	Position = std::move(other.Position);
	Thickness = other.Thickness;
	ThicknessOffset = other.ThicknessOffset;
	Color = other.Color;
	BrushE = other.BrushE;
	FillColor = other.FillColor;
	VertexBuffers = other.VertexBuffers;
	VertexArray = other.VertexArray;

	other.Zeroize();
	return *this;
}

Stroke::~Stroke()
{
	glDeleteBuffers(VertexBuffers.size(), VertexBuffers.data());
	glDeleteVertexArrays(1, &VertexArray);
	Zeroize();
}

void Stroke::UpdateBuffers()
{
	UpdatePositionBuffer();
	UpdateThicknessOffsetBuffer();
	UpdateDistanceBuffer();
}

void Stroke::GenBuffers()
{
	glCreateVertexArrays(1, &VertexArray);
	glCreateBuffers(VertexBuffers.size(), VertexBuffers.data());

	glEnableVertexArrayAttrib(VertexArray, 0);
	glVertexArrayAttribBinding(VertexArray, 0, 0);
	glVertexArrayAttribFormat(VertexArray, 0, 2, GL_FLOAT, GL_FALSE, 0);
	glEnableVertexArrayAttrib(VertexArray, 1);
	glVertexArrayAttribBinding(VertexArray, 1, 1);
	glVertexArrayAttribFormat(VertexArray, 1, 1, GL_FLOAT, GL_FALSE, 0);
	glEnableVertexArrayAttrib(VertexArray, 2);
	glVertexArrayAttribBinding(VertexArray, 2, 2);
	glVertexArrayAttribFormat(VertexArray, 2, 1, GL_FLOAT, GL_FALSE, 0);

	GLintptr offsets[] = {0, 0, 0};
	int strides[] = {sizeof(glm::vec2), sizeof(float), sizeof(float)};

	glVertexArrayVertexBuffers(VertexArray, 0, VertexBuffers.size(), VertexBuffers.data(), offsets, strides);
}

void Stroke::UpdatePositionBuffer()
{
	glNamedBufferData(VertexBuffers[0], Position.size() * sizeof(glm::vec2), Position.data(), GL_DYNAMIC_DRAW);
}

void Stroke::UpdateThicknessOffsetBuffer()
{
	if (ThicknessOffset.size() <= 1)
	{
		float v = ThicknessOffset.empty() ? 0.0f : ThicknessOffset.at(0);
		std::vector<float> values(Position.size(), v);
		glNamedBufferData(VertexBuffers[1], values.size() * sizeof(float), values.data(), GL_DYNAMIC_DRAW);
		return;
	}
	glNamedBufferData(VertexBuffers[1], ThicknessOffset.size() * sizeof(float), ThicknessOffset.data(),
	                  GL_DYNAMIC_DRAW);
}

// Using position buffer, be careful about the calling order.
void Stroke::UpdateDistanceBuffer()
{
	glNamedBufferData(VertexBuffers[2], Position.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
	glUseProgram(RenderingSystem::PrefixSum->Program);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, VertexBuffers[0]);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, VertexBuffers[2]);
	glDispatchCompute(1, 1, 1);
}

void Stroke::LineDrawCall()
{
	int count = Position.size();
	if (count == 1)
	{
		glm::vec2 p = Position[0];
		float offset = 0.0f;
		if (!ThicknessOffset.empty()) offset = ThicknessOffset[0];

		glm::vec2 paddedPos = {p.x + 0.01f * (Thickness + offset), p.y};
		Position.push_back(paddedPos);

		UpdatePositionBuffer();
		UpdateThicknessOffsetBuffer();

		Position.pop_back();

		count = 2;
	}

	glBindVertexArray(VertexArray);
	glDrawArrays(GL_LINE_STRIP, 0, count);
}

void Stroke::FillDrawCall()
{
	int count = Position.size(); // number of points in polygon
	if (count <= 2) return;

	glClear(GL_STENCIL_BUFFER_BIT);
	glStencilMask(1);
	// render to stencil only
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	// flip bit in stencil buffer pixel, odd pass the stencil test.
	glStencilFunc(GL_ALWAYS, 0, 1);
	glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
	glBindVertexArray(VertexArray);
	glDrawArrays(GL_TRIANGLE_FAN, 0, count);
	// only color odd times fliped pixel.
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glStencilFunc(GL_EQUAL, 1, 1);
	glDrawArrays(GL_TRIANGLE_FAN, 0, count);
}

void Stroke::SetUniforms()
{
	glUniform1f(2, Thickness);
	glUniform4fv(1, 1, glm::value_ptr(Color));
}

void Stroke::Zeroize()
{
	VertexBuffers.fill(0);
	VertexArray = 0;
}
