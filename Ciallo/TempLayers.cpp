#include "pch.hpp"
#include "TempLayers.h"

#include "StrokeContainer.h"
#include "Stroke.h"
#include "Brush.h"
#include "RenderingSystem.h"
#include "Canvas.h"
#include "ArrangementManager.h"
#include "Painter.h"
#include "InnerBrush.h"

#include <glm/gtx/transform.hpp>

TempLayers::TempLayers(glm::ivec2 size)
{
	Overlay = RenderableTexture{size.x, size.y};
	Drawing = RenderableTexture{size.x, size.y};
	Fill = RenderableTexture{size.x, size.y};
	GenCircleStroke();
}

void TempLayers::RenderOverlay()
{
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	Overlay.BindFramebuffer();

	auto& viewport = R.ctx().get<Canvas>().Viewport;
	viewport.UploadMVP();
	viewport.BindMVPBuffer();

	Lines.clear();
	auto& ls = R.ctx().get<OverlayContainer>().Lines;
	for (auto& l : ls)
	{
		if (l.size() == 0) continue;
		Stroke s{};
		s.Position = l;
		s.Thickness = 0.0003f;
		s.Color = {135.0f / 255, 206.0f / 255, 235.0f / 255, 1.0f};
		s.UpdateBuffers();
		Lines.push_back(std::move(s));
	}

	auto& brush = R.ctx().get<InnerBrush>().Get("vanilla");
	brush.Use();

	for (auto& line : Lines)
	{
		line.SetUniforms();
		line.LineDrawCall();
	}

	auto& circlePos = R.ctx().get<OverlayContainer>().Circles;
	for (auto& c : circlePos)
	{
		for (glm::vec2 pos : c)
		{
			viewport.UploadMVP(glm::translate(glm::vec3(pos.x, pos.y, 0.0f)));
			Circle.SetUniforms();
			Circle.LineDrawCall();
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TempLayers::RenderDrawing()
{
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	Drawing.BindFramebuffer();
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	auto& canvas = R.ctx().get<Canvas>();
	canvas.Viewport.UploadMVP();
	canvas.Viewport.BindMVPBuffer();
	auto& strokeEs = R.ctx().get<StrokeContainer>().StrokeEs;
	for (entt::entity e : strokeEs)
	{
		auto& stroke = R.get<Stroke>(e);
		auto strokeUsage = R.get<StrokeUsageFlags>(e);
		bool skipLine = FinalOnly && !(strokeUsage & StrokeUsageFlags::Final);
		if (!skipLine)
		{
			auto& brush = R.get<Brush>(stroke.BrushE);
			brush.Use();
			brush.SetUniforms();
			stroke.SetUniforms();
			stroke.LineDrawCall();
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TempLayers::RenderFill()
{
	Fill.BindFramebuffer();
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	auto& canvas = R.ctx().get<Canvas>();
	canvas.Viewport.UploadMVP();
	canvas.Viewport.BindMVPBuffer();
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	auto& arm = R.ctx().get<ArrangementManager>();
	auto& strokeEs = R.ctx().get<StrokeContainer>().StrokeEs;
	glUseProgram(RenderingSystem::Polygon->Program);
	glEnable(GL_STENCIL_TEST);
	for (entt::entity e : strokeEs)
	{
		auto& stroke = R.get<Stroke>(e);
		auto strokeUsage = R.get<StrokeUsageFlags>(e);
		if (!!(strokeUsage & StrokeUsageFlags::Fill))
		{
			glUniform4fv(1, 1, glm::value_ptr(stroke.FillColor));
			stroke.FillDrawCall();
		}
		if (!!(strokeUsage & StrokeUsageFlags::Zone))
		{
			if (arm.QueryResultsContainer.contains(e))
			{
				for (auto& face : arm.QueryResultsContainer[e])
				{
					face.GenUploadBuffers();
					glUniform4fv(1, 1, glm::value_ptr(stroke.FillColor));
					face.FillDrawCall();
				}
			}
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TempLayers::BlendAll()
{
	auto& canvas = R.ctx().get<Canvas>();
	auto& port = canvas.Viewport;
	glm::vec2 size = port.GetSize() / 2.0f;
	glm::vec2 mid = (port.Min + port.Max) / 2.0f;

	GLuint vao, vbo;
	glCreateVertexArrays(1, &vao);
	glCreateBuffers(1, &vbo);

	glEnableVertexArrayAttrib(vao, 0);
	glVertexArrayAttribBinding(vao, 0, 0);
	glVertexArrayAttribFormat(vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
	glNamedBufferData(vbo, sizeof(glm::vec2), &mid, GL_DYNAMIC_DRAW);
	glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(glm::vec2));

	canvas.Image.BindFramebuffer();
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_DEPTH_TEST);

	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(RenderingSystem::Dot->Program);
	port.UploadMVP();
	port.BindMVPBuffer();
	glBindVertexArray(vao);
	glUniform2fv(2, 1, glm::value_ptr(size));
	if (!HideFill)
	{
		glBindTexture(GL_TEXTURE_2D, Fill.ColorTexture);
		glDrawArrays(GL_POINTS, 0, 1);
	}
	glBindTexture(GL_TEXTURE_2D, Drawing.ColorTexture);
	glDrawArrays(GL_POINTS, 0, 1);
	glBindTexture(GL_TEXTURE_2D, Overlay.ColorTexture);
	glDrawArrays(GL_POINTS, 0, 1);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
}

void TempLayers::ClearOverlay()
{
	Overlay.BindFramebuffer();
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TempLayers::GenCircleStroke()
{
	const int nSeg = 32;
	float r = 0.001f;
	std::vector<glm::vec2> line(nSeg + 1);
	for (int i = 0; i < nSeg; ++i)
	{
		float theta = static_cast<float>(i) / nSeg * 2 * glm::pi<float>();
		glm::vec2 p = {r * glm::sin(theta), r * glm::cos(theta)};
		line[i] = p;
	}
	line[nSeg] = line[0];
	Circle.Position = line;
	Circle.Thickness = 0.0003f;
	Circle.Color = {135.0f / 255, 206.0f / 255, 235.0f / 255, 1.0f};
	Circle.UpdateBuffers();
}
