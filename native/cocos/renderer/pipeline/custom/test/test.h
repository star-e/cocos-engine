/*Copyright (c) 2021-2022 Xiamen Yaji Software Co., Ltd.

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
#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include "../FGDispatcherTypes.h"
#include "../LayoutGraphGraphs.h"
#include "../NativePipelineTypes.h"
#include "../RenderGraphGraphs.h"
#include "../RenderGraphTypes.h"
#include "gfx-base/GFXDef-common.h"
#include "../Range.h"
#include "../../Enum.h"
#include "frame-graph/FrameGraph.h"
#include "cocos/scene/RenderScene.h"
#include "cocos/scene/RenderWindow.h"

namespace cc {

namespace render {

using ccstd::pmr::string;
using std::map;
using std::pair;
using std::vector;
using gfx::PassType;
using ViewInfo = vector<pair<PassType, vector<vector<vector<string>>>>>;
using ResourceInfo = vector<pair<string, gfx::DescriptorType>>;
using LayoutUnit = std::tuple<string, uint32_t, gfx::ShaderStageFlagBit>;
using LayoutInfo = vector<vector<LayoutUnit>>;

void testData(const ViewInfo &rasterData, const ResourceInfo &rescInfo, const LayoutInfo &layoutInfo, RenderGraph &renderGraph, ResourceGraph &rescGraph, LayoutGraphData &layoutGraphData, framegraph::FrameGraph& framegraph, const ccstd::vector<scene::Camera*>& cameras) {
    for (const auto &resc : rescInfo) {
        string name = std::get<0>(resc);
        auto rescVertexID = add_vertex(rescGraph, ManagedTag{}, name.c_str());
        rescGraph.descs.emplace_back(ResourceDesc{resc.second == gfx::DESCRIPTOR_BUFFER_TYPE ? ResourceDimension::BUFFER : ResourceDimension::TEXTURE2D});
    }

    const auto &mem_resource = layoutGraphData.get_allocator();
    auto &stages = layoutGraphData.stages;
    stages.resize(layoutInfo.size());

    for (size_t i = 0; i < layoutInfo.size(); ++i) {
        const ccstd::string passName = "pass" + std::to_string(i);
        auto layoutVtxID = add_vertex(layoutGraphData, RenderStageTag{}, passName.c_str());

        for (size_t j = 0; j < layoutInfo[i].size(); ++j) {
            const auto &renderStageInfo = layoutInfo[i];
            for (const auto &layoutUnit : renderStageInfo) {
                const auto &rescName = std::get<0>(layoutUnit);
                const auto &nameID = std::get<1>(layoutUnit);
                const auto &shaderStage = std::get<2>(layoutUnit);
                if (layoutGraphData.attributeIndex.find(rescName) == layoutGraphData.attributeIndex.end()) {
                    layoutGraphData.attributeIndex.emplace(std::make_pair<string, NameLocalID>(rescName.c_str(), NameLocalID{nameID}));
                }
                stages[i].descriptorVisibility.emplace(NameLocalID{nameID}, shaderStage);
            }
        }
    }

    string tempPresentHandle;

    uint32_t passCount = 0;
    for (size_t i = 0; i < rasterData.size(); ++i) {
        const auto &pass = rasterData[i];
        switch (pass.first) {
            case PassType::RASTER: {
                // const string name = pass.first;
                const auto &subpasses = pass.second;
                for (size_t j = 0; j < subpasses.size(); ++j) {
                    const ccstd::string name = "pass" + std::to_string(passCount++);
                    const auto vertexID = add_vertex(renderGraph, RasterTag{}, name.c_str());
                    assert(subpasses[j].size() == 2); // inputs and outputs
                    const auto &attachments = subpasses[j];
                    auto &raster = get(RasterTag{}, vertexID, renderGraph);
                    bool isOutput = false;

                    for (size_t k = 0; k < attachments.size(); ++k) {
                        for (size_t l = 0; l < attachments[k].size(); ++l) {
                            const auto &inputsOrOutputs = attachments[k];
                            const auto &viewName = inputsOrOutputs[l];
                            raster.rasterViews.emplace(viewName.c_str(), RasterView{
                                                           viewName.c_str(),
                                                           isOutput ? AccessType::WRITE : AccessType::READ,
                                                           AttachmentType::RENDER_TARGET,
                                                           gfx::LoadOp::CLEAR,
                                                           gfx::StoreOp::STORE,
                                                           gfx::ClearFlagBit::ALL,
                                                           gfx::Color({1.0, 0.0, 0.0, 1.0}),
                                                       });
                            tempPresentHandle = viewName;
                        }
                        isOutput = true;
                    }
                }
            }
        }
    }


    FrameGraphDispatcher fgDispatcher(rescGraph, renderGraph, layoutGraphData, layoutGraphData.resource(), layoutGraphData.resource());
    fgDispatcher.enableMemoryAliasing(true);
    fgDispatcher.enablePassReorder(false);
    fgDispatcher.setParalellWeight(0.4);
    fgDispatcher.run();

    const auto &barriers = fgDispatcher.getBarriers();

    struct RenderData {
        ccstd::vector<std::pair<AccessType, framegraph::TextureHandle>> outputTexes;
    };

    for (const auto passID : makeRange(vertices(renderGraph))) {
        visitObject(passID, renderGraph,
                    [&](const RasterPass &pass){
                        RenderData tmpData;
                        auto forwardSetup = [&](framegraph::PassNodeBuilder &builder, RenderData &data) {
                            for(const auto& rasterView : pass.rasterViews) {
                                const auto handle = framegraph::FrameGraph::stringToHandle(rasterView.first.c_str());
                                auto typedHandle = builder.readFromBlackboard(handle);
                                data.outputTexes.push_back({});
                                auto &lastTex = data.outputTexes.back();
                                framegraph::Texture::Descriptor colorTexInfo;
                                colorTexInfo.format = gfx::Format::RGBA8;

                                if (rasterView.second.accessType == AccessType::READ) {
                                    colorTexInfo.usage = gfx::TextureUsage::INPUT_ATTACHMENT | gfx::TextureUsage::COLOR_ATTACHMENT;
                                }
                                if (rasterView.second.accessType == AccessType::WRITE) {
                                    colorTexInfo.usage = gfx::TextureUsage::COLOR_ATTACHMENT;
                                }
                                lastTex.first = rasterView.second.accessType;
                                lastTex.second = static_cast<framegraph::TextureHandle>(typedHandle);

                                if (framegraph::Handle::IndexType(typedHandle) == framegraph::Handle::UNINITIALIZED) {
                                    colorTexInfo.width = 960;
                                    colorTexInfo.height = 640;

                                    lastTex.second = builder.create(handle, colorTexInfo);
                                }

                                framegraph::RenderTargetAttachment::Descriptor colorAttachmentInfo;
                                colorAttachmentInfo.usage = rasterView.second.attachmentType == AttachmentType::RENDER_TARGET ?
                                    framegraph::RenderTargetAttachment::Usage::COLOR : framegraph::RenderTargetAttachment::Usage::DEPTH_STENCIL;
                                colorAttachmentInfo.clearColor = rasterView.second.clearColor;
                                colorAttachmentInfo.loadOp = rasterView.second.loadOp;
                                if (rasterView.second.accessType == AccessType::WRITE) {
                                    lastTex.second = builder.write(lastTex.second, colorAttachmentInfo);
                                    builder.writeToBlackboard(handle, lastTex.second);
                                    colorAttachmentInfo.beginAccesses = colorAttachmentInfo.endAccesses = gfx::AccessFlagBit::COLOR_ATTACHMENT_WRITE;
                                } else {
                                    colorAttachmentInfo.beginAccesses = colorAttachmentInfo.endAccesses = gfx::AccessFlagBit::COLOR_ATTACHMENT_READ;
                                    builder.read(lastTex.second);
                                }
                            }
                            builder.setViewport({0U, 640U, 0U, 960U, 0.0F, 1.0F}, {0U, 0U, 960U, 640U});

                            if (barriers.find(passID + 1) == barriers.end()) {
                                return;
                            }
                            const auto &barrier = barriers.at(passID + 1);
                            auto fullfillBarrier = [&](bool front) {
                                const auto parts = front ? barrier.frontBarriers : barrier.rearBarriers;
                                for (const auto &resBarrier : parts) {
                                    const auto &name = get(ResourceGraph::Name, rescGraph, resBarrier.resourceID);
                                    const auto &desc = get(ResourceGraph::Desc, rescGraph, resBarrier.resourceID);
                                    auto type = desc.dimension == ResourceDimension::BUFFER ? cc::framegraph::ResourceType::BUFFER : cc::framegraph::ResourceType::TEXTURE;
                                    framegraph::Range layerRange;
                                    framegraph::Range mipRange;
                                    if(type == framegraph::ResourceType::BUFFER) {
                                        auto bufferRange = ccstd::get<BufferRange>(resBarrier.beginStatus.range);
                                        layerRange = {0, 0};
                                        mipRange = {bufferRange.offset, bufferRange.size};
                                    } else {
                                        auto textureRange = ccstd::get<TextureRange>(resBarrier.beginStatus.range);
                                        layerRange = {textureRange.firstSlice, textureRange.numSlices};
                                        mipRange = {textureRange.mipLevel, textureRange.levelCount};
                                    }
                                    builder.addBarrier(cc::framegraph::ResourceBarrier{
                                                           type,
                                                           resBarrier.type,
                                                           builder.readFromBlackboard(framegraph::FrameGraph::stringToHandle(name.c_str())),
                                                           {
                                                               resBarrier.beginStatus.passType,
                                                               resBarrier.beginStatus.visibility,
                                                               resBarrier.beginStatus.access
                                                           },
                                                           {    resBarrier.endStatus.passType,
                                                               resBarrier.endStatus.visibility,
                                                               resBarrier.endStatus.access
                                                           },
                                                           layerRange,
                                                           mipRange,
                                                       },
                                                       front);
                                }
                            };

                            fullfillBarrier(true);
                            //...
                            fullfillBarrier(false);

                        };



                        auto forwardExec = [](const RenderData & data,
                                              const framegraph::DevicePassResourceTable &table) {
                            /*for(const auto& pair: data.outputTexes) {
                                if(pair.first == AccessType::WRITE) {
                                    table.getWrite(pair.second);
                                }
                                if(pair.first == AccessType::READ) {
                                    table.getRead(pair.second);
                                }
                                if(pair.first == AccessType::READ_WRITE) {
                                    table.getRead(pair.second);
                                    table.getWrite(pair.second);
                                }
                            }*/
                        };

                        auto passHandle = framegraph::FrameGraph::stringToHandle(get(RenderGraph::Name, renderGraph, passID).c_str());

                        framegraph.addPass<RenderData>(static_cast<uint>(ForwardInsertPoint::IP_FORWARD), passHandle, forwardSetup, forwardExec);


                    },
                    [&](const ComputePass &pass){},
                    [&](const CopyPass &pass) {},
                    [&](const RaytracePass &pass) {},
                    [&](const PresentPass &pass) {},
                    [&](const auto & /*pass*/) {}
                    );

    }
    framegraph.presentFromBlackboard(framegraph::FrameGraph::stringToHandle(tempPresentHandle.c_str()),
                                     cameras[0]->getWindow()->getFramebuffer()->getColorTextures()[0], true);
};

void testCase1(framegraph::FrameGraph& framegraph, const ccstd::vector<scene::Camera*>& cameras) {
    boost::container::pmr::memory_resource *resource = boost::container::pmr::get_default_resource();
    RenderGraph renderGraph(resource);
    ResourceGraph rescGraph(resource);
    LayoutGraphData layoutGraph(resource);

    ResourceInfo resources = {
        {"0", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"1", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"2", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"3", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"4", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"5", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"6", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"7", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"8", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"9", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"10", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"11", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"12", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"13", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"14", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"15", gfx::DESCRIPTOR_TEXTURE_TYPE},
    };

    ViewInfo data = {
        {
            PassType::RASTER,
            {
                {{}, {"0", "1", "2"}},
                {{"0", "1", "2", "4"}, {"3"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"3"}, {"5"}},
            },
        },
        {
            PassType::RASTER,//present
            {
                {{"5"}, {}},
            },
        },
    };

    using ShaderStageMap = map<string, gfx::ShaderStageFlagBit>;

    LayoutInfo layoutInfo = {
        {
            {"0", 0, gfx::ShaderStageFlagBit::VERTEX},
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
            {"2", 2, gfx::ShaderStageFlagBit::VERTEX},
        },
        {
            {"0", 0, gfx::ShaderStageFlagBit::VERTEX},
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
            {"2", 2, gfx::ShaderStageFlagBit::VERTEX},
            {"3", 3, gfx::ShaderStageFlagBit::FRAGMENT},
            {"4", 4, gfx::ShaderStageFlagBit::VERTEX},
        },
        {
            {"3", 3, gfx::ShaderStageFlagBit::VERTEX},
            {"5", 5, gfx::ShaderStageFlagBit::VERTEX},
        },
        {
            {"5", 5, gfx::ShaderStageFlagBit::COMPUTE},
        }};

    testData(data, resources, layoutInfo, renderGraph, rescGraph, layoutGraph, framegraph, cameras);
    // for(const auto* camera : cameras) {}
}

void testCase2(framegraph::FrameGraph& framegraph, const ccstd::vector<scene::Camera*>& cameras) {
    boost::container::pmr::memory_resource *resource = boost::container::pmr::get_default_resource();
    RenderGraph renderGraph(resource);
    ResourceGraph rescGraph(resource);
    LayoutGraphData layoutGraph(resource);

    ResourceInfo resources = {
        {"0", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"1", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"2", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"3", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"4", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"5", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"6", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"7", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"8", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"9", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"10", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"11", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"12", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"13", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"14", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"15", gfx::DESCRIPTOR_TEXTURE_TYPE},
    };

    ViewInfo data = {
        {
            PassType::RASTER,
            {
                {{}, {"0", "1"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"0"}, {"2", "3"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"1"}, {"4", "5"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"3", "5"}, {"6"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"2", "4", "6"}, {"7"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{}, {"8"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"0", "8"}, {"9"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"7", "9"}, {"10"}},
            },
        },
        {
            PassType::PRESENT,
            {
                {{"10"}, {}},
            },
        },
    };

    LayoutInfo layoutInfo = {
        {
            {"0", 0, gfx::ShaderStageFlagBit::FRAGMENT},
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"0", 0, gfx::ShaderStageFlagBit::VERTEX},
            {"2", 2, gfx::ShaderStageFlagBit::FRAGMENT},
            {"3", 3, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"4", 4, gfx::ShaderStageFlagBit::FRAGMENT},
            {"5", 5, gfx::ShaderStageFlagBit::FRAGMENT},
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"3", 3, gfx::ShaderStageFlagBit::FRAGMENT},
            {"5", 5, gfx::ShaderStageFlagBit::FRAGMENT},
            {"6", 6, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"2", 2, gfx::ShaderStageFlagBit::FRAGMENT},
            {"4", 4, gfx::ShaderStageFlagBit::VERTEX},
            {"6", 6, gfx::ShaderStageFlagBit::FRAGMENT},
            {"7", 7, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"8", 8, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"0", 0, gfx::ShaderStageFlagBit::FRAGMENT},
            {"8", 8, gfx::ShaderStageFlagBit::FRAGMENT},
            {"9", 9, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"7", 7, gfx::ShaderStageFlagBit::FRAGMENT},
            {"9", 9, gfx::ShaderStageFlagBit::FRAGMENT},
            {"10", 10, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"10", 10, gfx::ShaderStageFlagBit::FRAGMENT},
        },
    };

    testData(data, resources, layoutInfo, renderGraph, rescGraph, layoutGraph, framegraph, cameras);
    // for(const auto* camera : cameras) {}
}

void testCase3(framegraph::FrameGraph& framegraph, const ccstd::vector<scene::Camera*>& cameras) {
    boost::container::pmr::memory_resource *resource = boost::container::pmr::get_default_resource();
    RenderGraph renderGraph(resource);
    ResourceGraph rescGraph(resource);
    LayoutGraphData layoutGraph(resource);

    ResourceInfo resources = {
        {"0", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"1", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"2", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"3", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"4", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"5", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"6", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"7", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"8", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"9", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"10", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"11", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"12", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"13", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"14", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"15", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"16", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"17", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"18", gfx::DESCRIPTOR_TEXTURE_TYPE},
    };

    ViewInfo data = {
        {
            PassType::RASTER,
            {
                {{}, {"0"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"0"}, {"1"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"1"}, {"2"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"2"}, {"3"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"3"}, {"4"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"4"}, {"5"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"5"}, {"6"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"3"}, {"7"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"7"}, {"8"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"1"}, {"9"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"1"}, {"14"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"14"}, {"15"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"15", "9"}, {"10"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"1"}, {"16"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"16"}, {"17"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"8", "10", "17"}, {"11"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"11", "6"}, {"12"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"12"}, {"13"}},
            },
        },
        {
            PassType::PRESENT,
            {
                {{"13"}, {}},
            },
        },
    };

    using ShaderStageMap = map<string, gfx::ShaderStageFlagBit>;

    LayoutInfo layoutInfo = {
        {
            {"0", 0, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"0", 0, gfx::ShaderStageFlagBit::FRAGMENT},
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
            {"2", 2, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"2", 2, gfx::ShaderStageFlagBit::FRAGMENT},
            {"3", 3, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"3", 3, gfx::ShaderStageFlagBit::FRAGMENT},
            {"4", 4, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"4", 4, gfx::ShaderStageFlagBit::FRAGMENT},
            {"5", 5, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"5", 5, gfx::ShaderStageFlagBit::FRAGMENT},
            {"6", 6, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"3", 3, gfx::ShaderStageFlagBit::FRAGMENT},
            {"7", 7, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"7", 7, gfx::ShaderStageFlagBit::FRAGMENT},
            {"8", 8, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
            {"9", 9, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
            {"14", 14, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"14", 14, gfx::ShaderStageFlagBit::FRAGMENT},
            {"15", 15, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"15", 15, gfx::ShaderStageFlagBit::FRAGMENT},
            {"9", 9, gfx::ShaderStageFlagBit::FRAGMENT},
            {"10", 10, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
            {"16", 16, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"16", 16, gfx::ShaderStageFlagBit::FRAGMENT},
            {"17", 17, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"8", 8, gfx::ShaderStageFlagBit::FRAGMENT},
            {"10", 10, gfx::ShaderStageFlagBit::FRAGMENT},
            {"17", 17, gfx::ShaderStageFlagBit::FRAGMENT},
            {"11", 11, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"6", 6, gfx::ShaderStageFlagBit::FRAGMENT},
            {"11", 11, gfx::ShaderStageFlagBit::FRAGMENT},
            {"12", 12, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"12", 12, gfx::ShaderStageFlagBit::FRAGMENT},
            {"13", 13, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"13", 13, gfx::ShaderStageFlagBit::FRAGMENT},
        },
    };

    testData(data, resources, layoutInfo, renderGraph, rescGraph, layoutGraph, framegraph, cameras);
    // for(const auto* camera : cameras) {}
}

void testCase4(framegraph::FrameGraph& framegraph, ccstd::vector<scene::Camera*>& cameras) {
    boost::container::pmr::memory_resource *resource = boost::container::pmr::get_default_resource();
    RenderGraph renderGraph(resource);
    ResourceGraph rescGraph(resource);
    LayoutGraphData layoutGraph(resource);

    ResourceInfo resources = {
        {"0", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"1", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"2", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"3", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"4", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"5", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"6", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"7", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"8", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"9", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"10", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"11", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"12", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"13", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"14", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"15", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"16", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"17", gfx::DESCRIPTOR_TEXTURE_TYPE},
        {"18", gfx::DESCRIPTOR_TEXTURE_TYPE},
    };

    ViewInfo data = {
        {
            PassType::RASTER,
            {
                {{}, {"0"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"0"}, {"1"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"1"}, {"2"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"2"}, {"3"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"1"}, {"4"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"4"}, {"5"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"1"}, {"6"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"6"}, {"7"}},
            },
        },
        {
            PassType::RASTER,
            {
                {{"7", "5", "3"}, {"8"}},
            },
        },
        {
            PassType::PRESENT,
            {
                {{"8"}, {}},
            },
        }};

    using ShaderStageMap = map<string, gfx::ShaderStageFlagBit>;

    LayoutInfo layoutInfo = {
        {
            {"0", 0, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"0", 0, gfx::ShaderStageFlagBit::FRAGMENT},
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
            {"2", 2, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"2", 2, gfx::ShaderStageFlagBit::FRAGMENT},
            {"3", 3, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
            {"4", 4, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"4", 4, gfx::ShaderStageFlagBit::FRAGMENT},
            {"5", 5, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"1", 1, gfx::ShaderStageFlagBit::FRAGMENT},
            {"6", 6, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"6", 6, gfx::ShaderStageFlagBit::FRAGMENT},
            {"7", 7, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"3", 3, gfx::ShaderStageFlagBit::FRAGMENT},
            {"5", 5, gfx::ShaderStageFlagBit::FRAGMENT},
            {"7", 7, gfx::ShaderStageFlagBit::FRAGMENT},
            {"8", 8, gfx::ShaderStageFlagBit::FRAGMENT},
        },
        {
            {"8", 8, gfx::ShaderStageFlagBit::FRAGMENT},
        },
    };

    testData(data, resources, layoutInfo, renderGraph, rescGraph, layoutGraph, framegraph, cameras);
    // for(const auto* camera : cameras) {}
}

    } // namespace render
} // namespace cc
