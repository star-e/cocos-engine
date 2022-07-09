
/****************************************************************************
 Copyright (c) 2021-2022 Xiamen Yaji Software Co., Ltd.

 http://www.cocos.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated engine source code (the "Software"), a limited,
 worldwide, royalty-free, non-assignable, revocable and non-exclusive license
 to use Cocos Creator solely to develop games on your target platforms. You shall
 not use Cocos Creator software for developing other software or tools that's
 used for developing games. You are not granted to publish, distribute,
 sublicense, and/or sell copies of Cocos Creator.

 The software or tools in this License Agreement are licensed, not sold.
 Xiamen Yaji Software Co., Ltd. reserves all rights not expressly granted to you.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
****************************************************************************/

#include "ImmutableState.h"
#include <tuple>
#include "gfx-base/GFXDef-common.h"
#include "pipeline/custom/GslUtils.h"
#include "DevicePassResourceTable.h"

namespace cc {
namespace framegraph {

using gfx::hasFlag;
using gfx::BufferUsage;
using gfx::TextureUsage;
using gfx::MemoryUsage;
using gfx::MemoryAccess;
using gfx::ShaderStageFlags;
using gfx::PassType;
using gfx::AccessFlags;


namespace {
    enum class CommonUsage : uint32_t{
        NONE = 0,
        COPY_SRC = 1 << 0,
        COPY_DST = 1 << 1,
        ROM = 1 << 2,   // sampled or UNIFORM
        STORAGE = 1 << 3,
        IB_OR_CA = 1 << 4,
        VB_OR_DS = 1 << 5,
        INDIRECT_OR_INPUT = 1 << 6,
    };
    CC_ENUM_BITWISE_OPERATORS(CommonUsage);

    constexpr auto IGNORE_0 = MemoryAccess::NONE;
    constexpr auto IGNORE_1 = MemoryUsage::NONE;
    constexpr auto IGNORE_2 = PassType::RASTER;
    constexpr auto IGNORE_3 = ResourceType::UNKNOWN;
    constexpr auto IGNORE_4 = ShaderStageFlags::NONE;
    constexpr auto IGNORE_5 = CommonUsage::NONE;
    
    struct AccessKey {
        MemoryAccess memAccess{IGNORE_0};
        MemoryUsage memUsage{IGNORE_1};
        PassType passType{IGNORE_2};
        ResourceType resourceType{IGNORE_3};
        ShaderStageFlags shaderStage{IGNORE_4};
        CommonUsage usage{IGNORE_5};
    };

    const ccstd::unordered_map<AccessKey, AccessFlags> ACCESS_MAP = {
        {{}, AccessFlags::NONE},
        
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, IGNORE_2, ResourceType::BUFFER, IGNORE_4, CommonUsage::INDIRECT_OR_INPUT}, AccessFlags::INDIRECT_BUFFER},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::BUFFER, IGNORE_4, CommonUsage::IB_OR_CA}, AccessFlags::INDEX_BUFFER},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::BUFFER, IGNORE_4, CommonUsage::VB_OR_DS}, AccessFlags::VERTEX_BUFFER},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::BUFFER, ShaderStageFlags::VERTEX, CommonUsage::ROM}, AccessFlags::VERTEX_SHADER_READ_UNIFORM_BUFFER},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::TEXTURE, ShaderStageFlags::VERTEX, CommonUsage::ROM}, AccessFlags::VERTEX_SHADER_READ_TEXTURE},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, IGNORE_3, ShaderStageFlags::VERTEX, IGNORE_5}, AccessFlags::VERTEX_SHADER_READ_OTHER},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::BUFFER, ShaderStageFlags::FRAGMENT, CommonUsage::ROM}, AccessFlags::FRAGMENT_SHADER_READ_UNIFORM_BUFFER},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::TEXTURE, ShaderStageFlags::FRAGMENT, CommonUsage::ROM}, AccessFlags::FRAGMENT_SHADER_READ_TEXTURE},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::TEXTURE, ShaderStageFlags::FRAGMENT, CommonUsage::INDIRECT_OR_INPUT | CommonUsage::IB_OR_CA}, AccessFlags::FRAGMENT_SHADER_READ_COLOR_INPUT_ATTACHMENT},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::TEXTURE, ShaderStageFlags::FRAGMENT, CommonUsage::INDIRECT_OR_INPUT | CommonUsage::VB_OR_DS}, AccessFlags::FRAGMENT_SHADER_READ_DEPTH_STENCIL_INPUT_ATTACHMENT},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, IGNORE_3, ShaderStageFlags::FRAGMENT, IGNORE_5}, AccessFlags::FRAGMENT_SHADER_READ_OTHER},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::TEXTURE, ShaderStageFlags::FRAGMENT, CommonUsage::IB_OR_CA}, AccessFlags::COLOR_ATTACHMENT_READ},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::TEXTURE, ShaderStageFlags::FRAGMENT, CommonUsage::VB_OR_DS}, AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, IGNORE_2, ResourceType::BUFFER, IGNORE_4, CommonUsage::ROM}, AccessFlags::COMPUTE_SHADER_READ_UNIFORM_BUFFER},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, IGNORE_2, ResourceType::TEXTURE, IGNORE_4, CommonUsage::ROM}, AccessFlags::COMPUTE_SHADER_READ_TEXTURE},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, IGNORE_2, IGNORE_3, IGNORE_4, IGNORE_5}, AccessFlags::COMPUTE_SHADER_READ_OTHER},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::COPY, IGNORE_3, IGNORE_4, IGNORE_5}, AccessFlags::TRANSFER_READ},
        {{MemoryAccess::READ_ONLY, MemoryUsage::HOST, IGNORE_2, IGNORE_3, IGNORE_4, IGNORE_5}, AccessFlags::HOST_READ},
        {{MemoryAccess::READ_ONLY, MemoryUsage::DEVICE, PassType::PRESENT, IGNORE_3, IGNORE_4, IGNORE_5}, AccessFlags::PRESENT},

        {{MemoryAccess::WRITE_ONLY, MemoryUsage::DEVICE, PassType::RASTER, IGNORE_3, ShaderStageFlags::VERTEX, IGNORE_5}, AccessFlags::VERTEX_SHADER_WRITE},
        {{MemoryAccess::WRITE_ONLY, MemoryUsage::DEVICE, PassType::RASTER, IGNORE_3, ShaderStageFlags::FRAGMENT, IGNORE_5}, AccessFlags::FRAGMENT_SHADER_WRITE},
        {{MemoryAccess::WRITE_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::TEXTURE, ShaderStageFlags::FRAGMENT, CommonUsage::IB_OR_CA}, AccessFlags::COLOR_ATTACHMENT_WRITE},
        {{MemoryAccess::WRITE_ONLY, MemoryUsage::DEVICE, PassType::RASTER, ResourceType::TEXTURE, ShaderStageFlags::FRAGMENT, CommonUsage::VB_OR_DS}, AccessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE},
        {{MemoryAccess::WRITE_ONLY, MemoryUsage::DEVICE, IGNORE_2, IGNORE_3, IGNORE_4, IGNORE_5}, AccessFlags::COMPUTE_SHADER_WRITE},
        {{MemoryAccess::WRITE_ONLY, MemoryUsage::DEVICE, PassType::COPY, IGNORE_3, IGNORE_4, IGNORE_5}, AccessFlags::TRANSFER_WRITE},
        {{MemoryAccess::WRITE_ONLY, MemoryUsage::HOST, IGNORE_2, IGNORE_3, IGNORE_4, IGNORE_5}, AccessFlags::HOST_WRITE},
    };

}  // namespace

std::pair<gfx::GFXObject*, gfx::GFXObject*> getBarrier(const ResourceBarrier& barrierInfo, const DevicePassResourceTable* dictPtr) noexcept {


    std::pair<gfx::GFXObject*, gfx::GFXObject*> res;

    auto extract = [&](const AccessStatus& status, ccstd::vector<AccessKey>& keys) {
        if(!status.access) {
            keys.emplace_back();
        }

        if(hasFlag(status.access, MemoryAccess::READ_ONLY)) {
            keys.emplace_back(AccessKey{MemoryAccess::READ_ONLY});
        }
    };

    const auto& dict = *dictPtr;
    if(barrierInfo.resourceType == ResourceType::BUFFER) {
        gfx::Buffer* gfxBuffer{nullptr};
        if (hasFlag(barrierInfo.endStatus.access, MemoryAccess::WRITE_ONLY)) {
            gfxBuffer = dict.getWrite(static_cast<BufferHandle>(barrierInfo.handle));
        } else {
            gfxBuffer = dict.getRead(static_cast<BufferHandle>(barrierInfo.handle));
        }
        gfx::BufferBarrierInfo info;

        auto getGFXAccess = [&gfxBuffer](const AccessStatus& status) {
            auto usage = gfxBuffer->getUsage();
            auto memUsage = gfxBuffer->getMemUsage();

            AccessFlags flags{AccessFlags::NONE};
            if(hasFlag(usage, BufferUsage::INDIRECT)) {
                flags |= AccessFlags::INDIRECT_BUFFER;
            }
            if(hasFlag(usage, BufferUsage::INDEX)) {
                flags |= AccessFlags::INDEX_BUFFER;
            }
            if(hasFlag(usage, BufferUsage::VERTEX)) {
                flags |= AccessFlags::VERTEX_BUFFER;
            }

            if(memUsage == MemoryUsage::HOST) {
                if(hasFlag(status.access, MemoryAccess::READ_ONLY)) {
                    flags |= AccessFlags::HOST_READ;
                }
                if(hasFlag(status.access, MemoryAccess::WRITE_ONLY)) {
                    flags |= AccessFlags::HOST_WRITE;
                }
            } 

            if(hasFlag(status.access, MemoryAccess::READ_ONLY)) {
                if(status.passType == PassType::RASTER) {
                    if(hasFlag(status.visibility, ShaderStageFlags::VERTEX)) {
                        if(hasFlag(usage, BufferUsage::UNIFORM)) {
                            flags |= AccessFlags::VERTEX_SHADER_READ_UNIFORM_BUFFER;
                        } else {
                            flags |= AccessFlags::VERTEX_SHADER_READ_OTHER;
                        }
                    } 
                    if(hasFlag(status.visibility, ShaderStageFlags::FRAGMENT)){
                        if(hasFlag(usage, BufferUsage::UNIFORM)) {
                            flags |= AccessFlags::FRAGMENT_SHADER_READ_UNIFORM_BUFFER;
                        } else {
                            flags |= AccessFlags::FRAGMENT_SHADER_READ_OTHER;
                        }
                    }
                } else if(status.passType == PassType::COMPUTE) {
                    if(hasFlag(usage, BufferUsage::UNIFORM)) {
                        flags |= AccessFlags::COMPUTE_SHADER_READ_UNIFORM_BUFFER;
                    } else {
                        flags |= AccessFlags::COMPUTE_SHADER_READ_OTHER;
                    }
                } else if(status.passType == PassType::COPY) {
                    flags |= AccessFlags::TRANSFER_READ;
                } else if(status.passType == PassType::RAYTRACE){
                    if(hasFlag(usage, BufferUsage::UNIFORM)) {
                        flags |= AccessFlags::COMPUTE_SHADER_READ_TEXTURE;
                    } else {
                        flags |= AccessFlags::COMPUTE_SHADER_READ_OTHER;
                    }
                }
            }
            if(hasFlag(status.access, MemoryAccess::WRITE_ONLY)) {
                if(status.passType == PassType::RASTER) {
                    if(hasFlag(status.visibility, ShaderStageFlags::VERTEX)) {
                        flags |= AccessFlags::VERTEX_SHADER_WRITE;
                    }
                    if(hasFlag(status.visibility, ShaderStageFlags::FRAGMENT)) {
                        flags |= AccessFlags::FRAGMENT_SHADER_WRITE;
                    }
                } else if(status.passType == PassType::COPY) {
                    flags |= AccessFlags::TRANSFER_WRITE;
                } else if(status.passType == PassType::COMPUTE || status.passType == PassType::RAYTRACE){
                    flags |= AccessFlags::COMPUTE_SHADER_WRITE;
                }
            }
            return flags;
        };

        info.prevAccesses = getGFXAccess(barrierInfo.beginStatus);
        info.nextAccesses = getGFXAccess(barrierInfo.endStatus);
        info.offset = static_cast<uint32_t>(barrierInfo.bufferRange.base);
        info.size = static_cast<uint32_t>(barrierInfo.bufferRange.len);
        info.type = barrierInfo.barrierType;

        res.first = gfx::Device::getInstance()->getBufferBarrier(info);
        res.second = gfxBuffer;
    } else if(barrierInfo.resourceType == ResourceType::TEXTURE) {
        gfx::Texture* gfxTexture{nullptr};
        if (hasFlag(barrierInfo.beginStatus.access, MemoryAccess::WRITE_ONLY)) {
            gfxTexture = dict.getWrite(static_cast<TextureHandle>(barrierInfo.handle));
        } else {
            gfxTexture = dict.getRead(static_cast<TextureHandle>(barrierInfo.handle));
        }
        gfx::TextureBarrierInfo info;

        auto getGFXAccess = [&gfxTexture](const AccessStatus& status) {
            auto usage = gfxTexture->getInfo().usage;

            AccessFlags flags{AccessFlags::NONE};
            if(hasFlag(status.access, MemoryAccess::READ_ONLY)) {
                if(status.passType == PassType::RASTER) {
                    if(hasFlag(status.visibility, ShaderStageFlags::VERTEX)) {
                        if(hasFlag(usage, TextureUsage::SAMPLED)) {
                            flags |= AccessFlags::VERTEX_SHADER_READ_TEXTURE;
                        } else {
                            flags |= AccessFlags::VERTEX_SHADER_READ_OTHER;
                        }
                    } 
                    if(hasFlag(status.visibility, ShaderStageFlags::FRAGMENT)){
                        if(hasFlag(usage, TextureUsage::COLOR_ATTACHMENT)) {
                            if(hasFlag(usage, TextureUsage::INPUT_ATTACHMENT)) {
                                flags |= AccessFlags::FRAGMENT_SHADER_READ_COLOR_INPUT_ATTACHMENT;
                            } else {
                                flags |= AccessFlags::COLOR_ATTACHMENT_READ;
                            }
                        } else if(hasFlag(usage, TextureUsage::DEPTH_STENCIL_ATTACHMENT)) {
                            if(hasFlag(usage, TextureUsage::INPUT_ATTACHMENT)) {
                                flags |= AccessFlags::FRAGMENT_SHADER_READ_DEPTH_STENCIL_INPUT_ATTACHMENT;
                            } else {
                                flags |= AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ;
                            }
                        }
                    }
                }
            } else if(status.passType == PassType::COMPUTE) {
                if(hasFlag(usage, TextureUsage::SAMPLED)) {
                    flags |= AccessFlags::COMPUTE_SHADER_READ_TEXTURE;
                } else {
                    flags |= AccessFlags::COMPUTE_SHADER_READ_OTHER;
                }
            } else if(status.passType == PassType::COPY) {
                flags |= AccessFlags::TRANSFER_READ;
            } else if(status.passType == PassType::RAYTRACE){
                if(hasFlag(usage, TextureUsage::SAMPLED)) {
                    flags |= AccessFlags::COMPUTE_SHADER_READ_TEXTURE;
                } else {
                    flags |= AccessFlags::COMPUTE_SHADER_READ_OTHER;
                }
            }

            if(hasFlag(status.access, MemoryAccess::WRITE_ONLY)) {
                if(status.passType == PassType::RASTER) {
                    if(hasFlag(status.visibility, ShaderStageFlags::VERTEX)) {
                        flags |= AccessFlags::VERTEX_SHADER_WRITE;
                    }
                    if(hasFlag(status.visibility, ShaderStageFlags::FRAGMENT)) {
                        if(hasFlag(usage, TextureUsage::COLOR_ATTACHMENT)) {
                            flags |= AccessFlags::COLOR_ATTACHMENT_WRITE;
                        } else if(hasFlag(usage, TextureUsage::DEPTH_STENCIL_ATTACHMENT)) {
                            flags |= AccessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE;
                        } else {
                            flags |= AccessFlags::FRAGMENT_SHADER_WRITE;
                        }
                    }
                } else if(status.passType == PassType::COPY) {
                    flags |= AccessFlags::TRANSFER_WRITE;
                } else if(status.passType == PassType::COMPUTE || status.passType == PassType::RAYTRACE){
                    flags |= AccessFlags::COMPUTE_SHADER_WRITE;
                } else if (status.passType == PassType::PRESENT) {
                    flags |= AccessFlags::PRESENT;
                }
            }
            return flags;
        };

        info.type = barrierInfo.barrierType;
        info.prevAccesses = getGFXAccess(barrierInfo.beginStatus);
        info.nextAccesses = getGFXAccess(barrierInfo.endStatus);
        info.baseMipLevel = static_cast<uint32_t>(barrierInfo.mipRange.base);
        info.levelCount = static_cast<uint32_t>(barrierInfo.mipRange.len);
        info.baseSlice = static_cast<uint32_t>(barrierInfo.layerRange.base);
        info.sliceCount = static_cast<uint32_t>(barrierInfo.layerRange.len);

        res.first = gfx::Device::getInstance()->getTextureBarrier(info);
        res.second = gfxTexture;       
    }

    return res;
}

} // namespace framegraph
} // namespace cc
