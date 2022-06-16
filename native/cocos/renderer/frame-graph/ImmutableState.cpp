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
#include "FrameGraph.h"

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

std::pair<gfx::GFXObject*, gfx::GFXObject*> getBarrier(const ResourceBarrier& barrierInfo, const FrameGraph* graph) noexcept {
    std::pair<gfx::GFXObject*, gfx::GFXObject*> res;

    if(barrierInfo.resourceType == ResourceType::BUFFER) {
        auto resNode = (*graph).getResourceNode(static_cast<BufferHandle>(barrierInfo.handle));
        auto* gfxBuffer = static_cast<ResourceEntry<Buffer> *>(resNode.virtualResource)->getDeviceResource();
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
        info.offset = barrierInfo.bufferRange.base;
        info.offset = barrierInfo.bufferRange.len;
        info.type = barrierInfo.barrierType;

        res.first = gfx::Device::getInstance()->getBufferBarrier(info);
        res.second = gfxBuffer;
    } else if(barrierInfo.resourceType == ResourceType::TEXTURE) {
        auto resNode = (*graph).getResourceNode(static_cast<TextureHandle>(barrierInfo.handle));
        auto* gfxTexture = static_cast<ResourceEntry<Texture> *>(resNode.virtualResource)->getDeviceResource();
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
        info.baseMipLevel = barrierInfo.mipRange.base;
        info.levelCount = barrierInfo.mipRange.len;
        info.baseSlice = barrierInfo.layerRange.base;
        info.sliceCount = barrierInfo.layerRange.len;

        res.first = gfx::Device::getInstance()->getTextureBarrier(info);
        res.second = gfxTexture;       
    }

    return res;
}

} // namespace framegraph
} // namespace cc
