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
#include "IComponent.h"
#include <atomic>
#include "../../RHI/RHI_Definition.h"
//===================================

namespace Spartan
{
    class Mesh;
    namespace Math
    {
        class Vector3;
    }

    class SP_CLASS Terrain : public IComponent
    {
    public:
        Terrain(Context* context, Entity* entity, uint64_t id = 0);
        ~Terrain() = default;

        //= IComponent ===============================
        void Serialize(FileStream* stream) override;
        void Deserialize(FileStream* stream) override;
        //============================================

        const std::shared_ptr<RHI_Texture>& GetHeightMap() const { return m_height_map; }
        void SetHeightMap(const std::shared_ptr<RHI_Texture>& height_map);

        float GetMinY()     const { return m_min_y; }
        void SetMinY(float min_z) { m_min_y = min_z; }

        float GetMaxY()     const { return m_max_y; }
        void SetMaxY(float max_z) { m_max_y = max_z; }

        uint64_t GetHeightsamples() const { return m_height_samples; }
        uint32_t GetVertexCount()   const { return m_vertex_count; }
        uint32_t GetIndexCount()    const { return m_index_count; }

        void GenerateAsync();

    private:
        void UpdateFromMesh(const std::shared_ptr<Mesh>& mesh) const;
        void UpdateFromVertices(const std::vector<uint32_t>& indices, std::vector<RHI_Vertex_PosTexNorTan>& vertices);

        float m_min_y                     = 0.0f;
        float m_max_y                     = 30.0f;
        float m_vertex_density            = 1.0f;
        std::atomic<bool> m_is_generating = false;
        uint32_t m_height_samples         = 0;
        uint32_t m_vertex_count           = 0;
        uint32_t m_index_count            = 0;
        uint32_t m_triangle_count         = 0;
        std::shared_ptr<RHI_Texture> m_height_map;
        std::shared_ptr<Mesh> m_mesh;
    };
}
