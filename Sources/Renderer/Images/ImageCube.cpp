﻿#include "ImageCube.hpp"

#include "Renderer/Buffers/Buffer.hpp"
#include "Renderer/Renderer.hpp"
#include "Resources/Resources.hpp"
#include "Image.hpp"

namespace acid
{
std::shared_ptr<ImageCube> ImageCube::Create(const Metadata &metadata)
{
	auto resource = Resources::Get()->Find(metadata);

	if (resource != nullptr)
	{
		return std::dynamic_pointer_cast<ImageCube>(resource);
	}

	auto result = std::make_shared<ImageCube>("");
	Resources::Get()->Add(metadata, std::dynamic_pointer_cast<Resource>(result));
	result->Decode(metadata);
	result->Load();
	return result;
}

std::shared_ptr<ImageCube> ImageCube::Create(const std::string &filename, const std::string &fileSuffix, const VkFilter &filter, const VkSamplerAddressMode &addressMode,
	const bool &anisotropic, const bool &mipmap)
{
	auto temp = ImageCube(filename, fileSuffix, filter, addressMode, anisotropic, mipmap, false);
	Metadata metadata = Metadata();
	temp.Encode(metadata);
	return Create(metadata);
}

ImageCube::ImageCube(std::string filename, std::string fileSuffix, const VkFilter &filter, const VkSamplerAddressMode &addressMode, const bool &anisotropic, const bool &mipmap,
	const bool &load) :
	m_filename(std::move(filename)),
	m_fileSuffix(std::move(fileSuffix)),
	m_fileSides(std::vector<std::string>{ "Right", "Left", "Top", "Bottom", "Back", "Front" }),
	m_filter(filter),
	m_addressMode(addressMode),
	m_anisotropic(anisotropic),
	m_mipmap(mipmap),
	m_samples(VK_SAMPLE_COUNT_1_BIT),
	m_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
	m_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
	m_components(0),
	m_loadPixels(nullptr),
	m_mipLevels(0),
	m_image(VK_NULL_HANDLE),
	m_memory(VK_NULL_HANDLE),
	m_sampler(VK_NULL_HANDLE),
	m_view(VK_NULL_HANDLE),
	m_format(VK_FORMAT_R8G8B8A8_UNORM)
{
	if (load)
	{
		ImageCube::Load();
	}
}

ImageCube::ImageCube(const Vector2ui &extent, std::unique_ptr<uint8_t[]> pixels, const VkFormat &format, const VkImageLayout &layout,
	const VkImageUsageFlags &usage, const VkFilter &filter, const VkSamplerAddressMode &addressMode, const VkSampleCountFlagBits &samples, const bool &anisotropic,
	const bool &mipmap) :
	m_filename(""),
	m_fileSuffix(""),
	m_filter(filter),
	m_addressMode(addressMode),
	m_anisotropic(anisotropic),
	m_mipmap(mipmap),
	m_samples(samples),
	m_layout(layout),
	m_usage(usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
	m_components(4),
	m_extent(extent),
	m_loadPixels(std::move(pixels)),
	m_mipLevels(0),
	m_image(VK_NULL_HANDLE),
	m_memory(VK_NULL_HANDLE),
	m_sampler(VK_NULL_HANDLE),
	m_view(VK_NULL_HANDLE),
	m_format(format)
{
	ImageCube::Load();
}

ImageCube::~ImageCube()
{
	auto logicalDevice = Renderer::Get()->GetLogicalDevice();

	vkDestroyImageView(*logicalDevice, m_view, nullptr);
	vkDestroySampler(*logicalDevice, m_sampler, nullptr);
	vkFreeMemory(*logicalDevice, m_memory, nullptr);
	vkDestroyImage(*logicalDevice, m_image, nullptr);
}

VkDescriptorSetLayoutBinding ImageCube::GetDescriptorSetLayout(const uint32_t &binding, const VkDescriptorType &descriptorType, const VkShaderStageFlags &stage,
	const uint32_t &count)
{
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
	descriptorSetLayoutBinding.binding = binding;
	descriptorSetLayoutBinding.descriptorType = descriptorType;
	descriptorSetLayoutBinding.descriptorCount = 1;
	descriptorSetLayoutBinding.stageFlags = stage;
	descriptorSetLayoutBinding.pImmutableSamplers = nullptr;
	return descriptorSetLayoutBinding;
}

WriteDescriptorSet ImageCube::GetWriteDescriptor(const uint32_t &binding, const VkDescriptorType &descriptorType, const std::optional<OffsetSize> &offsetSize) const
{
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.sampler = m_sampler;
	imageInfo.imageView = m_view;
	imageInfo.imageLayout = m_layout;

	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = VK_NULL_HANDLE; // Will be set in the descriptor handler.
	descriptorWrite.dstBinding = binding;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.descriptorType = descriptorType;
	//descriptorWrite.pImageInfo = &imageInfo;
	return WriteDescriptorSet(descriptorWrite, imageInfo);
}

void ImageCube::Load()
{
	if (!m_filename.empty() && m_loadPixels == nullptr)
	{
#if defined(ACID_VERBOSE)
		auto debugStart = Engine::GetTime();
#endif
		m_loadPixels = LoadPixels(m_filename, m_fileSuffix, m_fileSides, m_extent, m_components, m_format);
#if defined(ACID_VERBOSE)
		auto debugEnd = Engine::GetTime();
		Log::Out("Image Cube '%s' loaded in %.3fms\n", m_filename.c_str(), (debugEnd - debugStart).AsMilliseconds<float>());
#endif
	}

	if (m_extent.m_x == 0 || m_extent.m_y == 0)
	{
		return;
	}

	m_mipLevels = m_mipmap ? Image::GetMipLevels({ m_extent.m_x, m_extent.m_y, 1 }) : 1;

	Image::CreateImage(m_image, m_memory, { m_extent.m_x, m_extent.m_y, 1 }, m_format, m_samples, VK_IMAGE_TILING_OPTIMAL, m_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_mipLevels, 6,
		VK_IMAGE_TYPE_2D);
	Image::CreateImageSampler(m_sampler, m_filter, m_addressMode, m_anisotropic, m_mipLevels);
	Image::CreateImageView(m_image, m_view, VK_IMAGE_VIEW_TYPE_CUBE, m_format, VK_IMAGE_ASPECT_COLOR_BIT, m_mipLevels, 0, 6, 0);

	if (m_loadPixels != nullptr || m_mipmap)
	{
		Image::TransitionImageLayout(m_image, m_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, m_mipLevels, 0, 6, 0);
	}

	if (m_loadPixels != nullptr)
	{
		auto bufferStaging = Buffer(m_extent.m_x * m_extent.m_y * m_components * 6, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void *data;
		bufferStaging.MapMemory(&data);
		std::memcpy(data, m_loadPixels.get(), bufferStaging.GetSize());
		bufferStaging.UnmapMemory();

		Image::CopyBufferToImage(bufferStaging.GetBuffer(), m_image, { m_extent.m_x, m_extent.m_y, 1 }, 6, 0);
	}

	if (m_mipmap)
	{
		Image::CreateMipmaps(m_image, { m_extent.m_x, m_extent.m_y, 1 }, m_format, m_layout, m_mipLevels, 0, 6);
	}
	else if (m_loadPixels != nullptr)
	{
		Image::TransitionImageLayout(m_image, m_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_layout, VK_IMAGE_ASPECT_COLOR_BIT, m_mipLevels, 0, 6, 0);
	}
	else
	{
		Image::TransitionImageLayout(m_image, m_format, VK_IMAGE_LAYOUT_UNDEFINED, m_layout, VK_IMAGE_ASPECT_COLOR_BIT, m_mipLevels, 0, 6, 0);
	}

	m_loadPixels = nullptr;
}

void ImageCube::Decode(const Metadata &metadata)
{
	metadata.GetChild("Filename", m_filename);
	metadata.GetChild("Suffix", m_fileSuffix);
	//metadata.GetChild("Sides", m_fileSides);
	metadata.GetChild("Filter", m_filter);
	metadata.GetChild("Address Mode", m_addressMode);
	metadata.GetChild("Anisotropic", m_anisotropic);
	metadata.GetChild("Mipmap", m_mipmap);
}

void ImageCube::Encode(Metadata &metadata) const
{
	metadata.SetChild("Filename", m_filename);
	metadata.SetChild("Suffix", m_fileSuffix);
	//metadata.SetChild("Sides", m_fileSides);
	metadata.SetChild("Filter", m_filter);
	metadata.SetChild("Address Mode", m_addressMode);
	metadata.SetChild("Anisotropic", m_anisotropic);
	metadata.SetChild("Mipmap", m_mipmap);
}

std::unique_ptr<uint8_t[]> ImageCube::GetPixels(VkExtent3D &extent, const uint32_t &mipLevel, const uint32_t &arrayLayer) const
{
	auto logicalDevice = Renderer::Get()->GetLogicalDevice();

	extent.width = int32_t(m_extent.m_x >> mipLevel);
	extent.height = int32_t(m_extent.m_y >> mipLevel);
	extent.depth = 1;

	VkImage dstImage;
	VkDeviceMemory dstImageMemory;
	Image::CopyImage(m_image, dstImage, dstImageMemory, m_format, extent, m_layout, mipLevel, arrayLayer);

	VkImageSubresource dstImageSubresource = {};
	dstImageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	dstImageSubresource.mipLevel = 0;
	dstImageSubresource.arrayLayer = 0;

	VkSubresourceLayout dstSubresourceLayout;
	vkGetImageSubresourceLayout(*logicalDevice, dstImage, &dstImageSubresource, &dstSubresourceLayout);

	auto result = std::make_unique<uint8_t[]>(dstSubresourceLayout.size);

	void *data;
	vkMapMemory(*logicalDevice, dstImageMemory, dstSubresourceLayout.offset, dstSubresourceLayout.size, 0, &data);
	std::memcpy(result.get(), data, static_cast<size_t>(dstSubresourceLayout.size));
	vkUnmapMemory(*logicalDevice, dstImageMemory);

	vkFreeMemory(*logicalDevice, dstImageMemory, nullptr);
	vkDestroyImage(*logicalDevice, dstImage, nullptr);

	return result;
}

std::unique_ptr<uint8_t[]> ImageCube::GetPixels(VkExtent3D &extent, const uint32_t &mipLevel) const
{
	std::unique_ptr<uint8_t[]> pixels = nullptr;
	uint8_t *offset = nullptr;

	for (uint32_t i = 0; i < 6; i++)
	{
		auto resultSide = GetPixels(extent, mipLevel, i);
		int32_t sizeSide = extent.width * extent.height * m_components;

		if (pixels == nullptr)
		{
			pixels = std::make_unique<uint8_t[]>(sizeSide * 6);
			offset = pixels.get();
		}

		memcpy(offset, resultSide.get(), sizeSide);
		offset += sizeSide;
	}

	extent.height *= 6;
	return pixels;
}

void ImageCube::SetPixels(const uint8_t *pixels, const uint32_t &layerCount, const uint32_t &baseArrayLayer)
{
	Buffer bufferStaging = Buffer(m_extent.m_x * m_extent.m_y * m_components * 6, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void *data;
	bufferStaging.MapMemory(&data);
	memcpy(data, pixels, bufferStaging.GetSize());
	bufferStaging.UnmapMemory();

	Image::CopyBufferToImage(bufferStaging.GetBuffer(), m_image, { m_extent.m_x, m_extent.m_y, 1 }, layerCount, baseArrayLayer);
}

std::unique_ptr<uint8_t[]> ImageCube::LoadPixels(const std::string &filename, const std::string &fileSuffix, const std::vector<std::string> &fileSides, Vector2ui &extent,
	uint32_t &components, VkFormat &format)
{
	std::unique_ptr<uint8_t[]> result = nullptr;
	uint8_t *offset = nullptr;

	for (const auto &side : fileSides)
	{
		std::string filenameSide = std::string(filename).append("/").append(side).append(fileSuffix);
		auto resultSide = Image::LoadPixels(filenameSide, extent, components, format);
		int32_t sizeSide = extent.m_x * extent.m_y * components;

		if (result == nullptr)
		{
			result = std::make_unique<uint8_t[]>(sizeSide * fileSides.size());
			offset = result.get();
		}

		memcpy(offset, resultSide.get(), sizeSide);
		offset += sizeSide;
	}

	return result;
}
}
