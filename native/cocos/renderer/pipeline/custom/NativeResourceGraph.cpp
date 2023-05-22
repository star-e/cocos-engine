/****************************************************************************
 Copyright (c) 2021-2023 Xiamen Yaji Software Co., Ltd.

 http://www.cocos.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
****************************************************************************/

#include "NativePipelineTypes.h"
#include "RenderGraphGraphs.h"
#include "RenderGraphTypes.h"
#include "cocos/renderer/gfx-base/GFXDevice.h"
#include "details/Range.h"
#include "gfx-base/GFXDef-common.h"
#include "pipeline/custom/RenderCommonFwd.h"

namespace cc {

namespace render {

ResourceGroup::~ResourceGroup() noexcept {
    for (const auto& buffer : instancingBuffers) {
        buffer->clear();
    }
}

void ProgramResource::syncResources() noexcept {
    for (auto&& [nameID, buffer] : uniformBuffers) {
        buffer.bufferPool.syncResources();
    }
    descriptorSetPool.syncDescriptorSets();
}

void LayoutGraphNodeResource::syncResources() noexcept {
    for (auto&& [nameID, buffer] : uniformBuffers) {
        buffer.bufferPool.syncResources();
    }
    descriptorSetPool.syncDescriptorSets();
    for (auto&& [programName, programResource] : programResources) {
        programResource.syncResources();
    }
}

void NativeRenderContext::clearPreviousResources(uint64_t finishedFenceValue) noexcept {
    for (auto iter = resourceGroups.begin(); iter != resourceGroups.end();) {
        if (iter->first <= finishedFenceValue) {
            iter = resourceGroups.erase(iter);
        } else {
            break;
        }
    }
    for (auto& node : layoutGraphResources) {
        node.syncResources();
    }
}

namespace {

gfx::BufferInfo getBufferInfo(const ResourceDesc& desc) {
    using namespace gfx; // NOLINT(google-build-using-namespace)

    BufferUsage usage = BufferUsage::TRANSFER_SRC | BufferUsage::TRANSFER_DST;
    if (any(desc.flags & ResourceFlags::UNIFORM)) {
        usage |= BufferUsage::UNIFORM;
    }
    if (any(desc.flags & ResourceFlags::STORAGE)) {
        usage |= BufferUsage::STORAGE;
    }
    if (any(desc.flags & ResourceFlags::INDIRECT)) {
        usage |= BufferUsage::INDIRECT;
    }

    return {
        usage,
        MemoryUsage::DEVICE,
        desc.width,
        1U, // stride should not be used
        BufferFlagBit::NONE,
    };
}

gfx::TextureInfo getTextureInfo(const ResourceDesc& desc, bool bCube = false) {
    using namespace gfx; // NOLINT(google-build-using-namespace)
    // type
    auto type = TextureType::TEX1D;
    switch (desc.dimension) {
        case ResourceDimension::TEXTURE1D:
            if (desc.depthOrArraySize > 1) {
                type = TextureType::TEX1D_ARRAY;
            } else {
                type = TextureType::TEX1D;
            }
            break;
        case ResourceDimension::TEXTURE2D:
            if (desc.depthOrArraySize > 1) {
                if (bCube) {
                    type = TextureType::CUBE;
                } else {
                    type = TextureType::TEX2D_ARRAY;
                }
            } else {
                type = TextureType::TEX2D;
            }
            break;
        case ResourceDimension::TEXTURE3D:
            type = TextureType::TEX3D;
            break;
        case ResourceDimension::BUFFER:
            CC_EXPECTS(false);
    }

    // usage
    TextureUsage usage = TextureUsage::SAMPLED | TextureUsage::TRANSFER_SRC | TextureUsage::TRANSFER_DST;
    if (any(desc.flags & ResourceFlags::COLOR_ATTACHMENT)) {
        usage |= TextureUsage::COLOR_ATTACHMENT;
    }
    if (any(desc.flags & ResourceFlags::DEPTH_STENCIL_ATTACHMENT)) {
        CC_EXPECTS(!any(desc.flags & ResourceFlags::COLOR_ATTACHMENT));
        usage |= TextureUsage::DEPTH_STENCIL_ATTACHMENT;
    }
    if (any(desc.flags & ResourceFlags::INPUT_ATTACHMENT)) {
        usage |= TextureUsage::INPUT_ATTACHMENT;
    }
    if (any(desc.flags & ResourceFlags::STORAGE)) {
        usage |= TextureUsage::STORAGE;
    }
    if (any(desc.flags & ResourceFlags::SHADING_RATE)) {
        usage |= TextureUsage::SHADING_RATE;
    }

    return {
        type,
        usage,
        desc.format,
        desc.width,
        desc.height,
        desc.textureFlags,
        type == TextureType::TEX3D ? 1U : desc.depthOrArraySize,
        desc.mipLevels,
        desc.sampleCount,
        type == TextureType::TEX3D ? desc.depthOrArraySize : 1U,
        nullptr,
    };
}

gfx::TextureViewInfo getTextureViewInfo(gfx::Texture* texture, const ResourceDesc& desc, uint32_t planeID) {
    gfx::TextureViewInfo viewInfo;
    viewInfo.texture = texture;
    viewInfo.format = texture->getFormat();
    viewInfo.type = gfx::TextureType::TEX2D;
    viewInfo.baseLayer = planeID;
    return viewInfo;
};

} // namespace

bool ManagedTexture::checkResource(const ResourceDesc& desc) const {
    if (!texture) {
        return false;
    }
    const auto& info = texture->getInfo();
    return desc.width == info.width && desc.height == info.height && desc.format == info.format;
}

void ResourceGraph::validateSwapchains() {
    bool swapchainInvalidated = false;
    for (auto& sc : swapchains) {
        if (sc.generation != sc.swapchain->getGeneration()) {
            swapchainInvalidated = true;
            sc.generation = sc.swapchain->getGeneration();
        }
    }
    if (swapchainInvalidated) {
        renderPasses.clear();
    }
}

void ResourceGraph::mount(gfx::Device* device, vertex_descriptor vertID) {
    std::ignore = device;
    auto& resg = *this;
    const auto& desc = get(ResourceGraph::DescTag{}, *this, vertID);
    visitObject(
        vertID, resg,
        [&](const ManagedResource& resource) {
            // to be removed
        },
        [&](ManagedBuffer& buffer) {
            if (!buffer.buffer) {
                auto info = getBufferInfo(desc);
                buffer.buffer = device->createBuffer(info);
            }
            CC_ENSURES(buffer.buffer);
            buffer.fenceValue = nextFenceValue;
        },
        [&](ManagedTexture& texture) {
            if (!texture.checkResource(desc)) {
                auto info = getTextureInfo(desc);
                texture.texture = device->createTexture(info);
            }
            CC_ENSURES(texture.texture);
            texture.fenceValue = nextFenceValue;
        },
        [&](const IntrusivePtr<gfx::Buffer>& buffer) {
            CC_EXPECTS(buffer);
            std::ignore = buffer;
        },
        [&](const IntrusivePtr<gfx::Texture>& texture) {
            CC_EXPECTS(texture);
            std::ignore = texture;
        },
        [&](const IntrusivePtr<gfx::Framebuffer>& fb) {
            CC_EXPECTS(fb);
            std::ignore = fb;
        },
        [&](const RenderSwapchain& queue) {
            CC_EXPECTS(queue.swapchain);
            std::ignore = queue;
        });
}

void mountView(gfx::Device* device,
    ResourceGraph& resg,
    ResourceGraph::vertex_descriptor vertID,
    const ccstd::pmr::string& name0,
    uint32_t planeID) {

    auto* originTexture = resg.getTexture(vertID);
    const auto& desc = get(ResourceGraph::DescTag{}, resg, vertID);
    CC_ASSERT(originTexture);

    auto resID = findVertex(name0, resg);
    if (resID == ResourceGraph::null_vertex()) {
        resID = addVertex(
            ManagedTextureTag{},
            std::forward_as_tuple(name0.c_str()),
            std::forward_as_tuple(desc),
            std::forward_as_tuple(ResourceResidency::MANAGED),
            std::forward_as_tuple(),
            std::forward_as_tuple(),
            std::forward_as_tuple(),
            resg);
    }
    visitObject(
        resID, resg,
        [&](ManagedTexture& texture) {
            if (!texture.texture) {
                auto& viewInfo = getTextureViewInfo(originTexture, desc, planeID);
                texture.texture = device->createTexture(viewInfo);
            }
            texture.fenceValue = resg.nextFenceValue;
        },
        [&](const auto& res) {
            CC_EXPECTS(false);
            std::ignore = res;
        });
}

const char* ResourceGraph::getViewLocalName(const ccstd::pmr::string& name) const {
    auto index = name.find_first_of("_$_");
    std::string_view view;
    if (index == name.npos) {
        view = std::string_view(name);
    } else {
        view = std::string_view{name.c_str(), index};
    }

    thread_local char localChars[256];
    memcpy(localChars, view.data(), view.length());
    localChars[view.length()] = '\0';
    return localChars;
}

void ResourceGraph::mount(gfx::Device* device, vertex_descriptor vertID, const ccstd::pmr::string& name, const ccstd::pmr::string& name0, const ccstd::pmr::string& name1) {
    mount(device, vertID);
    auto* texture = getTexture(vertID);
    const auto& desc = get(ResourceGraph::DescTag{}, *this, vertID);
    CC_ASSERT(texture);

    if (!name0.empty() && name0 != "_") {
        mountView(device, *this, vertID, name, 0);
    }
    if (!name1.empty() && name1 != "_") {
        mountView(device, *this, vertID, name, 1);
    }
}

void ResourceGraph::unmount(uint64_t completedFenceValue) {
    auto& resg = *this;
    for (const auto& vertID : makeRange(vertices(resg))) {
        // here msvc has strange behaviour when using visitObject
        // we use if-else instead.
        if (holds<ManagedBufferTag>(vertID, resg)) {
            auto& buffer = get(ManagedBufferTag{}, vertID, resg);
            if (buffer.buffer && buffer.fenceValue <= completedFenceValue) {
                buffer.buffer.reset();
            }
        } else if (holds<ManagedTextureTag>(vertID, resg)) {
            auto& texture = get(ManagedTextureTag{}, vertID, resg);
            if (texture.texture && texture.fenceValue <= completedFenceValue) {
                invalidatePersistentRenderPassAndFramebuffer(texture.texture.get());
                texture.texture.reset();
                const auto& traits = get(ResourceGraph::TraitsTag{}, resg, vertID);
                if (traits.hasSideEffects()) {
                    auto& states = get(ResourceGraph::StatesTag{}, resg, vertID);
                    states.states = cc::gfx::AccessFlagBit::NONE;
                }
            }
        }
    }
}

bool ResourceGraph::isTexture(vertex_descriptor resID) const noexcept {
    return visitObject(
        resID, *this,
        [&](const ManagedBuffer& res) {
            std::ignore = res;
            return false;
        },
        [&](const IntrusivePtr<gfx::Buffer>& res) {
            std::ignore = res;
            return false;
        },
        [&](const auto& res) {
            std::ignore = res;
            return true;
        });
}

gfx::Buffer* ResourceGraph::getBuffer(vertex_descriptor resID) {
    gfx::Buffer* buffer = nullptr;
    visitObject(
        resID, *this,
        [&](const ManagedBuffer& res) {
            buffer = res.buffer.get();
        },
        [&](const IntrusivePtr<gfx::Buffer>& buf) {
            buffer = buf.get();
        },
        [&](const auto& buffer) {
            std::ignore = buffer;
            CC_EXPECTS(false);
        });
    CC_ENSURES(buffer);

    return buffer;
}

gfx::Texture* ResourceGraph::getTexture(vertex_descriptor resID) {
    gfx::Texture* texture = nullptr;
    visitObject(
        resID, *this,
        [&](const ManagedTexture& res) {
            texture = res.texture.get();
        },
        [&](const IntrusivePtr<gfx::Texture>& tex) {
            texture = tex.get();
        },
        [&](const IntrusivePtr<gfx::Framebuffer>& fb) {
            CC_EXPECTS(fb->getColorTextures().size() == 1);
            CC_EXPECTS(fb->getColorTextures().at(0));
            texture = fb->getColorTextures()[0];
        },
        [&](const RenderSwapchain& sc) {
            texture = sc.swapchain->getColorTexture();
        },
        [&](const auto& buffer) {
            std::ignore = buffer;
            CC_EXPECTS(false);
        });
    CC_ENSURES(texture);

    return texture;
}

void ResourceGraph::invalidatePersistentRenderPassAndFramebuffer(gfx::Texture* pTexture) {
    if (!pTexture) {
        return;
    }
    for (auto iter = renderPasses.begin(); iter != renderPasses.end();) {
        const auto& pass = iter->second;
        bool bErase = false;
        for (const auto& color : pass.framebuffer->getColorTextures()) {
            if (color == pTexture) {
                bErase = true;
                break;
            }
        }
        if (pass.framebuffer->getDepthStencilTexture() == pTexture) {
            bErase = true;
        }
        if (bErase) {
            iter = renderPasses.erase(iter);
        } else {
            ++iter;
        }
    }
}

} // namespace render

} // namespace cc
