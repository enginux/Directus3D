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

//= INCLUDES ========================
#include <array>
#include <memory>
#include "IComponent.h"
#include "../../Math/Vector4.h"
#include "../../Math/Vector3.h"
#include "../../Math/Matrix.h"
#include "../../RHI/RHI_Definition.h"
#include "../../Math/Frustum.h"
#include "../../Rendering/Color.h"
//===================================

namespace Spartan
{
    //= FWD DECLARATIONS =
    class Camera;
    class Renderable;
    class Renderer;
    //====================

    enum class LightType
    {
        Directional,
        Point,
        Spot
    };

    enum class LightIntensity
    {
        direct_sunglight,
        bulb_150_watt,
        average_flashlight
    };

    struct ShadowSlice
    {
        Math::Vector3 min    = Math::Vector3::Zero;
        Math::Vector3 max    = Math::Vector3::Zero;
        Math::Vector3 center = Math::Vector3::Zero;
        Math::Frustum frustum;
    };

    struct ShadowMap
    {
        std::shared_ptr<RHI_Texture> texture_color;
        std::shared_ptr<RHI_Texture> texture_depth;
        std::vector<ShadowSlice> slices;
    };

    class SP_CLASS Light : public IComponent
    {
    public:
        Light(Context* context, Entity* entity, uint64_t id = 0);
        ~Light() = default;

        //= COMPONENT ================================
        void OnInitialize() override;
        void OnStart() override;
        void OnTick(double delta_time) override;
        void Serialize(FileStream* stream) override;
        void Deserialize(FileStream* stream) override;
        //============================================

        const auto GetLightType() const { return m_light_type; }
        void SetLightType(LightType type);

        void SetColor(const float temperature);
        void SetColor(const Color& rgb) { m_color_rgb = rgb; }
        const Color& GetColor()   const { return m_color_rgb; }

        void SetIntensity(const LightIntensity lumens);
        void SetIntensity(const float lumens) { m_intensity_lumens = lumens; }
        float GetIntensity()            const { return m_intensity_lumens; }
        float GetIntensityForShader(Camera* camera) const;

        bool GetShadowsEnabled() const { return m_shadows_enabled; }
        void SetShadowsEnabled(bool cast_shadows);

        bool GetShadowsScreenSpaceEnabled() const                    { return m_shadows_screen_space_enabled; }
        void SetShadowsScreenSpaceEnabled(bool cast_contact_shadows) { m_shadows_screen_space_enabled = cast_contact_shadows; }

        bool GetShadowsTransparentEnabled() const { return m_shadows_transparent_enabled; }
        void SetShadowsTransparentEnabled(bool cast_transparent_shadows);

        bool GetVolumetricEnabled() const             { return m_volumetric_enabled; }
        void SetVolumetricEnabled(bool is_volumetric) { m_volumetric_enabled = is_volumetric; }

        void SetRange(float range);
        auto GetRange() const { return m_range; }

        void SetAngle(float angle_rad);
        auto GetAngle() const { return m_angle_rad; }

        void SetBias(float value) { m_bias = value; }
        float GetBias() const     { return m_bias; }

        void SetNormalBias(float value) { m_normal_bias = value; }
        auto GetNormalBias() const { return m_normal_bias; }

        const Math::Matrix& GetViewMatrix(uint32_t index = 0) const;
        const Math::Matrix& GetProjectionMatrix(uint32_t index = 0) const;

        RHI_Texture* GetDepthTexture() const { return m_shadow_map.texture_depth.get(); }
        RHI_Texture* GetColorTexture() const { return m_shadow_map.texture_color.get(); }
        uint32_t GetShadowArraySize() const;
        void CreateShadowMap();

        bool IsInViewFrustum(Renderable* renderable, uint32_t index) const;

    private:
        void ComputeViewMatrix();
        bool ComputeProjectionMatrix(uint32_t index = 0);
        void ComputeCascadeSplits();

        // Shadows
        bool m_shadows_enabled              = true;
        bool m_shadows_screen_space_enabled = true;
        bool m_shadows_transparent_enabled  = true;
        uint32_t m_cascade_count            = 4;
        ShadowMap m_shadow_map;

        // Bias
        float m_bias        = 0.0f;
        float m_normal_bias = 10.0f;

        // Misc
        LightType m_light_type    = LightType::Directional;
        Color m_color_rgb         = Color(1.0f, 0.76f, 0.57f, 1.0f);
        bool m_volumetric_enabled = true;
        float m_range             = 10.0f;
        float m_intensity_lumens  = 0.0f;
        float m_angle_rad         = 0.5f; // about 30 degrees
        bool m_initialized        = false;
        std::array<Math::Matrix, 6> m_matrix_view;
        std::array<Math::Matrix, 6> m_matrix_projection;

        // Dirty checks
        bool m_is_dirty                     = true;
        Math::Matrix m_previous_camera_view = Math::Matrix::Identity;
        bool m_previous_reverse_z           = false;

        // Dependencies
        Renderer* m_renderer = nullptr;
    };
}
