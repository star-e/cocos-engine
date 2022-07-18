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

/**
 * ========================= !DO NOT CHANGE THE FOLLOWING SECTION MANUALLY! =========================
 * The following section is auto-generated.
 * ========================= !DO NOT CHANGE THE FOLLOWING SECTION MANUALLY! =========================
 */
// clang-format off
#include "RenderGraphTypes.h"

namespace cc {

namespace render {

ResourceGraph::ResourceGraph(const allocator_type& alloc) noexcept
: vertices(alloc),
  names(alloc),
  descs(alloc),
  traits(alloc),
  states(alloc),
  resources(alloc),
  buffers(alloc),
  textures(alloc),
  framebuffers(alloc),
  swapchains(alloc),
  valueIndex(alloc) {}

ResourceGraph::ResourceGraph(ResourceGraph&& rhs, const allocator_type& alloc)
: vertices(std::move(rhs.vertices), alloc),
  names(std::move(rhs.names), alloc),
  descs(std::move(rhs.descs), alloc),
  traits(std::move(rhs.traits), alloc),
  states(std::move(rhs.states), alloc),
  resources(std::move(rhs.resources), alloc),
  buffers(std::move(rhs.buffers), alloc),
  textures(std::move(rhs.textures), alloc),
  framebuffers(std::move(rhs.framebuffers), alloc),
  swapchains(std::move(rhs.swapchains), alloc),
  valueIndex(std::move(rhs.valueIndex), alloc) {}

ResourceGraph::ResourceGraph(ResourceGraph const& rhs, const allocator_type& alloc)
: vertices(rhs.vertices, alloc),
  names(rhs.names, alloc),
  descs(rhs.descs, alloc),
  traits(rhs.traits, alloc),
  states(rhs.states, alloc),
  resources(rhs.resources, alloc),
  buffers(rhs.buffers, alloc),
  textures(rhs.textures, alloc),
  framebuffers(rhs.framebuffers, alloc),
  swapchains(rhs.swapchains, alloc),
  valueIndex(rhs.valueIndex, alloc) {}

// ContinuousContainer
void ResourceGraph::reserve(vertices_size_type sz) {
    vertices.reserve(sz);
    names.reserve(sz);
    descs.reserve(sz);
    traits.reserve(sz);
    states.reserve(sz);
}

ResourceGraph::Vertex::Vertex(const allocator_type& alloc) noexcept
: outEdges(alloc),
  inEdges(alloc) {}

ResourceGraph::Vertex::Vertex(Vertex&& rhs, const allocator_type& alloc)
: outEdges(std::move(rhs.outEdges), alloc),
  inEdges(std::move(rhs.inEdges), alloc),
  handle(std::move(rhs.handle)) {}

ResourceGraph::Vertex::Vertex(Vertex const& rhs, const allocator_type& alloc)
: outEdges(rhs.outEdges, alloc),
  inEdges(rhs.inEdges, alloc),
  handle(rhs.handle) {}

RasterView::RasterView(const allocator_type& alloc) noexcept
: slotName(alloc) {}

RasterView::RasterView(ccstd::pmr::string slotNameIn, AccessType accessTypeIn, AttachmentType attachmentTypeIn, gfx::LoadOp loadOpIn, gfx::StoreOp storeOpIn, gfx::ClearFlagBit clearFlagsIn, gfx::Color clearColorIn, const allocator_type& alloc) noexcept
: slotName(std::move(slotNameIn), alloc),
  accessType(accessTypeIn),
  attachmentType(attachmentTypeIn),
  loadOp(loadOpIn),
  storeOp(storeOpIn),
  clearFlags(clearFlagsIn),
  clearColor(clearColorIn) {}

RasterView::RasterView(RasterView&& rhs, const allocator_type& alloc)
: slotName(std::move(rhs.slotName), alloc),
  accessType(rhs.accessType),
  attachmentType(rhs.attachmentType),
  loadOp(rhs.loadOp),
  storeOp(rhs.storeOp),
  clearFlags(rhs.clearFlags),
  clearColor(rhs.clearColor) {}

RasterView::RasterView(RasterView const& rhs, const allocator_type& alloc)
: slotName(rhs.slotName, alloc),
  accessType(rhs.accessType),
  attachmentType(rhs.attachmentType),
  loadOp(rhs.loadOp),
  storeOp(rhs.storeOp),
  clearFlags(rhs.clearFlags),
  clearColor(rhs.clearColor) {}

ComputeView::ComputeView(const allocator_type& alloc) noexcept
: name(alloc) {}

ComputeView::ComputeView(ComputeView&& rhs, const allocator_type& alloc)
: name(std::move(rhs.name), alloc),
  accessType(rhs.accessType),
  clearFlags(rhs.clearFlags),
  clearColor(rhs.clearColor),
  clearValueType(rhs.clearValueType) {}

ComputeView::ComputeView(ComputeView const& rhs, const allocator_type& alloc)
: name(rhs.name, alloc),
  accessType(rhs.accessType),
  clearFlags(rhs.clearFlags),
  clearColor(rhs.clearColor),
  clearValueType(rhs.clearValueType) {}

RasterSubpass::RasterSubpass(const allocator_type& alloc) noexcept
: rasterViews(alloc),
  computeViews(alloc) {}

RasterSubpass::RasterSubpass(RasterSubpass&& rhs, const allocator_type& alloc)
: rasterViews(std::move(rhs.rasterViews), alloc),
  computeViews(std::move(rhs.computeViews), alloc) {}

RasterSubpass::RasterSubpass(RasterSubpass const& rhs, const allocator_type& alloc)
: rasterViews(rhs.rasterViews, alloc),
  computeViews(rhs.computeViews, alloc) {}

SubpassGraph::SubpassGraph(const allocator_type& alloc) noexcept
: vertices(alloc),
  names(alloc),
  subpasses(alloc) {}

SubpassGraph::SubpassGraph(SubpassGraph&& rhs, const allocator_type& alloc)
: vertices(std::move(rhs.vertices), alloc),
  names(std::move(rhs.names), alloc),
  subpasses(std::move(rhs.subpasses), alloc) {}

SubpassGraph::SubpassGraph(SubpassGraph const& rhs, const allocator_type& alloc)
: vertices(rhs.vertices, alloc),
  names(rhs.names, alloc),
  subpasses(rhs.subpasses, alloc) {}

// ContinuousContainer
void SubpassGraph::reserve(vertices_size_type sz) {
    vertices.reserve(sz);
    names.reserve(sz);
    subpasses.reserve(sz);
}

SubpassGraph::Vertex::Vertex(const allocator_type& alloc) noexcept
: outEdges(alloc),
  inEdges(alloc) {}

SubpassGraph::Vertex::Vertex(Vertex&& rhs, const allocator_type& alloc)
: outEdges(std::move(rhs.outEdges), alloc),
  inEdges(std::move(rhs.inEdges), alloc) {}

SubpassGraph::Vertex::Vertex(Vertex const& rhs, const allocator_type& alloc)
: outEdges(rhs.outEdges, alloc),
  inEdges(rhs.inEdges, alloc) {}

RasterPass::RasterPass(const allocator_type& alloc) noexcept
: rasterViews(alloc),
  computeViews(alloc),
  subpassGraph(alloc) {}

RasterPass::RasterPass(RasterPass&& rhs, const allocator_type& alloc)
: isValid(rhs.isValid),
  rasterViews(std::move(rhs.rasterViews), alloc),
  computeViews(std::move(rhs.computeViews), alloc),
  subpassGraph(std::move(rhs.subpassGraph), alloc),
  width(rhs.width),
  height(rhs.height) {}

RasterPass::RasterPass(RasterPass const& rhs, const allocator_type& alloc)
: isValid(rhs.isValid),
  rasterViews(rhs.rasterViews, alloc),
  computeViews(rhs.computeViews, alloc),
  subpassGraph(rhs.subpassGraph, alloc),
  width(rhs.width),
  height(rhs.height) {}

ComputePass::ComputePass(const allocator_type& alloc) noexcept
: computeViews(alloc) {}

ComputePass::ComputePass(ComputePass&& rhs, const allocator_type& alloc)
: computeViews(std::move(rhs.computeViews), alloc) {}

ComputePass::ComputePass(ComputePass const& rhs, const allocator_type& alloc)
: computeViews(rhs.computeViews, alloc) {}

CopyPair::CopyPair(const allocator_type& alloc) noexcept
: source(alloc),
  target(alloc) {}

CopyPair::CopyPair(ccstd::pmr::string sourceIn, ccstd::pmr::string targetIn, uint32_t mipLevelsIn, uint32_t numSlicesIn, uint32_t sourceMostDetailedMipIn, uint32_t sourceFirstSliceIn, uint32_t sourcePlaneSliceIn, uint32_t targetMostDetailedMipIn, uint32_t targetFirstSliceIn, uint32_t targetPlaneSliceIn, const allocator_type& alloc) noexcept // NOLINT
: source(std::move(sourceIn), alloc),
  target(std::move(targetIn), alloc),
  mipLevels(mipLevelsIn),
  numSlices(numSlicesIn),
  sourceMostDetailedMip(sourceMostDetailedMipIn),
  sourceFirstSlice(sourceFirstSliceIn),
  sourcePlaneSlice(sourcePlaneSliceIn),
  targetMostDetailedMip(targetMostDetailedMipIn),
  targetFirstSlice(targetFirstSliceIn),
  targetPlaneSlice(targetPlaneSliceIn) {}

CopyPair::CopyPair(CopyPair&& rhs, const allocator_type& alloc)
: source(std::move(rhs.source), alloc),
  target(std::move(rhs.target), alloc),
  mipLevels(rhs.mipLevels),
  numSlices(rhs.numSlices),
  sourceMostDetailedMip(rhs.sourceMostDetailedMip),
  sourceFirstSlice(rhs.sourceFirstSlice),
  sourcePlaneSlice(rhs.sourcePlaneSlice),
  targetMostDetailedMip(rhs.targetMostDetailedMip),
  targetFirstSlice(rhs.targetFirstSlice),
  targetPlaneSlice(rhs.targetPlaneSlice) {}

CopyPair::CopyPair(CopyPair const& rhs, const allocator_type& alloc)
: source(rhs.source, alloc),
  target(rhs.target, alloc),
  mipLevels(rhs.mipLevels),
  numSlices(rhs.numSlices),
  sourceMostDetailedMip(rhs.sourceMostDetailedMip),
  sourceFirstSlice(rhs.sourceFirstSlice),
  sourcePlaneSlice(rhs.sourcePlaneSlice),
  targetMostDetailedMip(rhs.targetMostDetailedMip),
  targetFirstSlice(rhs.targetFirstSlice),
  targetPlaneSlice(rhs.targetPlaneSlice) {}

CopyPass::CopyPass(const allocator_type& alloc) noexcept
: copyPairs(alloc) {}

CopyPass::CopyPass(CopyPass&& rhs, const allocator_type& alloc)
: copyPairs(std::move(rhs.copyPairs), alloc) {}

CopyPass::CopyPass(CopyPass const& rhs, const allocator_type& alloc)
: copyPairs(rhs.copyPairs, alloc) {}

MovePair::MovePair(const allocator_type& alloc) noexcept
: source(alloc),
  target(alloc) {}

MovePair::MovePair(ccstd::pmr::string sourceIn, ccstd::pmr::string targetIn, uint32_t mipLevelsIn, uint32_t numSlicesIn, uint32_t targetMostDetailedMipIn, uint32_t targetFirstSliceIn, uint32_t targetPlaneSliceIn, const allocator_type& alloc) noexcept // NOLINT
: source(std::move(sourceIn), alloc),
  target(std::move(targetIn), alloc),
  mipLevels(mipLevelsIn),
  numSlices(numSlicesIn),
  targetMostDetailedMip(targetMostDetailedMipIn),
  targetFirstSlice(targetFirstSliceIn),
  targetPlaneSlice(targetPlaneSliceIn) {}

MovePair::MovePair(MovePair&& rhs, const allocator_type& alloc)
: source(std::move(rhs.source), alloc),
  target(std::move(rhs.target), alloc),
  mipLevels(rhs.mipLevels),
  numSlices(rhs.numSlices),
  targetMostDetailedMip(rhs.targetMostDetailedMip),
  targetFirstSlice(rhs.targetFirstSlice),
  targetPlaneSlice(rhs.targetPlaneSlice) {}

MovePair::MovePair(MovePair const& rhs, const allocator_type& alloc)
: source(rhs.source, alloc),
  target(rhs.target, alloc),
  mipLevels(rhs.mipLevels),
  numSlices(rhs.numSlices),
  targetMostDetailedMip(rhs.targetMostDetailedMip),
  targetFirstSlice(rhs.targetFirstSlice),
  targetPlaneSlice(rhs.targetPlaneSlice) {}

MovePass::MovePass(const allocator_type& alloc) noexcept
: movePairs(alloc) {}

MovePass::MovePass(MovePass&& rhs, const allocator_type& alloc)
: movePairs(std::move(rhs.movePairs), alloc) {}

MovePass::MovePass(MovePass const& rhs, const allocator_type& alloc)
: movePairs(rhs.movePairs, alloc) {}

RaytracePass::RaytracePass(const allocator_type& alloc) noexcept
: computeViews(alloc) {}

RaytracePass::RaytracePass(RaytracePass&& rhs, const allocator_type& alloc)
: computeViews(std::move(rhs.computeViews), alloc) {}

RaytracePass::RaytracePass(RaytracePass const& rhs, const allocator_type& alloc)
: computeViews(rhs.computeViews, alloc) {}

SceneData::SceneData(const allocator_type& alloc) noexcept
: name(alloc),
  scenes(alloc) {}

SceneData::SceneData(ccstd::pmr::string nameIn, SceneFlags flagsIn, LightInfo lightIn, const allocator_type& alloc) noexcept
: name(std::move(nameIn), alloc),
  light(std::move(lightIn)),
  flags(flagsIn),
  scenes(alloc) {}

SceneData::SceneData(SceneData&& rhs, const allocator_type& alloc)
: name(std::move(rhs.name), alloc),
  camera(rhs.camera),
  light(std::move(rhs.light)),
  flags(rhs.flags),
  scenes(std::move(rhs.scenes), alloc) {}

SceneData::SceneData(SceneData const& rhs, const allocator_type& alloc)
: name(rhs.name, alloc),
  camera(rhs.camera),
  light(rhs.light),
  flags(rhs.flags),
  scenes(rhs.scenes, alloc) {}

Dispatch::Dispatch(const allocator_type& alloc) noexcept
: shader(alloc) {}

Dispatch::Dispatch(ccstd::pmr::string shaderIn, uint32_t threadGroupCountXIn, uint32_t threadGroupCountYIn, uint32_t threadGroupCountZIn, const allocator_type& alloc) noexcept // NOLINT
: shader(std::move(shaderIn), alloc),
  threadGroupCountX(threadGroupCountXIn),
  threadGroupCountY(threadGroupCountYIn),
  threadGroupCountZ(threadGroupCountZIn) {}

Dispatch::Dispatch(Dispatch&& rhs, const allocator_type& alloc)
: shader(std::move(rhs.shader), alloc),
  threadGroupCountX(rhs.threadGroupCountX),
  threadGroupCountY(rhs.threadGroupCountY),
  threadGroupCountZ(rhs.threadGroupCountZ) {}

Dispatch::Dispatch(Dispatch const& rhs, const allocator_type& alloc)
: shader(rhs.shader, alloc),
  threadGroupCountX(rhs.threadGroupCountX),
  threadGroupCountY(rhs.threadGroupCountY),
  threadGroupCountZ(rhs.threadGroupCountZ) {}

PresentPass::PresentPass(const allocator_type& alloc) noexcept
: presents(alloc) {}

PresentPass::PresentPass(PresentPass&& rhs, const allocator_type& alloc)
: presents(std::move(rhs.presents), alloc) {}

PresentPass::PresentPass(PresentPass const& rhs, const allocator_type& alloc)
: presents(rhs.presents, alloc) {}

RenderData::RenderData(const allocator_type& alloc) noexcept
: constants(alloc),
  buffers(alloc),
  textures(alloc),
  samplers(alloc) {}

RenderData::RenderData(RenderData&& rhs, const allocator_type& alloc)
: constants(std::move(rhs.constants), alloc),
  buffers(std::move(rhs.buffers), alloc),
  textures(std::move(rhs.textures), alloc),
  samplers(std::move(rhs.samplers), alloc) {}

RenderGraph::RenderGraph(const allocator_type& alloc) noexcept
: objects(alloc),
  vertices(alloc),
  names(alloc),
  layoutNodes(alloc),
  data(alloc),
  valid(alloc),
  rasterPasses(alloc),
  computePasses(alloc),
  copyPasses(alloc),
  movePasses(alloc),
  presentPasses(alloc),
  raytracePasses(alloc),
  renderQueues(alloc),
  scenes(alloc),
  blits(alloc),
  dispatches(alloc),
  index(alloc) {}

RenderGraph::RenderGraph(RenderGraph&& rhs, const allocator_type& alloc)
: objects(std::move(rhs.objects), alloc),
  vertices(std::move(rhs.vertices), alloc),
  names(std::move(rhs.names), alloc),
  layoutNodes(std::move(rhs.layoutNodes), alloc),
  data(std::move(rhs.data), alloc),
  valid(std::move(rhs.valid), alloc),
  rasterPasses(std::move(rhs.rasterPasses), alloc),
  computePasses(std::move(rhs.computePasses), alloc),
  copyPasses(std::move(rhs.copyPasses), alloc),
  movePasses(std::move(rhs.movePasses), alloc),
  presentPasses(std::move(rhs.presentPasses), alloc),
  raytracePasses(std::move(rhs.raytracePasses), alloc),
  renderQueues(std::move(rhs.renderQueues), alloc),
  scenes(std::move(rhs.scenes), alloc),
  blits(std::move(rhs.blits), alloc),
  dispatches(std::move(rhs.dispatches), alloc),
  index(std::move(rhs.index), alloc) {}

// ContinuousContainer
void RenderGraph::reserve(vertices_size_type sz) {
    objects.reserve(sz);
    vertices.reserve(sz);
    names.reserve(sz);
    layoutNodes.reserve(sz);
    data.reserve(sz);
    valid.reserve(sz);
}

RenderGraph::Object::Object(const allocator_type& alloc) noexcept
: children(alloc),
  parents(alloc) {}

RenderGraph::Object::Object(Object&& rhs, const allocator_type& alloc)
: children(std::move(rhs.children), alloc),
  parents(std::move(rhs.parents), alloc) {}

RenderGraph::Object::Object(Object const& rhs, const allocator_type& alloc)
: children(rhs.children, alloc),
  parents(rhs.parents, alloc) {}

RenderGraph::Vertex::Vertex(const allocator_type& alloc) noexcept
: outEdges(alloc),
  inEdges(alloc) {}

RenderGraph::Vertex::Vertex(Vertex&& rhs, const allocator_type& alloc)
: outEdges(std::move(rhs.outEdges), alloc),
  inEdges(std::move(rhs.inEdges), alloc),
  handle(std::move(rhs.handle)) {}

RenderGraph::Vertex::Vertex(Vertex const& rhs, const allocator_type& alloc)
: outEdges(rhs.outEdges, alloc),
  inEdges(rhs.inEdges, alloc),
  handle(rhs.handle) {}

} // namespace render

} // namespace cc

// clang-format on
