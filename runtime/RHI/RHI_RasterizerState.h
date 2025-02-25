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
#include <memory>
#include "RHI_Definition.h"
#include "../Core/Object.h"
//================================

namespace Spartan
{
    class SP_CLASS RHI_RasterizerState : public Object
    {
    public:
        RHI_RasterizerState() = default;
        RHI_RasterizerState(
            RHI_Device* rhi_device,
            const RHI_CullMode cull_mode,
            const RHI_PolygonMode fill_mode,
            const bool depth_clip_enabled,
            const bool scissor_enabled,
            const bool antialised_line_enabled,
            const float depth_bias              = 0.0f,
            const float depth_bias_clamp        = 0.0f,
            const float depth_bias_slope_scaled = 0.0f,
            const float line_width              = 1.0f
        );
        ~RHI_RasterizerState();

        RHI_CullMode GetCullMode()       const { return m_cull_mode; }
        RHI_PolygonMode GetPolygonMode() const { return m_polygon_mode; }
        bool GetDepthClipEnabled()       const { return m_depth_clip_enabled; }
        bool GetScissorEnabled()         const { return m_scissor_enabled; }
        bool GetAntialisedLineEnabled()  const { return m_antialised_line_enabled; }
        void* GetRhiResource()           const { return m_rhi_resource; }
        float GetLineWidth()             const { return m_line_width; }
        float GetDepthBias()             const { return m_depth_bias; }
        float GetDepthBiasClamp()        const { return m_depth_bias_clamp; }
        float GetDepthBiasSlopeScaled()  const { return m_depth_bias_slope_scaled; }

        bool operator==(const RHI_RasterizerState& rhs) const
        {
            return
                m_cull_mode               == rhs.GetCullMode()              &&
                m_polygon_mode            == rhs.GetPolygonMode()           &&
                m_depth_clip_enabled      == rhs.GetDepthClipEnabled()      &&
                m_scissor_enabled         == rhs.GetScissorEnabled()        &&
                m_antialised_line_enabled == rhs.GetAntialisedLineEnabled() &&
                m_line_width              == rhs.GetLineWidth()             &&
                m_depth_bias              == rhs.GetDepthBias()             &&
                m_depth_bias_clamp        == rhs.GetDepthBiasClamp()        &&
                m_depth_bias_slope_scaled == rhs.GetDepthBiasSlopeScaled();
        }

    private:
        RHI_CullMode m_cull_mode        = RHI_CullMode::Undefined;
        RHI_PolygonMode m_polygon_mode  = RHI_PolygonMode::Undefined;
        bool m_depth_clip_enabled       = false;
        bool m_scissor_enabled          = false;
        bool m_antialised_line_enabled  = false;
        float m_depth_bias              = 0.0f;
        float m_depth_bias_clamp        = 0.0f;
        float m_depth_bias_slope_scaled = 0.0f;
        float m_line_width              = 1.0f;
        
        void* m_rhi_resource = nullptr;
    };
}
