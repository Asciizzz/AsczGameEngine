#include "Az3D/Material.hpp"
#include "tiny_obj_loader.h"
#include <iostream>

namespace Az3D {
    
    Material::Material(const std::string& name) : name(name) {
        // Initialize with default properties
        properties = MaterialProperties{};
    }
    
    void Material::setDiffuseTexture(const std::string& textureId) {
        diffuseTextureId = textureId;
    }
    
    void Material::setNormalTexture(const std::string& textureId) {
        normalTextureId = textureId;
    }
    
    void Material::setSpecularTexture(const std::string& textureId) {
        specularTextureId = textureId;
    }
    
    void Material::setRoughnessTexture(const std::string& textureId) {
        roughnessTextureId = textureId;
    }
    
    void Material::setMetallicTexture(const std::string& textureId) {
        metallicTextureId = textureId;
    }
    
    void Material::setAOTexture(const std::string& textureId) {
        aoTextureId = textureId;
    }
    
    void Material::setEmissiveTexture(const std::string& textureId) {
        emissiveTextureId = textureId;
    }
    
    std::shared_ptr<Material> Material::createFromOBJ(const tinyobj::material_t& objMaterial) {
        auto material = std::make_shared<Material>(objMaterial.name);
        
        // Set material properties from OBJ material
        material->properties.albedoColor = glm::vec3(
            objMaterial.diffuse[0],
            objMaterial.diffuse[1], 
            objMaterial.diffuse[2]
        );
        
        material->properties.emissiveColor = glm::vec3(
            objMaterial.emission[0],
            objMaterial.emission[1],
            objMaterial.emission[2]
        );
        
        material->properties.specular = (objMaterial.specular[0] + objMaterial.specular[1] + objMaterial.specular[2]) / 3.0f;
        material->properties.opacity = objMaterial.dissolve;
        
        // Map shininess to roughness (inverse relationship)
        material->properties.roughness = 1.0f - std::min(objMaterial.shininess / 128.0f, 1.0f);
        
        // PBR properties if available
        material->properties.metallic = objMaterial.metallic;
        material->properties.roughness = objMaterial.roughness > 0 ? objMaterial.roughness : material->properties.roughness;
        
        // Set texture IDs from OBJ material (use filename as texture ID)
        if (!objMaterial.diffuse_texname.empty()) {
            material->setDiffuseTexture(objMaterial.diffuse_texname);
        }
        
        if (!objMaterial.normal_texname.empty()) {
            material->setNormalTexture(objMaterial.normal_texname);
        }
        
        if (!objMaterial.specular_texname.empty()) {
            material->setSpecularTexture(objMaterial.specular_texname);
        }
        
        if (!objMaterial.roughness_texname.empty()) {
            material->setRoughnessTexture(objMaterial.roughness_texname);
        }
        
        if (!objMaterial.metallic_texname.empty()) {
            material->setMetallicTexture(objMaterial.metallic_texname);
        }
        
        if (!objMaterial.bump_texname.empty()) {
            // Use bump map as normal map
            material->setNormalTexture(objMaterial.bump_texname);
        }
        
        return material;
    }
    
} // namespace Az3D
