/*
Copyright(c) 2016-2022 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES =======================
#include "pch.h"
#include "RHI_DescriptorSetLayout.h"
#include "RHI_ConstantBuffer.h"
#include "RHI_StructuredBuffer.h"
#include "RHI_Sampler.h"
#include "RHI_Texture.h"
#include "RHI_DescriptorSet.h"
#include "RHI_Device.h"
//==================================

//= NAMESPACES =====
using namespace std;
//==================

namespace Spartan
{
    RHI_DescriptorSetLayout::RHI_DescriptorSetLayout(RHI_Device* rhi_device, const vector<RHI_Descriptor>& descriptors, const string& name)
    {
        m_rhi_device  = rhi_device;
        m_descriptors = descriptors;
        m_name        = name;

        CreateResource(m_descriptors);

        for (const RHI_Descriptor& descriptor : m_descriptors)
        {
            m_hash = rhi_hash_combine(m_hash, descriptor.ComputeHash());
        }
    }

    void RHI_DescriptorSetLayout::SetConstantBuffer(const uint32_t slot, RHI_ConstantBuffer* constant_buffer)
    {
        for (RHI_Descriptor& descriptor : m_descriptors)
        {
            if ((descriptor.type == RHI_Descriptor_Type::ConstantBuffer) && descriptor.slot == slot + rhi_shader_shift_register_b)
            {
                // Determine if the descriptor set needs to bind (affects vkUpdateDescriptorSets)
                m_needs_to_bind = descriptor.data           != constant_buffer              ? true : m_needs_to_bind;
                m_needs_to_bind = descriptor.dynamic_offset != constant_buffer->GetOffset() ? true : m_needs_to_bind;
                m_needs_to_bind = descriptor.range          != constant_buffer->GetStride() ? true : m_needs_to_bind;

                descriptor.data           = static_cast<void*>(constant_buffer);
                descriptor.dynamic_offset = constant_buffer->GetOffset();
                descriptor.range          = constant_buffer->GetStride();

                return;
            }
        }
    }

    void RHI_DescriptorSetLayout::SetStructuredBuffer(const uint32_t slot, RHI_StructuredBuffer* structured_buffer)
    {
        for (RHI_Descriptor& descriptor : m_descriptors)
        {
            if ((descriptor.type == RHI_Descriptor_Type::StructuredBuffer) && descriptor.slot == slot + rhi_shader_shift_register_u)
            {
                // Determine if the descriptor set needs to bind (affects vkUpdateDescriptorSets)
                m_needs_to_bind = descriptor.data           != structured_buffer              ? true : m_needs_to_bind;
                m_needs_to_bind = descriptor.dynamic_offset != structured_buffer->GetOffset() ? true : m_needs_to_bind;
                m_needs_to_bind = descriptor.range          != structured_buffer->GetStride() ? true : m_needs_to_bind;

                descriptor.data           = static_cast<void*>(structured_buffer);
                descriptor.dynamic_offset = structured_buffer->GetOffset();
                descriptor.range          = structured_buffer->GetStride();

                return;
            }
        }
    }

    void RHI_DescriptorSetLayout::SetSampler(const uint32_t slot, RHI_Sampler* sampler)
    {
        for (RHI_Descriptor& descriptor : m_descriptors)
        {
            if (descriptor.type == RHI_Descriptor_Type::Sampler && descriptor.slot == slot + rhi_shader_shift_register_s)
            {
                // Determine if the descriptor set needs to bind (affects vkUpdateDescriptorSets)
                m_needs_to_bind = descriptor.data != sampler ? true : m_needs_to_bind;

                // Update
                descriptor.data = static_cast<void*>(sampler);

                return;
            }
        }
    }

    void RHI_DescriptorSetLayout::SetTexture(const uint32_t slot, RHI_Texture* texture, const uint32_t mip_index, const uint32_t mip_range)
    {
        bool mip_specified = mip_index != rhi_all_mips;
        RHI_Image_Layout layout = texture->GetLayout(mip_specified ? mip_index : 0);

         // Validate layout
        SP_ASSERT(layout == RHI_Image_Layout::General || layout == RHI_Image_Layout::Shader_Read_Only_Optimal || layout == RHI_Image_Layout::Depth_Stencil_Read_Only_Optimal);

        // Validate type
        SP_ASSERT(texture->IsSrv());

        for (RHI_Descriptor& descriptor : m_descriptors)
        {
            bool is_storage = layout == RHI_Image_Layout::General;
            bool match_type = descriptor.type == (is_storage ? RHI_Descriptor_Type::TextureStorage : RHI_Descriptor_Type::Texture);
            uint32_t shift  = is_storage ? rhi_shader_shift_register_u : rhi_shader_shift_register_t;
            bool match_slot = descriptor.slot == (slot + shift);

            if (match_type && match_slot)
            {
                // Determine if the descriptor set needs to bind (affects vkUpdateDescriptorSets)
                m_needs_to_bind = descriptor.data      != texture   ? true : m_needs_to_bind;
                m_needs_to_bind = descriptor.mip       != mip_index ? true : m_needs_to_bind;
                m_needs_to_bind = descriptor.mip_range != mip_range ? true : m_needs_to_bind;

                // Update
                descriptor.data      = static_cast<void*>(texture);
                descriptor.layout    = layout;
                descriptor.mip       = mip_index;
                descriptor.mip_range = mip_range;

                return;
            }
        }
    }

    void RHI_DescriptorSetLayout::ClearDescriptorData()
    {
        for (RHI_Descriptor& descriptor : m_descriptors)
        {
            descriptor.data = nullptr;
            descriptor.mip  = 0;
        }
    }

    RHI_DescriptorSet* RHI_DescriptorSetLayout::GetDescriptorSet()
    {
        RHI_DescriptorSet* descriptor_set = nullptr;

        // Integrate descriptor data into the hash
        uint64_t hash = m_hash;
        for (const RHI_Descriptor& descriptor : m_descriptors)
        {
            hash = rhi_hash_combine(hash, reinterpret_cast<uint64_t>(descriptor.data));
            hash = rhi_hash_combine(hash, static_cast<uint64_t>(descriptor.mip));
            hash = rhi_hash_combine(hash, static_cast<uint64_t>(descriptor.mip_range));
            hash = rhi_hash_combine(hash, static_cast<uint64_t>(descriptor.range));
        }

        // If we don't have a descriptor set to match that state, create one
        unordered_map<uint64_t, RHI_DescriptorSet>& descriptor_sets = m_rhi_device->GetDescriptorSets();
        const auto it = descriptor_sets.find(hash);
        if (it == descriptor_sets.end())
        {
            // Only allocate if the descriptor set cache hash enough capacity
            SP_ASSERT(m_rhi_device->HasDescriptorSetCapacity() && "Descriptor pool has no more memory to allocate another descriptor set");

            // Create descriptor set
            descriptor_sets[hash] = RHI_DescriptorSet(m_rhi_device, m_descriptors, this, m_name.c_str());

            // Out
            descriptor_set = &descriptor_sets[hash];
        }
        else if(m_needs_to_bind) // retrieve the existing one
        {
            descriptor_set  = &it->second;
            m_needs_to_bind = false;
        }

        return descriptor_set;
    }

    void RHI_DescriptorSetLayout::GetDynamicOffsets(vector<uint32_t>* offsets)
    {
        // Offsets should be ordered by the binding numbers in the descriptor set layouts

        (*offsets).clear();

        for (RHI_Descriptor& descriptor : m_descriptors)
        {
            if (descriptor.type == RHI_Descriptor_Type::StructuredBuffer || descriptor.type == RHI_Descriptor_Type::ConstantBuffer)
            {
                (*offsets).emplace_back(descriptor.dynamic_offset);
            }
        }
    }
}
