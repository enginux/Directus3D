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

//= INCLUDES ===============
#include "../Math/Vector2.h"
#include "RHI_Definition.h"
//==========================

struct FfxFsr2Context;
struct FfxFsr2ContextDescription;
struct FfxFsr2DispatchDescription;

namespace Spartan
{
    class Camera;

    class RHI_FSR2
    {
    public:
        static void OnResolutionChange(RHI_Device* rhi_device, const Math::Vector2& resolution_render, const Math::Vector2& resolution_output);
        static void GenerateJitterSample(float* x, float* y);
        static void Dispatch(
            RHI_CommandList* cmd_list,
            RHI_Texture* tex_input,
            RHI_Texture* tex_depth,
            RHI_Texture* tex_velocity,
            RHI_Texture* tex_output,
            Camera* camera,
            float delta_time,
            float sharpness,
            bool reset
        );
        static void Destroy();

    private:
        static FfxFsr2Context m_ffx_fsr2_context;
        static FfxFsr2ContextDescription m_ffx_fsr2_context_description;
        static FfxFsr2DispatchDescription m_ffx_fsr2_dispatch_description;
    };
}
