#pragma once

#include "SysUtils.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <deque>

class DescriptorSetManager
{
    VkDevice _device = VK_NULL_HANDLE;
    VkDescriptorPool _pool = VK_NULL_HANDLE;
    VkDescriptorSet _set = VK_NULL_HANDLE;

    // Use deque to ensure pointers remain valid when adding new elements
    std::deque<VkDescriptorBufferInfo> _bufferInfos;
    std::deque<VkDescriptorImageInfo> _imageInfos;
    std::vector<VkWriteDescriptorSet> _writes;

  public:
    DescriptorSetManager() = default;

    bool Initialize(VkDevice device, VkDescriptorSetLayout layout, uint32_t numSrv, uint32_t numUav, uint32_t numCbv)
    {
        _device = device;
        _bufferInfos.clear();
        _imageInfos.clear();
        _writes.clear();

        std::vector<VkDescriptorPoolSize> poolSizes;

        // We allocate enough pool sizes for potential types mapping to SRV/UAV/CBV
        // SRV can be Sampled Image, Texture Buffer, Uniform Texel Buffer, or Storage Buffer (Read Only)
        if (numSrv > 0)
        {
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, numSrv });
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, numSrv });
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numSrv });
        }

        // UAV can be Storage Image, Storage Texel Buffer, Storage Buffer
        if (numUav > 0)
        {
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numUav });
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, numUav });
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numUav });
        }

        // CBV is Uniform Buffer
        if (numCbv > 0)
        {
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numCbv });
        }

        if (poolSizes.empty())
            return false;

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_pool) != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create descriptor pool");
            return false;
        }

        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        if (vkAllocateDescriptorSets(_device, &allocInfo, &_set) != VK_SUCCESS)
        {
            LOG_ERROR("Failed to allocate descriptor set");
            return false;
        }

        return true;
    }

    void SetBuffer(uint32_t binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset = 0)
    {
        VkDescriptorBufferInfo info {};
        info.buffer = buffer;
        info.offset = offset;
        info.range = size;

        _bufferInfos.push_back(info);

        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = _set;
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = type;
        write.descriptorCount = 1;
        write.pBufferInfo = &_bufferInfos.back();

        _writes.push_back(write);
    }

    void SetImage(uint32_t binding, VkDescriptorType type, VkImageView view, VkImageLayout layout,
                  VkSampler sampler = VK_NULL_HANDLE)
    {
        VkDescriptorImageInfo info {};
        info.imageView = view;
        info.imageLayout = layout;
        info.sampler = sampler;

        _imageInfos.push_back(info);

        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = _set;
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = type;
        write.descriptorCount = 1;
        write.pImageInfo = &_imageInfos.back();

        _writes.push_back(write);
    }

    void Update()
    {
        if (!_writes.empty())
        {
            vkUpdateDescriptorSets(_device, static_cast<uint32_t>(_writes.size()), _writes.data(), 0, nullptr);
            _writes.clear();
            _bufferInfos.clear();
            _imageInfos.clear();
        }
    }

    VkDescriptorSet GetSet() const { return _set; }
    VkDescriptorPool GetPool() const { return _pool; }

    void Release()
    {
        if (_pool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(_device, _pool, nullptr);
            _pool = VK_NULL_HANDLE;
        }
    }

    ~DescriptorSetManager() { Release(); }
};
