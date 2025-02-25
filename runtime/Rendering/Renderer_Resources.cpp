#/*
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

//= INCLUDES ============================
#include "pch.h"
#include "Renderer.h"
#include "Geometry.h"
#include "Grid.h"
#include "Font/Font.h"
#include "../Resource/ResourceCache.h"
#include "../RHI/RHI_Texture2D.h"
#include "../RHI/RHI_Texture2DArray.h"
#include "../RHI/RHI_Shader.h"
#include "../RHI/RHI_Sampler.h"
#include "../RHI/RHI_BlendState.h"
#include "../RHI/RHI_ConstantBuffer.h"
#include "../RHI/RHI_RasterizerState.h"
#include "../RHI/RHI_DepthStencilState.h"
#include "../RHI/RHI_SwapChain.h"
#include "../RHI/RHI_StructuredBuffer.h"
#include "../RHI/RHI_VertexBuffer.h"
#include "../RHI/RHI_IndexBuffer.h"
#include "../RHI/RHI_TextureCube.h"
#include "../RHI/RHI_Device.h"
#include "../RHI/RHI_FSR2.h"
//=======================================

//= NAMESPACES ===============
using namespace std;
using namespace Spartan::Math;
//============================

#define render_target(rt_enum) m_render_targets[static_cast<uint8_t>(rt_enum)]
#define shader(rt_enum)        m_shaders[static_cast<uint8_t>(rt_enum)]

namespace Spartan
{
    void Renderer::CreateConstantBuffers()
    {
        SP_ASSERT(m_rhi_device != nullptr);

        // buffers can dynamically re-allocate anyway, no need to go bigger.
        const uint32_t offset_count = 8192;

        m_cb_frame_gpu = make_shared<RHI_ConstantBuffer>(m_rhi_device.get(), "frame");
        m_cb_frame_gpu->Create<Cb_Frame>(offset_count);

        m_cb_uber_gpu = make_shared<RHI_ConstantBuffer>(m_rhi_device.get(), "uber");
        m_cb_uber_gpu->Create<Cb_Uber>(offset_count);

        m_cb_light_gpu = make_shared<RHI_ConstantBuffer>(m_rhi_device.get(), "light");
        m_cb_light_gpu->Create<Cb_Light>(offset_count);

        m_cb_material_gpu = make_shared<RHI_ConstantBuffer>(m_rhi_device.get(), "material");
        m_cb_material_gpu->Create<Cb_Material>(4096); // Nvidia failed to allocate beyond this point
    }

    void Renderer::CreateStructuredBuffers()
    {
        const uint32_t offset_count = 32;
        m_sb_spd_counter = make_shared<RHI_StructuredBuffer>(m_rhi_device.get(), static_cast<uint32_t>(sizeof(uint32_t)), offset_count, "spd_counter");
    }

    void Renderer::CreateDepthStencilStates()
    {
        SP_ASSERT(m_rhi_device != nullptr);

        RHI_Comparison_Function reverse_z_aware_comp_func = GetOption<bool>(RendererOption::ReverseZ) ? RHI_Comparison_Function::GreaterEqual : RHI_Comparison_Function::LessEqual;

        // arguments: depth_test, depth_write, depth_function, stencil_test, stencil_write, stencil_function
        m_depth_stencil_off_off = make_shared<RHI_DepthStencilState>(m_rhi_device.get(), false, false, RHI_Comparison_Function::Never, false, false, RHI_Comparison_Function::Never);  // no depth or stencil
        m_depth_stencil_rw_off  = make_shared<RHI_DepthStencilState>(m_rhi_device.get(), true,  true,  reverse_z_aware_comp_func,      false, false, RHI_Comparison_Function::Never);  // depth
        m_depth_stencil_r_off   = make_shared<RHI_DepthStencilState>(m_rhi_device.get(), true,  false, reverse_z_aware_comp_func,      false, false, RHI_Comparison_Function::Never);  // depth
        m_depth_stencil_off_r   = make_shared<RHI_DepthStencilState>(m_rhi_device.get(), false, false, RHI_Comparison_Function::Never, true,  false, RHI_Comparison_Function::Equal);  // depth + stencil
        m_depth_stencil_rw_w    = make_shared<RHI_DepthStencilState>(m_rhi_device.get(), true,  true,  reverse_z_aware_comp_func,      false, true,  RHI_Comparison_Function::Always); // depth + stencil
    }

    void Renderer::CreateRasterizerStates()
    {
        SP_ASSERT(m_rhi_device != nullptr);

        float depth_bias              = GetOption<bool>(RendererOption::ReverseZ) ? -m_depth_bias : m_depth_bias;
        float depth_bias_slope_scaled = GetOption<bool>(RendererOption::ReverseZ) ? -m_depth_bias_slope_scaled : m_depth_bias_slope_scaled;

        m_rasterizer_cull_back_solid     = make_shared<RHI_RasterizerState>(m_rhi_device.get(), RHI_CullMode::Back, RHI_PolygonMode::Solid,     true,  false, false);
        m_rasterizer_cull_back_wireframe = make_shared<RHI_RasterizerState>(m_rhi_device.get(), RHI_CullMode::Back, RHI_PolygonMode::Wireframe, true,  false, true);
        m_rasterizer_cull_none_solid     = make_shared<RHI_RasterizerState>(m_rhi_device.get(), RHI_CullMode::Back, RHI_PolygonMode::Solid,     true, false, false);
        m_rasterizer_light_point_spot    = make_shared<RHI_RasterizerState>(m_rhi_device.get(), RHI_CullMode::Back, RHI_PolygonMode::Solid,     true,  false, false, depth_bias,        m_depth_bias_clamp, depth_bias_slope_scaled);
        m_rasterizer_light_directional   = make_shared<RHI_RasterizerState>(m_rhi_device.get(), RHI_CullMode::Back, RHI_PolygonMode::Solid,     false, false, false, depth_bias * 0.1f, m_depth_bias_clamp, depth_bias_slope_scaled);
    }

    void Renderer::CreateBlendStates()
    {
        SP_ASSERT(m_rhi_device != nullptr);

        // blend_enabled, source_blend, dest_blend, blend_op, source_blend_alpha, dest_blend_alpha, blend_op_alpha, blend_factor
        m_blend_disabled = make_shared<RHI_BlendState>(m_rhi_device.get(), false);
        m_blend_alpha    = make_shared<RHI_BlendState>(m_rhi_device.get(), true, RHI_Blend::Src_Alpha, RHI_Blend::Inv_Src_Alpha, RHI_Blend_Operation::Add, RHI_Blend::One, RHI_Blend::One, RHI_Blend_Operation::Add, 0.0f);
        m_blend_additive = make_shared<RHI_BlendState>(m_rhi_device.get(), true, RHI_Blend::One,       RHI_Blend::One,           RHI_Blend_Operation::Add, RHI_Blend::One, RHI_Blend::One, RHI_Blend_Operation::Add, 1.0f);
    }

    void Renderer::CreateSamplers(const bool create_only_anisotropic /*= false*/)
    {
        SP_ASSERT(m_rhi_device != nullptr);

        float anisotropy                         = GetOption<float>(RendererOption::Anisotropy);
        RHI_Comparison_Function depth_comparison = GetOption<bool>(RendererOption::ReverseZ) ? RHI_Comparison_Function::Greater : RHI_Comparison_Function::Less;

        // Compute mip bias
        float mip_bias = 0.0f;
        if (m_resolution_output.x > m_resolution_render.x)
        {
            // Progressively negative values when upsampling for increased texture fidelity
            mip_bias = log2(m_resolution_render.x / m_resolution_output.x) - 1.0f;
        }

        // sampler parameters: minification, magnification, mip, sampler address mode, comparison, anisotropy, comparison enabled, mip lod bias
        if (!create_only_anisotropic)
        {
            m_sampler_compare_depth   = make_shared<RHI_Sampler>(m_rhi_device, RHI_Filter::Linear,  RHI_Filter::Linear,  RHI_Sampler_Mipmap_Mode::Nearest, RHI_Sampler_Address_Mode::Clamp, depth_comparison, 0.0f, true);
            m_sampler_point_clamp     = make_shared<RHI_Sampler>(m_rhi_device, RHI_Filter::Nearest, RHI_Filter::Nearest, RHI_Sampler_Mipmap_Mode::Nearest, RHI_Sampler_Address_Mode::Clamp, RHI_Comparison_Function::Always);
            m_sampler_point_wrap      = make_shared<RHI_Sampler>(m_rhi_device, RHI_Filter::Nearest, RHI_Filter::Nearest, RHI_Sampler_Mipmap_Mode::Nearest, RHI_Sampler_Address_Mode::Wrap,  RHI_Comparison_Function::Always);
            m_sampler_bilinear_clamp  = make_shared<RHI_Sampler>(m_rhi_device, RHI_Filter::Linear,  RHI_Filter::Linear,  RHI_Sampler_Mipmap_Mode::Nearest, RHI_Sampler_Address_Mode::Clamp, RHI_Comparison_Function::Always);
            m_sampler_bilinear_wrap   = make_shared<RHI_Sampler>(m_rhi_device, RHI_Filter::Linear,  RHI_Filter::Linear,  RHI_Sampler_Mipmap_Mode::Nearest, RHI_Sampler_Address_Mode::Wrap,  RHI_Comparison_Function::Always);
            m_sampler_trilinear_clamp = make_shared<RHI_Sampler>(m_rhi_device, RHI_Filter::Linear,  RHI_Filter::Linear,  RHI_Sampler_Mipmap_Mode::Linear,  RHI_Sampler_Address_Mode::Clamp, RHI_Comparison_Function::Always);
        }

        m_sampler_anisotropic_wrap = make_shared<RHI_Sampler>(m_rhi_device, RHI_Filter::Linear, RHI_Filter::Linear, RHI_Sampler_Mipmap_Mode::Linear, RHI_Sampler_Address_Mode::Wrap, RHI_Comparison_Function::Always, anisotropy, false, mip_bias);

        SP_LOG_INFO("Mip bias set to %f", mip_bias);
    }

    void Renderer::CreateRenderTextures(const bool create_render, const bool create_output, const bool create_fixed, const bool create_dynamic)
    {
        // Get render resolution
        uint32_t width_render  = static_cast<uint32_t>(m_resolution_render.x);
        uint32_t height_render = static_cast<uint32_t>(m_resolution_render.y);

        // Get output resolution
        uint32_t width_output  = static_cast<uint32_t>(m_resolution_output.x);
        uint32_t height_output = static_cast<uint32_t>(m_resolution_output.y);

        // Deduce how many mips are required to scale down any dimension close to 16px (or exactly)
        uint32_t mip_count           = 1;
        uint32_t width               = width_render;
        uint32_t height              = height_render;
        uint32_t smallest_dimension  = 1;
        while (width > smallest_dimension && height > smallest_dimension)
        {
            width /= 2;
            height /= 2;
            mip_count++;
        }

        // Notes.
        // Gbuffer_Normal: Any format with or below 8 bits per channel, will produce banding.

        // Render resolution
        if (create_render)
        {
            // Frame (HDR) - Mips are used to emulate roughness when blending with transparent surfaces
            render_target(RendererTexture::Frame_Render)   = make_unique<RHI_Texture2D>(m_context, width_render, height_render, mip_count, RHI_Format_R16G16B16A16_Float, RHI_Texture_Rt_Color | RHI_Texture_Uav | RHI_Texture_Srv | RHI_Texture_PerMipViews | RHI_Texture_ClearOrBlit, "rt_frame_render");
            render_target(RendererTexture::Frame_Render_2) = make_unique<RHI_Texture2D>(m_context, width_render, height_render, mip_count, RHI_Format_R16G16B16A16_Float, RHI_Texture_Rt_Color | RHI_Texture_Uav | RHI_Texture_Srv | RHI_Texture_PerMipViews | RHI_Texture_ClearOrBlit, "rt_frame_render_2");

            // G-Buffer
            render_target(RendererTexture::Gbuffer_Albedo)   = make_shared<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_R8G8B8A8_Unorm,     RHI_Texture_Rt_Color        | RHI_Texture_Srv,                                       "rt_gbuffer_albedo");
            render_target(RendererTexture::Gbuffer_Normal)   = make_shared<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_R16G16B16A16_Float, RHI_Texture_Rt_Color        | RHI_Texture_Srv,                                       "rt_gbuffer_normal");
            render_target(RendererTexture::Gbuffer_Material) = make_shared<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_R8G8B8A8_Unorm,     RHI_Texture_Rt_Color        | RHI_Texture_Srv,                                       "rt_gbuffer_material");
            render_target(RendererTexture::Gbuffer_Velocity) = make_shared<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_R16G16_Float,       RHI_Texture_Rt_Color        | RHI_Texture_Srv,                                       "rt_gbuffer_velocity");
            render_target(RendererTexture::Gbuffer_Depth)    = make_shared<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_D32_Float,          RHI_Texture_Rt_DepthStencil | RHI_Texture_Rt_DepthStencilReadOnly | RHI_Texture_Srv, "rt_gbuffer_depth");

            // Light
            render_target(RendererTexture::Light_Diffuse)              = make_unique<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_R11G11B10_Float, RHI_Texture_Uav | RHI_Texture_Srv | RHI_Texture_ClearOrBlit, "rt_light_diffuse");
            render_target(RendererTexture::Light_Diffuse_Transparent)  = make_unique<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_R11G11B10_Float, RHI_Texture_Uav | RHI_Texture_Srv | RHI_Texture_ClearOrBlit, "rt_light_diffuse_transparent");
            render_target(RendererTexture::Light_Specular)             = make_unique<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_R11G11B10_Float, RHI_Texture_Uav | RHI_Texture_Srv | RHI_Texture_ClearOrBlit, "rt_light_specular");
            render_target(RendererTexture::Light_Specular_Transparent) = make_unique<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_R11G11B10_Float, RHI_Texture_Uav | RHI_Texture_Srv | RHI_Texture_ClearOrBlit, "rt_light_specular_transparent");
            render_target(RendererTexture::Light_Volumetric)           = make_unique<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_R11G11B10_Float, RHI_Texture_Uav | RHI_Texture_Srv | RHI_Texture_ClearOrBlit, "rt_light_volumetric");

            // SSR - Mips are used to emulate roughness for surfaces which require it
            render_target(RendererTexture::Ssr) = make_shared<RHI_Texture2D>(m_context, width_render, height_render, mip_count, RHI_Format_R16G16B16A16_Float, RHI_Texture_Uav | RHI_Texture_Srv | RHI_Texture_PerMipViews, "rt_ssr");

            // SSAO
            render_target(RendererTexture::Ssao)    = make_unique<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_R16G16B16A16_Snorm, RHI_Texture_Uav | RHI_Texture_Srv, "rt_ssao");
            render_target(RendererTexture::Ssao_Gi) = make_unique<RHI_Texture2D>(m_context, width_render, height_render, 1, RHI_Format_R16G16B16A16_Snorm, RHI_Texture_Uav | RHI_Texture_Srv, "rt_ssao_gi");

            // Dof
            render_target(RendererTexture::Dof_Half)   = make_unique<RHI_Texture2D>(m_context, width_render / 2, height_render / 2, 1, RHI_Format_R16G16B16A16_Float, RHI_Texture_Uav | RHI_Texture_Srv, "rt_dof_half");
            render_target(RendererTexture::Dof_Half_2) = make_unique<RHI_Texture2D>(m_context, width_render / 2, height_render / 2, 1, RHI_Format_R16G16B16A16_Float, RHI_Texture_Uav | RHI_Texture_Srv, "rt_dof_half_2");
        }

        // Output resolution
        if (create_output)
        {
            // Frame (LDR)
            render_target(RendererTexture::Frame_Output)   = make_unique<RHI_Texture2D>(m_context, width_output, height_output, 1, RHI_Format_R16G16B16A16_Float, RHI_Texture_Rt_Color | RHI_Texture_Uav | RHI_Texture_Srv | RHI_Texture_ClearOrBlit, "rt_frame_output");
            render_target(RendererTexture::Frame_Output_2) = make_unique<RHI_Texture2D>(m_context, width_output, height_output, 1, RHI_Format_R16G16B16A16_Float, RHI_Texture_Rt_Color | RHI_Texture_Uav | RHI_Texture_Srv | RHI_Texture_ClearOrBlit, "rt_frame_output_2");

            // Bloom
            render_target(RendererTexture::Bloom) = make_shared<RHI_Texture2D>(m_context, width_output, height_output, mip_count, RHI_Format_R11G11B10_Float, RHI_Texture_Uav | RHI_Texture_Srv | RHI_Texture_PerMipViews, "rt_bloom");
        }

        // Fixed resolution
        if (create_fixed)
        {
            render_target(RendererTexture::Brdf_Specular_Lut) = make_unique<RHI_Texture2D>(m_context, 400, 400, 1, RHI_Format_R8G8_Unorm, RHI_Texture_Uav | RHI_Texture_Srv, "rt_brdf_specular_lut");
            m_brdf_specular_lut_rendered = false;
        }

        // Dynamic resolution
        if (create_dynamic)
        {
            // Blur
            bool is_output_larger = width_output > width_render && height_output > height_render;
            uint32_t width        = is_output_larger ? width_output : width_render;
            uint32_t height       = is_output_larger ? height_output : height_render;
            render_target(RendererTexture::Blur) = make_unique<RHI_Texture2D>(m_context, width, height, 1, RHI_Format_R16G16B16A16_Float, RHI_Texture_Uav | RHI_Texture_Srv, "rt_blur");
        }

        RHI_FSR2::OnResolutionChange(m_rhi_device.get(), m_resolution_render, m_resolution_output);
    }

    void Renderer::CreateShaders()
    {
        const bool async        = true;
        const string shader_dir = ResourceCache::GetResourceDirectory(ResourceDirectory::Shaders) + "\\";

        // G-Buffer
        shader(RendererShader::Gbuffer_V) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Gbuffer_V)->Compile(RHI_Shader_Vertex, shader_dir + "g_buffer.hlsl", async, RHI_Vertex_Type::PosTexNorTan);
        shader(RendererShader::Gbuffer_P) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Gbuffer_P)->Compile(RHI_Shader_Pixel, shader_dir + "g_buffer.hlsl", async);

        // Light
        shader(RendererShader::Light_C) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Light_C)->Compile(RHI_Shader_Compute, shader_dir + "light.hlsl", async);

        // Triangle & Quad
        {
            shader(RendererShader::FullscreenTriangle_V) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::FullscreenTriangle_V)->Compile(RHI_Shader_Vertex, shader_dir + "fullscreen_triangle.hlsl", async, RHI_Vertex_Type::Undefined);

            shader(RendererShader::Quad_V) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Quad_V)->Compile(RHI_Shader_Vertex, shader_dir + "quad.hlsl", async, RHI_Vertex_Type::PosTexNorTan);
        }

        // Depth prepass
        {
            shader(RendererShader::Depth_Prepass_V) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Depth_Prepass_V)->Compile(RHI_Shader_Vertex, shader_dir + "depth_prepass.hlsl", async, RHI_Vertex_Type::PosTexNorTan);

            shader(RendererShader::Depth_Prepass_P) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Depth_Prepass_P)->Compile(RHI_Shader_Pixel, shader_dir + "depth_prepass.hlsl", async);
        }

        // Depth light
        {
            shader(RendererShader::Depth_Light_V) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Depth_Light_V)->Compile(RHI_Shader_Vertex, shader_dir + "depth_light.hlsl", async, RHI_Vertex_Type::PosTexNorTan);

            shader(RendererShader::Depth_Light_P) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Depth_Light_P)->Compile(RHI_Shader_Pixel, shader_dir + "depth_light.hlsl", async);
        }

        // Entity
        shader(RendererShader::Entity_V) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Entity_V)->Compile(RHI_Shader_Vertex, shader_dir + "entity.hlsl", async, RHI_Vertex_Type::PosTexNorTan);

        // Font
        shader(RendererShader::Font_V) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Font_V)->Compile(RHI_Shader_Vertex, shader_dir + "font.hlsl", async, RHI_Vertex_Type::PosTex);
        shader(RendererShader::Font_P) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Font_P)->Compile(RHI_Shader_Pixel, shader_dir + "font.hlsl", async);

        // Color
        shader(RendererShader::Lines_V) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Lines_V)->Compile(RHI_Shader_Vertex, shader_dir + "lines.hlsl", async, RHI_Vertex_Type::PosCol);
        shader(RendererShader::Lines_P) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Lines_P)->Compile(RHI_Shader_Pixel, shader_dir + "lines.hlsl", async);

        // Reflection probe
        shader(RendererShader::Reflection_Probe_V) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Reflection_Probe_V)->Compile(RHI_Shader_Vertex, shader_dir + "reflection_probe.hlsl", async, RHI_Vertex_Type::PosTexNorTan);
        shader(RendererShader::Reflection_Probe_P) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Reflection_Probe_P)->Compile(RHI_Shader_Pixel, shader_dir + "reflection_probe.hlsl", async);

        // Debug
        {
            shader(RendererShader::Debug_ReflectionProbe_V) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Debug_ReflectionProbe_V)->Compile(RHI_Shader_Vertex, shader_dir + "debug_reflection_probe.hlsl", async, RHI_Vertex_Type::PosTexNorTan);
            shader(RendererShader::Debug_ReflectionProbe_P) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Debug_ReflectionProbe_P)->Compile(RHI_Shader_Pixel, shader_dir + "debug_reflection_probe.hlsl", async);
        }

        // Copy
        {
            shader(RendererShader::Copy_Point_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Copy_Point_C)->AddDefine("COMPUTE");
            shader(RendererShader::Copy_Point_C)->Compile(RHI_Shader_Compute, shader_dir + "copy.hlsl", async);

            shader(RendererShader::Copy_Bilinear_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Copy_Bilinear_C)->AddDefine("COMPUTE");
            shader(RendererShader::Copy_Bilinear_C)->AddDefine("BILINEAR");
            shader(RendererShader::Copy_Bilinear_C)->Compile(RHI_Shader_Compute, shader_dir + "copy.hlsl", async);

            shader(RendererShader::Copy_Point_P) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Copy_Point_P)->AddDefine("PIXEL");
            shader(RendererShader::Copy_Point_P)->AddDefine("BILINEAR");
            shader(RendererShader::Copy_Point_P)->Compile(RHI_Shader_Pixel, shader_dir + "copy.hlsl", async);

            shader(RendererShader::Copy_Bilinear_P) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Copy_Bilinear_P)->AddDefine("PIXEL");
            shader(RendererShader::Copy_Bilinear_P)->AddDefine("BILINEAR");
            shader(RendererShader::Copy_Bilinear_P)->Compile(RHI_Shader_Pixel, shader_dir + "copy.hlsl", async);
        }

        // Blur
        {
            // Gaussian
            shader(RendererShader::BlurGaussian_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::BlurGaussian_C)->AddDefine("PASS_BLUR_GAUSSIAN");
            shader(RendererShader::BlurGaussian_C)->Compile(RHI_Shader_Compute, shader_dir + "blur.hlsl", async);

            // Gaussian bilateral 
            shader(RendererShader::BlurGaussianBilateral_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::BlurGaussianBilateral_C)->AddDefine("PASS_BLUR_BILATERAL_GAUSSIAN");
            shader(RendererShader::BlurGaussianBilateral_C)->Compile(RHI_Shader_Compute, shader_dir + "blur.hlsl", async);
        }

        // Bloom
        {
            // Downsample luminance
            shader(RendererShader::BloomLuminance_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::BloomLuminance_C)->AddDefine("LUMINANCE");
            shader(RendererShader::BloomLuminance_C)->Compile(RHI_Shader_Compute, shader_dir + "bloom.hlsl", async);

            // Upsample blend (with previous mip)
            shader(RendererShader::BloomUpsampleBlendMip_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::BloomUpsampleBlendMip_C)->AddDefine("UPSAMPLE_BLEND_MIP");
            shader(RendererShader::BloomUpsampleBlendMip_C)->Compile(RHI_Shader_Compute, shader_dir + "bloom.hlsl", async);

            // Upsample blend (with frame)
            shader(RendererShader::BloomBlendFrame_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::BloomBlendFrame_C)->AddDefine("BLEND_FRAME");
            shader(RendererShader::BloomBlendFrame_C)->Compile(RHI_Shader_Compute, shader_dir + "bloom.hlsl", async);
        }

        // Film grain
        shader(RendererShader::FilmGrain_C) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::FilmGrain_C)->Compile(RHI_Shader_Compute, shader_dir + "film_grain.hlsl", async);

        // Chromatic aberration
        shader(RendererShader::ChromaticAberration_C) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::ChromaticAberration_C)->AddDefine("PASS_CHROMATIC_ABERRATION");
        shader(RendererShader::ChromaticAberration_C)->Compile(RHI_Shader_Compute, shader_dir + "chromatic_aberration.hlsl", async);

        // Tone-mapping & gamma correction
        shader(RendererShader::ToneMappingGammaCorrection_C) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::ToneMappingGammaCorrection_C)->Compile(RHI_Shader_Compute, shader_dir + "tone_mapping_gamma_correction.hlsl", async);

        // FXAA
        shader(RendererShader::Fxaa_C) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Fxaa_C)->Compile(RHI_Shader_Compute, shader_dir + "fxaa.hlsl", async);

        // Depth of Field
        {
            shader(RendererShader::Dof_DownsampleCoc_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Dof_DownsampleCoc_C)->AddDefine("DOWNSAMPLE_CIRCLE_OF_CONFUSION");
            shader(RendererShader::Dof_DownsampleCoc_C)->Compile(RHI_Shader_Compute, shader_dir + "depth_of_field.hlsl", async);

            shader(RendererShader::Dof_Bokeh_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Dof_Bokeh_C)->AddDefine("BOKEH");
            shader(RendererShader::Dof_Bokeh_C)->Compile(RHI_Shader_Compute, shader_dir + "depth_of_field.hlsl", async);

            shader(RendererShader::Dof_Tent_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Dof_Tent_C)->AddDefine("TENT");
            shader(RendererShader::Dof_Tent_C)->Compile(RHI_Shader_Compute, shader_dir + "depth_of_field.hlsl", async);

            shader(RendererShader::Dof_UpscaleBlend_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Dof_UpscaleBlend_C)->AddDefine("UPSCALE_BLEND");
            shader(RendererShader::Dof_UpscaleBlend_C)->Compile(RHI_Shader_Compute, shader_dir + "depth_of_field.hlsl", async);
        }

        // Motion Blur
        shader(RendererShader::MotionBlur_C) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::MotionBlur_C)->Compile(RHI_Shader_Compute, shader_dir + "motion_blur.hlsl", async);

        // Dithering
        shader(RendererShader::Debanding_C) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Debanding_C)->Compile(RHI_Shader_Compute, shader_dir + "debanding.hlsl", async);

        // SSAO
        shader(RendererShader::Ssao_C) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Ssao_C)->Compile(RHI_Shader_Compute, shader_dir + "ssao.hlsl", async);

        // Light
        {
            shader(RendererShader::Light_Composition_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Light_Composition_C)->Compile(RHI_Shader_Compute, shader_dir + "light_composition.hlsl", async);

            shader(RendererShader::Light_ImageBased_P) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Light_ImageBased_P)->Compile(RHI_Shader_Pixel, shader_dir + "light_image_based.hlsl", async);
        }

        // SSR
        shader(RendererShader::Ssr_C) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Ssr_C)->Compile(RHI_Shader_Compute, shader_dir + "ssr.hlsl", async);

        // Entity - Transform
        shader(RendererShader::Entity_Transform_P) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Entity_Transform_P)->AddDefine("TRANSFORM");
        shader(RendererShader::Entity_Transform_P)->Compile(RHI_Shader_Pixel, shader_dir + "entity.hlsl", async);

        // Entity - Outline
        shader(RendererShader::Entity_Outline_P) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Entity_Outline_P)->AddDefine("OUTLINE");
        shader(RendererShader::Entity_Outline_P)->Compile(RHI_Shader_Pixel, shader_dir + "entity.hlsl", async);

        // AMD FidelityFX CAS - Contrast Adaptive Sharpening
        shader(RendererShader::Ffx_Cas_C) = make_shared<RHI_Shader>(m_context);
        shader(RendererShader::Ffx_Cas_C)->Compile(RHI_Shader_Compute, shader_dir + "amd_fidelity_fx\\cas.hlsl", async);

        // Compiled immediately, they are needed the moment the engine starts.
        {
            // AMD FidelityFX SPD - Single Pass Downsample
            shader(RendererShader::Ffx_Spd_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::Ffx_Spd_C)->Compile(RHI_Shader_Compute, shader_dir + "amd_fidelity_fx\\spd.hlsl", false);

            // BRDF - Specular Lut
            shader(RendererShader::BrdfSpecularLut_C) = make_shared<RHI_Shader>(m_context);
            shader(RendererShader::BrdfSpecularLut_C)->Compile(RHI_Shader_Compute, shader_dir + "brdf_specular_lut.hlsl", false); }
    }

    void Renderer::CreateFonts()
    {
        // Get standard font directory
        const string dir_font = ResourceCache::GetResourceDirectory(ResourceDirectory::Fonts) + "\\";

        // Load a font (used for performance metrics)
        m_font = make_unique<Font>(m_context, dir_font + "CalibriBold.ttf", 16, Vector4(0.8f, 0.8f, 0.8f, 1.0f));
    }

    void Renderer::CreateMeshes()
    {
        // Sphere
        {
            vector<RHI_Vertex_PosTexNorTan> vertices;
            vector<uint32_t> indices;
            Geometry::CreateSphere(&vertices, &indices, 0.2f, 20, 20);

            m_sphere_vertex_buffer = make_shared<RHI_VertexBuffer>(m_rhi_device.get(), false, "sphere");
            m_sphere_vertex_buffer->Create(vertices);

            m_sphere_index_buffer = make_shared<RHI_IndexBuffer>(m_rhi_device.get(), false, "sphere");
            m_sphere_index_buffer->Create(indices);
        }

        // Quad
        {
            vector<RHI_Vertex_PosTexNorTan> vertices;
            vector<uint32_t> indices;
            Geometry::CreateQuad(&vertices, &indices);

            m_quad_vertex_buffer = make_shared<RHI_VertexBuffer>(m_rhi_device.get(), false, "rectangle");
            m_quad_vertex_buffer->Create(vertices);

            m_quad_index_buffer = make_shared<RHI_IndexBuffer>(m_rhi_device.get(), false, "rectangle");
            m_quad_index_buffer->Create(indices);
        }

        // Buffer where all the lines are kept
        m_vertex_buffer_lines = make_shared<RHI_VertexBuffer>(m_rhi_device.get(), true, "lines");

        // World grid
        m_gizmo_grid = make_unique<Grid>(m_rhi_device.get());
    }

    void Renderer::CreateTextures()
    {
        // Get standard texture directory
        const string dir_texture = ResourceCache::GetResourceDirectory(ResourceDirectory::Textures) + "\\";

        // Noise textures
        {
            m_tex_default_noise_normal = make_shared<RHI_Texture2D>(m_context, RHI_Texture_Srv, "default_noise_normal");
            m_tex_default_noise_normal->LoadFromFile(dir_texture + "noise_normal.png");

            m_tex_default_noise_blue = static_pointer_cast<RHI_Texture>(make_shared<RHI_Texture2DArray>(m_context, RHI_Texture_Srv, "default_noise_blue"));
            m_tex_default_noise_blue->LoadFromFile(dir_texture + "noise_blue_0.png");
        }

        // Color textures
        {
            m_tex_default_white = make_shared<RHI_Texture2D>(m_context, RHI_Texture_Srv, "default_white");
            m_tex_default_white->LoadFromFile(dir_texture + "white.png");

            m_tex_default_black = make_shared<RHI_Texture2D>(m_context, RHI_Texture_Srv, "default_black");
            m_tex_default_black->LoadFromFile(dir_texture + "black.png");

            m_tex_default_transparent = make_shared<RHI_Texture2D>(m_context, RHI_Texture_Srv, "default_transparent");
            m_tex_default_transparent->LoadFromFile(dir_texture + "transparent.png");
        }

        // Gizmo icons
        {
            m_tex_gizmo_light_directional = make_shared<RHI_Texture2D>(m_context, RHI_Texture_Srv, "default_icon_light_directional");
            m_tex_gizmo_light_directional->LoadFromFile(dir_texture + "sun.png");

            m_tex_gizmo_light_point = make_shared<RHI_Texture2D>(m_context, RHI_Texture_Srv, "default_icon_light_point");
            m_tex_gizmo_light_point->LoadFromFile(dir_texture + "light_bulb.png");

            m_tex_gizmo_light_spot = make_shared<RHI_Texture2D>(m_context, RHI_Texture_Srv, "default_icon_light_spot");
            m_tex_gizmo_light_spot->LoadFromFile(dir_texture + "flashlight.png");
        }
    }
}
