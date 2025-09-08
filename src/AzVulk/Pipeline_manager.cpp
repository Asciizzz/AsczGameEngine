#include "AzVulk/Pipeline_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>

using namespace AzVulk;

// Simple JSON parser for pipeline assets
// This is a minimal parser - in production you'd use a proper JSON library
namespace SimpleJson {
    
    struct JsonValue {
        enum Type { STRING, OBJECT, ARRAY, BOOLEAN, NUMBER };
        Type type;
        std::string stringValue;
        std::unordered_map<std::string, JsonValue> objectValue;
        std::vector<JsonValue> arrayValue;
        bool boolValue = false;
        double numberValue = 0.0;
    };
    
    // Simple JSON parser - handles basic cases for our pipeline assets
    JsonValue parseJson(const std::string& json);
    std::string trim(const std::string& str);
    JsonValue parseValue(const std::string& json, size_t& pos);
    JsonValue parseObject(const std::string& json, size_t& pos);
    JsonValue parseArray(const std::string& json, size_t& pos);
    JsonValue parseString(const std::string& json, size_t& pos);
    
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \\t\\n\\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \\t\\n\\r");
        return str.substr(start, end - start + 1);
    }
    
    JsonValue parseJson(const std::string& json) {
        size_t pos = 0;
        return parseValue(json, pos);
    }
    
    JsonValue parseValue(const std::string& json, size_t& pos) {
        // Skip whitespace
        while (pos < json.length() && isspace(json[pos])) pos++;
        
        if (pos >= json.length()) return JsonValue{};
        
        char c = json[pos];
        if (c == '{') {
            return parseObject(json, pos);
        } else if (c == '[') {
            return parseArray(json, pos);
        } else if (c == '"') {
            return parseString(json, pos);
        } else if (c == 't' || c == 'f') {
            // Boolean
            JsonValue val;
            val.type = JsonValue::BOOLEAN;
            if (json.substr(pos, 4) == "true") {
                val.boolValue = true;
                pos += 4;
            } else if (json.substr(pos, 5) == "false") {
                val.boolValue = false;
                pos += 5;
            }
            return val;
        } else if (isdigit(c) || c == '-') {
            // Number
            JsonValue val;
            val.type = JsonValue::NUMBER;
            size_t start = pos;
            if (c == '-') pos++;
            while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '.')) pos++;
            val.numberValue = std::stod(json.substr(start, pos - start));
            return val;
        }
        
        return JsonValue{};
    }
    
    JsonValue parseObject(const std::string& json, size_t& pos) {
        JsonValue obj;
        obj.type = JsonValue::OBJECT;
        
        pos++; // Skip '{'
        
        while (pos < json.length()) {
            // Skip whitespace
            while (pos < json.length() && isspace(json[pos])) pos++;
            
            if (pos >= json.length() || json[pos] == '}') {
                pos++; // Skip '}'
                break;
            }
            
            // Parse key
            JsonValue key = parseString(json, pos);
            
            // Skip whitespace and ':'
            while (pos < json.length() && (isspace(json[pos]) || json[pos] == ':')) pos++;
            
            // Parse value
            JsonValue value = parseValue(json, pos);
            
            obj.objectValue[key.stringValue] = value;
            
            // Skip whitespace and optional ','
            while (pos < json.length() && (isspace(json[pos]) || json[pos] == ',')) pos++;
        }
        
        return obj;
    }
    
    JsonValue parseArray(const std::string& json, size_t& pos) {
        JsonValue arr;
        arr.type = JsonValue::ARRAY;
        
        pos++; // Skip '['
        
        while (pos < json.length()) {
            // Skip whitespace
            while (pos < json.length() && isspace(json[pos])) pos++;
            
            if (pos >= json.length() || json[pos] == ']') {
                pos++; // Skip ']'
                break;
            }
            
            JsonValue value = parseValue(json, pos);
            arr.arrayValue.push_back(value);
            
            // Skip whitespace and optional ','
            while (pos < json.length() && (isspace(json[pos]) || json[pos] == ',')) pos++;
        }
        
        return arr;
    }
    
    JsonValue parseString(const std::string& json, size_t& pos) {
        JsonValue str;
        str.type = JsonValue::STRING;
        
        pos++; // Skip opening '"'
        size_t start = pos;
        
        while (pos < json.length() && json[pos] != '"') {
            if (json[pos] == '\\') pos++; // Skip escaped character
            pos++;
        }
        
        str.stringValue = json.substr(start, pos - start);
        pos++; // Skip closing '"'
        
        return str;
    }
}

bool PipelineManager::loadPipelinesFromJson(const std::string& jsonFilePath) {
    std::ifstream file(jsonFilePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open pipeline JSON file: " + jsonFilePath);
    }
    
    // Read JSON file
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string jsonContent = buffer.str();
    
    try {
        SimpleJson::JsonValue root = SimpleJson::parseJson(jsonContent);
        
        if (root.type == SimpleJson::JsonValue::OBJECT) {
            // Parse prototypes first
            if (root.objectValue.find("prototypes") != root.objectValue.end()) {
                const auto& prototypesObj = root.objectValue.at("prototypes");
                if (prototypesObj.type == SimpleJson::JsonValue::OBJECT) {
                    for (const auto& [name, prototypeJson] : prototypesObj.objectValue) {
                        PipelineAsset asset;
                        asset.name = name;
                        asset.isPrototype = true;

                        RasterCfg config = parseRasterConfig(asset);
                        prototypes[name] = config;
                    }
                }
            }
            
            // Parse pipelines
            if (root.objectValue.find("pipelines") != root.objectValue.end()) {
                const auto& pipelinesObj = root.objectValue.at("pipelines");
                if (pipelinesObj.type == SimpleJson::JsonValue::ARRAY) {
                    for (const auto& pipelineJson : pipelinesObj.arrayValue) {
                        if (pipelineJson.type == SimpleJson::JsonValue::OBJECT) {
                            PipelineAsset asset;
                            
                            // Parse JSON into asset structure
                            auto& obj = pipelineJson.objectValue;
                            
                            if (obj.find("name") != obj.end()) {
                                asset.name = obj.at("name").stringValue;
                            }
                            
                            if (obj.find("inheritsFrom") != obj.end()) {
                                asset.inheritsFrom = obj.at("inheritsFrom").stringValue;
                            }
                            
                            if (obj.find("vertexShader") != obj.end()) {
                                asset.vertexShader = obj.at("vertexShader").stringValue;
                            }
                            
                            if (obj.find("fragmentShader") != obj.end()) {
                                asset.fragmentShader = obj.at("fragmentShader").stringValue;
                            }
                            
                            if (obj.find("vertexInput") != obj.end()) {
                                asset.vertexInput = obj.at("vertexInput").stringValue;
                            }
                            
                            if (obj.find("depthTest") != obj.end()) {
                                asset.depthTest = obj.at("depthTest").boolValue;
                            }
                            
                            if (obj.find("depthWrite") != obj.end()) {
                                asset.depthWrite = obj.at("depthWrite").boolValue;
                            }
                            
                            if (obj.find("cullMode") != obj.end()) {
                                asset.cullMode = obj.at("cullMode").stringValue;
                            }
                            
                            if (obj.find("blendMode") != obj.end()) {
                                asset.blendMode = obj.at("blendMode").stringValue;
                            }
                            
                            // Parse descriptor layouts array
                            if (obj.find("descriptorLayouts") != obj.end() && obj.at("descriptorLayouts").type == SimpleJson::JsonValue::ARRAY) {
                                for (const auto& layoutJson : obj.at("descriptorLayouts").arrayValue) {
                                    if (layoutJson.type == SimpleJson::JsonValue::STRING) {
                                        asset.descriptorLayouts.push_back(layoutJson.stringValue);
                                    }
                                }
                            }
                            
                            // Parse push constants array
                            if (obj.find("pushConstants") != obj.end() && obj.at("pushConstants").type == SimpleJson::JsonValue::ARRAY) {
                                for (const auto& pcJson : obj.at("pushConstants").arrayValue) {
                                    if (pcJson.type == SimpleJson::JsonValue::OBJECT) {
                                        PipelineAsset::PushConstant pc;
                                        auto& pcObj = pcJson.objectValue;
                                        
                                        if (pcObj.find("stages") != pcObj.end()) {
                                            const auto& stagesJson = pcObj.at("stages");
                                            if (stagesJson.type == SimpleJson::JsonValue::ARRAY) {
                                                // Handle array of stage strings
                                                for (const auto& stageJson : stagesJson.arrayValue) {
                                                    if (stageJson.type == SimpleJson::JsonValue::STRING) {
                                                        pc.stages.push_back(stageJson.stringValue);
                                                    }
                                                }
                                            } else if (stagesJson.type == SimpleJson::JsonValue::STRING) {
                                                // Handle single string for backward compatibility
                                                pc.stages.push_back(stagesJson.stringValue);
                                            }
                                        }
                                        if (pcObj.find("offset") != pcObj.end()) {
                                            pc.offset = static_cast<uint32_t>(pcObj.at("offset").numberValue);
                                        }
                                        if (pcObj.find("size") != pcObj.end()) {
                                            pc.size = static_cast<uint32_t>(pcObj.at("size").numberValue);
                                        }
                                        
                                        asset.pushConstants.push_back(pc);
                                    }
                                }
                            }
                            
                            
                            // Store the asset data for later use
                            pipelineAssets[asset.name] = asset;
                            
                            RasterCfg config = parseRasterConfig(asset);
                            pipelineConfigs[asset.name] = config;
                        }
                    }
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse pipeline JSON: " + std::string(e.what()));
    }
}

RasterCfg PipelineManager::getPipelineConfig(const std::string& name) const {
    auto it = pipelineConfigs.find(name);
    if (it != pipelineConfigs.end()) {
        return it->second;
    }
    
    throw std::runtime_error("Pipeline configuration '" + name + "' not found!");
}

bool PipelineManager::hasPipeline(const std::string& name) const {
    return pipelineConfigs.find(name) != pipelineConfigs.end();
}

std::vector<std::string> PipelineManager::getAllPipelineNames() const {
    std::vector<std::string> names;
    for (const auto& [name, config] : pipelineConfigs) {
        names.push_back(name);
    }
    return names;
}

void PipelineManager::initializePipelines(
    VkDevice device,
    VkRenderPass renderPass,
    VkSampleCountFlagBits msaa,
    const std::unordered_map<std::string, VkDescriptorSetLayout>& namedLayouts,
    const std::unordered_map<std::string, NamedVertexInput>& namedVertexInputs
) {
    // Create all pipeline instances
    for (const auto& [name, config] : pipelineConfigs) {
        RasterCfg pipelineConfig = config;
        
        // Set common properties
        pipelineConfig.renderPass = renderPass;
        pipelineConfig.setMSAA(msaa);
        
        // Build descriptor layouts from the asset configuration
        std::vector<VkDescriptorSetLayout> layouts;
        
        // Get the asset to find its descriptor layout configuration
        auto assetIt = pipelineAssets.find(name);
        if (assetIt != pipelineAssets.end()) {
            const auto& asset = assetIt->second;
            // Use the descriptor layout names from the JSON config
            for (const auto& layoutName : asset.descriptorLayouts) {
                auto layoutIt = namedLayouts.find(layoutName);
                if (layoutIt != namedLayouts.end()) {
                    layouts.push_back(layoutIt->second);
                } else {
                    printf("Warning: Layout '%s' not found in named layouts for pipeline '%s'\n", layoutName.c_str(), name.c_str());
                }
            }
            
            // Apply vertex input from named inputs - same pattern as layouts
            auto vertexInputIt = namedVertexInputs.find(asset.vertexInput);
            if (vertexInputIt != namedVertexInputs.end()) {
                pipelineConfig.withVertexInput(vertexInputIt->second.bindings, vertexInputIt->second.attributes);
            } else {
                printf("Warning: Vertex input '%s' not found in named vertex inputs for pipeline '%s'\n", 
                       asset.vertexInput.c_str(), name.c_str());
            }
        } else {
            printf("Warning: Asset configuration not found for pipeline '%s'\n", name.c_str());
        }
        
        pipelineConfig.setLayouts = layouts;
        
        // Create pipeline instance
        auto pipeline = std::make_unique<PipelineRaster>(device, pipelineConfig);
        pipeline->create();
        
        // Store instance
        pipelineInstances[name] = std::move(pipeline);
    }
}

PipelineRaster* PipelineManager::getPipeline(const std::string& name) const {
    auto it = pipelineInstances.find(name);
    if (it != pipelineInstances.end()) {
        return it->second.get();
    }

    printf("Warning: Pipeline instance '%s' not found!\n", name.c_str());
    return nullptr;
}

bool PipelineManager::hasPipelineInstance(const std::string& name) const {
    return pipelineInstances.find(name) != pipelineInstances.end();
}

void PipelineManager::recreateAllPipelines(VkRenderPass newRenderPass) {
    for (auto& [name, pipeline] : pipelineInstances) {
        pipeline->setRenderPass(newRenderPass);
        pipeline->recreate();
    }
}

void PipelineManager::clear() {
    // Explicitly clean up all pipeline instances first
    for (auto& [name, pipeline] : pipelineInstances) {
        if (pipeline) pipeline->cleanup();
    }
    
    pipelineConfigs.clear();
    prototypes.clear();
    pipelineAssets.clear();  // Clear the asset storage too
    pipelineInstances.clear();
}

RasterCfg PipelineManager::parseRasterConfig(const PipelineAsset& asset) const {
    RasterCfg config;
    
    // Start with prototype if specified
    if (!asset.inheritsFrom.empty()) {
        auto it = prototypes.find(asset.inheritsFrom);
        if (it != prototypes.end()) {
            config = it->second;
        }
    }
    
    // Apply asset-specific configuration
    if (!asset.vertexShader.empty() && !asset.fragmentShader.empty()) {
        config.withShaders(asset.vertexShader, asset.fragmentShader);
    } else if (!asset.isPrototype) {
        printf("Warning: Pipeline '%s' has missing shader paths! Vertex: '%s', Fragment: '%s'\n",
                asset.name.c_str(), asset.vertexShader.c_str(), asset.fragmentShader.c_str());
    }
    
    config.withDepthTest(asset.depthTest);
    config.withDepthWrite(asset.depthWrite);
    config.withCulling(parseCullMode(asset.cullMode));
    config.withBlending(parseBlendMode(asset.blendMode));
    
    // Add push constants
    for (const auto& pc : asset.pushConstants) {
        config.withPushConstants(parseShaderStages(pc.stages), pc.offset, pc.size);
    }
    
    return config;
}

// Parser implementations
CullMode PipelineManager::parseCullMode(const std::string& str) const {
    if (str == "None") return CullMode::None;
    if (str == "Front") return CullMode::Front;
    if (str == "Back") return CullMode::Back;
    if (str == "FrontAndBack") return CullMode::FrontAndBack;
    return CullMode::Back;
}

BlendMode PipelineManager::parseBlendMode(const std::string& str) const {
    if (str == "None") return BlendMode::None;
    if (str == "Alpha") return BlendMode::Alpha;
    if (str == "Additive") return BlendMode::Additive;
    if (str == "Multiply") return BlendMode::Multiply;
    return BlendMode::None;
}

VkCompareOp PipelineManager::parseCompareOp(const std::string& str) const {
    if (str == "Never") return VK_COMPARE_OP_NEVER;
    if (str == "Less") return VK_COMPARE_OP_LESS;
    if (str == "Equal") return VK_COMPARE_OP_EQUAL;
    if (str == "LessOrEqual") return VK_COMPARE_OP_LESS_OR_EQUAL;
    if (str == "Greater") return VK_COMPARE_OP_GREATER;
    if (str == "NotEqual") return VK_COMPARE_OP_NOT_EQUAL;
    if (str == "GreaterOrEqual") return VK_COMPARE_OP_GREATER_OR_EQUAL;
    if (str == "Always") return VK_COMPARE_OP_ALWAYS;
    return VK_COMPARE_OP_LESS;
}

VkPolygonMode PipelineManager::parsePolygonMode(const std::string& str) const {
    if (str == "Fill") return VK_POLYGON_MODE_FILL;
    if (str == "Line") return VK_POLYGON_MODE_LINE;
    if (str == "Point") return VK_POLYGON_MODE_POINT;
    return VK_POLYGON_MODE_FILL;
}

VkShaderStageFlags PipelineManager::parseShaderStages(const std::vector<std::string>& stages) const {
    VkShaderStageFlags result = 0;
    
    for (const auto& stage : stages) {
        if (stage == "Vertex") result |= VK_SHADER_STAGE_VERTEX_BIT;
        else if (stage == "Fragment") result |= VK_SHADER_STAGE_FRAGMENT_BIT;
        else if (stage == "Compute") result |= VK_SHADER_STAGE_COMPUTE_BIT;
        else if (stage == "Geometry") result |= VK_SHADER_STAGE_GEOMETRY_BIT;
        else if (stage == "TessellationControl") result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        else if (stage == "TessellationEvaluation") result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        else if (stage == "All") result |= VK_SHADER_STAGE_ALL;
    }
    
    // Default to Fragment if no valid stages found
    if (result == 0) result = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    return result;
}
