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

#pragma once

//= INCLUDES =====================
#include <vector>
#include "../Core/Object.h"
//================================

namespace Spartan
{
    class RHI_IndexBuffer : public Object
    {
    public:
        RHI_IndexBuffer() = default;
        RHI_IndexBuffer(RHI_Device* rhi_device, bool is_mappable, const char* name)
        {
            m_rhi_device  = rhi_device;
            m_is_mappable = is_mappable;
            m_name = name;
        }

        ~RHI_IndexBuffer()
        {
            _destroy();
        }

        template<typename T>
        void Create(const std::vector<T>& indices)
        {
            m_stride          = sizeof(T);
            m_index_count     = static_cast<uint32_t>(indices.size());
            m_object_size_gpu = static_cast<uint64_t>(m_stride * m_index_count);

            _create(static_cast<const void*>(indices.data()));
        }

        template<typename T>
        void Create(const T* indices, const uint32_t index_count)
        {
            m_stride          = sizeof(T);
            m_index_count     = index_count;
            m_object_size_gpu = static_cast<uint64_t>(m_stride * m_index_count);

            _create(static_cast<const void*>(indices));
        }

        template<typename T>
        void CreateDynamic(const uint32_t index_count)
        {
            m_stride          = sizeof(T);
            m_index_count     = index_count;
            m_object_size_gpu = static_cast<uint64_t>(m_stride * m_index_count);

            _create(nullptr);
        }

        void* Map();
        void Unmap();

        void* GetRhiResource()   const { return m_rhi_resource; }
        uint32_t GetIndexCount() const { return m_index_count; }
        bool Is16Bit()           const { return sizeof(uint16_t) == m_stride; }
        bool Is32Bit()           const { return sizeof(uint32_t) == m_stride; }

    private:
        void _create(const void* indices);
        void _destroy();

        void* m_mapped_data      = nullptr;
        bool m_is_mappable       = false;
        uint32_t m_stride        = 0;
        uint32_t m_index_count   = 0;
        RHI_Device* m_rhi_device = nullptr;

        // RHI Resources
        void* m_rhi_resource = nullptr;
    };
}
