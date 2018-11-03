#include "FilterSepia.hpp"

namespace acid
{
	FilterSepia::FilterSepia(const GraphicsStage &graphicsStage) :
		IPostFilter(graphicsStage, {"Shaders/Filters/Default.vert", "Shaders/Filters/Sepia.frag"}, {})
	{
	}

	void FilterSepia::Render(const CommandBuffer &commandBuffer, const Vector4 &clipPlane, const ICamera &camera)
	{
		// Updates descriptors.
		m_descriptorSet.Push("writeColour", GetAttachment("writeColour", "resolved"));
		m_descriptorSet.Push("samplerColour", GetAttachment("samplerColour", "resolved"));
		bool updateSuccess = m_descriptorSet.Update(m_pipeline);

		if (!updateSuccess)
		{
			return;
		}

		// Draws the object.
		m_pipeline.BindPipeline(commandBuffer);

		m_descriptorSet.BindDescriptor(commandBuffer);
		m_model->CmdRender(commandBuffer);
	}
}
