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

//= INCLUDES ============================
#include "pch.h"
#include "Renderable.h"
#include "Transform.h"
#include "../../IO/FileStream.h"
#include "../../Resource/ResourceCache.h"
#include "../../RHI/RHI_Texture2D.h"
#include "../../RHI/RHI_Vertex.h"
#include "../Rendering/Geometry.h"
#include "../Rendering/Mesh.h"
//=======================================

//= NAMESPACES ===============
using namespace std;
using namespace Spartan::Math;
//============================

namespace Spartan
{
    inline void build(const DefaultGeometry type, Renderable* renderable)
    {    
        Mesh* mesh = new Mesh(renderable->GetContext());
        vector<RHI_Vertex_PosTexNorTan> vertices;
        vector<uint32_t> indices;

        const string project_directory = ResourceCache::GetProjectDirectory();

        // Construct geometry
        if (type == DefaultGeometry::Cube)
        {
            Geometry::CreateCube(&vertices, &indices);
            mesh->SetResourceFilePath(project_directory + "default_cube" + EXTENSION_MODEL);
        }
        else if (type == DefaultGeometry::Quad)
        {
            Geometry::CreateQuad(&vertices, &indices);
            mesh->SetResourceFilePath(project_directory + "default_quad" + EXTENSION_MODEL);
        }
        else if (type == DefaultGeometry::Sphere)
        {
            Geometry::CreateSphere(&vertices, &indices);
            mesh->SetResourceFilePath(project_directory + "default_sphere" + EXTENSION_MODEL);
        }
        else if (type == DefaultGeometry::Cylinder)
        {
            Geometry::CreateCylinder(&vertices, &indices);
            mesh->SetResourceFilePath(project_directory + "default_cylinder" + EXTENSION_MODEL);
        }
        else if (type == DefaultGeometry::Cone)
        {
            Geometry::CreateCone(&vertices, &indices);
            mesh->SetResourceFilePath(project_directory + "default_cone" + EXTENSION_MODEL);
        }

        if (vertices.empty() || indices.empty())
            return;

        mesh->AddIndices(indices);
        mesh->AddVertices(vertices);
        mesh->ComputeAabb();
        mesh->ComputeNormalizedScale();
        mesh->CreateGpuBuffers();

        renderable->SetGeometry(
            "Default_Geometry",
            0,
            static_cast<uint32_t>(indices.size()),
            0,
            static_cast<uint32_t>(vertices.size()),
            BoundingBox(vertices.data(), static_cast<uint32_t>(vertices.size())),
            mesh
        );
    }

    Renderable::Renderable(Context* context, Entity* entity, uint64_t id /*= 0*/) : IComponent(context, entity, id)
    {
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(m_material_default,        bool);
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(m_material,                Material*);
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(m_cast_shadows,            bool);
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(m_geometry_index_offset,   uint32_t);
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(m_geometry_index_count,    uint32_t);
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(m_geometry_vertex_offset,  uint32_t);
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(m_geometry_vertex_count,   uint32_t);
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(m_geometry_name,           string);
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(m_mesh,                    Mesh*);
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(m_bounding_box,            BoundingBox);
        SP_REGISTER_ATTRIBUTE_GET_SET(DefaultGeometry, SetGeometry,  DefaultGeometry);
    }

    void Renderable::Serialize(FileStream* stream)
    {
        // Mesh
        stream->Write(static_cast<uint32_t>(m_geometry_type));
        stream->Write(m_geometry_index_offset);
        stream->Write(m_geometry_index_count);
        stream->Write(m_geometry_vertex_offset);
        stream->Write(m_geometry_vertex_count);
        stream->Write(m_bounding_box);
        stream->Write(m_mesh ? m_mesh->GetResourceName() : "");

        // Material
        stream->Write(m_cast_shadows);
        stream->Write(m_material_default);
        if (!m_material_default)
        {
            stream->Write(m_material ? m_material->GetResourceName() : "");
        }
    }

    void Renderable::Deserialize(FileStream* stream)
    {
        // Geometry
        m_geometry_type          = static_cast<DefaultGeometry>(stream->ReadAs<uint32_t>());
        m_geometry_index_offset  = stream->ReadAs<uint32_t>();
        m_geometry_index_count   = stream->ReadAs<uint32_t>();
        m_geometry_vertex_offset = stream->ReadAs<uint32_t>();
        m_geometry_vertex_count  = stream->ReadAs<uint32_t>();
        stream->Read(&m_bounding_box);
        string model_name;
        stream->Read(&model_name);
        m_mesh = ResourceCache::GetByName<Mesh>(model_name).get();

        // If it was a default mesh, we have to reconstruct it
        if (m_geometry_type != DefaultGeometry::Undefined)
        {
            SetGeometry(m_geometry_type);
        }

        // Material
        stream->Read(&m_cast_shadows);
        stream->Read(&m_material_default);
        if (m_material_default)
        {
            SetDefaultMaterial();
        }
        else
        {
            string material_name;
            stream->Read(&material_name);
            m_material = ResourceCache::GetByName<Material>(material_name).get();
        }
    }

    void Renderable::SetGeometry(const string& name, const uint32_t index_offset, const uint32_t index_count, const uint32_t vertex_offset, const uint32_t vertex_count, const BoundingBox& bounding_box, Mesh* mesh)
    {
        // Terrible way to delete previous geometry in case it's a default one
        if (m_geometry_name == "Default_Geometry")
        {
            delete m_mesh;
        }

        m_geometry_name          = name;
        m_geometry_index_offset  = index_offset;
        m_geometry_index_count   = index_count;
        m_geometry_vertex_offset = vertex_offset;
        m_geometry_vertex_count  = vertex_count;
        m_bounding_box           = bounding_box;
        m_mesh                   = mesh;
    }

    void Renderable::SetGeometry(const DefaultGeometry type)
    {
        m_geometry_type = type;

        if (type != DefaultGeometry::Undefined)
        {
            build(type, this);
        }
    }

    void Renderable::Clear()
    {
        SetGeometry("Cleared", 0, 0, 0, 0, BoundingBox(), nullptr);
    }

    void Renderable::GetGeometry(vector<uint32_t>* indices, vector<RHI_Vertex_PosTexNorTan>* vertices) const
    {
        SP_ASSERT_MSG(m_mesh != nullptr, "Invalid mesh");
        m_mesh->GetGeometry(m_geometry_index_offset, m_geometry_index_count, m_geometry_vertex_offset, m_geometry_vertex_count, indices, vertices);
    }

    const BoundingBox& Renderable::GetAabb()
    {
        // Updated if dirty
        if (m_last_transform != GetTransform()->GetMatrix() || !m_aabb.Defined())
        {
            m_aabb = m_bounding_box.Transform(GetTransform()->GetMatrix());
            m_last_transform = GetTransform()->GetMatrix();
        }

        return m_aabb;
    }

    // All functions (set/load) resolve to this
    shared_ptr<Material> Renderable::SetMaterial(const shared_ptr<Material>& material)
    {
        SP_ASSERT(material != nullptr);

        // In order for the component to guarantee serialization/deserialization, we cache the material
        shared_ptr<Material> _material = ResourceCache::Cache(material);

        m_material = _material.get();

        // Set to false otherwise material won't serialize/deserialize
        m_material_default = false;

        return _material;
    }

    shared_ptr<Material> Renderable::SetMaterial(const string& file_path)
    {
        // Load the material
        auto material = make_shared<Material>(GetContext());
        if (!material->LoadFromFile(file_path))
        {
            SP_LOG_WARNING("Failed to load material from \"%s\"", file_path.c_str());
            return nullptr;
        }

        // Set it as the current material
        return SetMaterial(material);
    }

    void Renderable::SetDefaultMaterial()
    {
        m_material_default = true;
        const string data_dir = ResourceCache::GetDataDirectory() + "\\";
        FileSystem::CreateDirectory(data_dir);

        // Create material
        shared_ptr<Material> material = make_shared<Material>(GetContext());
        material->SetResourceFilePath(ResourceCache::GetProjectDirectory() + "standard" + EXTENSION_MATERIAL); // Set resource file path so it can be used by the resource cache
        material->SetIsEditable(false);
        material->SetProperty(MaterialProperty::UvTilingX, 10.0f);
        material->SetProperty(MaterialProperty::UvTilingY, 10.0f);
        material->SetProperty(MaterialProperty::ColorR, 1.0f);
        material->SetProperty(MaterialProperty::ColorG, 1.0f);
        material->SetProperty(MaterialProperty::ColorB, 1.0f);
        material->SetProperty(MaterialProperty::ColorA, 1.0f);

        // Se default texture
        const shared_ptr<RHI_Texture2D> texture = ResourceCache::Load<RHI_Texture2D>(ResourceCache::GetResourceDirectory(ResourceDirectory::Textures) + "\\no_texture.png");
        material->SetTexture(MaterialTexture::Color, texture);

        // Set material
        SetMaterial(material);
        m_material_default = true;
    }

    string Renderable::GetMaterialName() const
    {
        return m_material ? m_material->GetResourceName() : "";
    }
}
