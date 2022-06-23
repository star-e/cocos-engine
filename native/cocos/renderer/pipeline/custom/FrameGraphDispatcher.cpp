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
#ifdef __clang__
    #pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif
#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/transitive_closure.hpp>
#include <boost/range/algorithm.hpp>
#include <iterator>
#include <limits>
#include <vector>
#include "FGDispatcherGraphs.h"
#include "FGDispatcherTypes.h"
#include "LayoutGraphGraphs.h"
#include "LayoutGraphTypes.h"
#include "Range.h"
#include "RenderGraphGraphs.h"
#include "base/Log.h"
#include "boost/graph/depth_first_search.hpp"
#include "boost/graph/hawick_circuits.hpp"
#include "boost/graph/visitors.hpp"
#include "boost/lexical_cast.hpp"
#include "gfx-base/GFXDef-common.h"
#include "gfx-base/GFXDef.h"
#include "math/Vec2.h"
#include "pipeline/custom/GslUtils.h"
#include "pipeline/custom/RenderCommonFwd.h"
#include "pipeline/custom/RenderGraphTypes.h"

namespace cc {

namespace render {

void passReorder(FrameGraphDispatcher &fgDispatcher);
void memoryAliasing(FrameGraphDispatcher &fgDispatcher);
void buildBarriers(FrameGraphDispatcher &fgDispatcher);

void FrameGraphDispatcher::run() {
    if (_enablePassReorder) {
        passReorder(*this);
    }
    if (_enableMemoryAliasing) {
        memoryAliasing(*this);
    }
    buildBarriers(*this);
}

void FrameGraphDispatcher::enablePassReorder(bool enable) {
    _enablePassReorder = enable;
}

void FrameGraphDispatcher::enableMemoryAliasing(bool enable) {
    _enableMemoryAliasing = enable;
}

void FrameGraphDispatcher::setParalellWeight(float paralellExecWeight) {
    _paralellExecWeight = clampf(paralellExecWeight, 0.0F, 1.0F);
}

/////////////////////////////////////////////////////////////////////////////////////INTERNAL⚡IMPLEMENTATION/////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------predefine------------------------------------------------------------------
using PmrString = ccstd::pmr::string;
using RAG = ResourceAccessGraph;
using LGD = LayoutGraphData;
using gfx::PassType;
using BarrierMap = FrameGraphDispatcher::BarrierMap;
using AccessVertex = ResourceAccessGraph::vertex_descriptor;
using InputStatusTuple = std::tuple<PassType /*passType*/, PmrString /*resourceName*/, gfx::ShaderStageFlagBit /*visibility*/, gfx::MemoryAccessBit /*access*/>;
using ResourceHandle = ResourceGraph::vertex_descriptor;
using ResourceNames = PmrFlatSet<PmrString>;
using EdgeList = std::pair<RAG::edge_descriptor, RAG::edge_descriptor>;
using CloseCircuit = std::pair<EdgeList, EdgeList>;
using CloseCircuits = std::vector<CloseCircuit>;
using EmptyVert = EmptyGraph::vertex_descriptor;
using EmptyVerts = std::vector<EmptyVert>;
using EmptyEdge = EmptyGraph::edge_descriptor;
using EmptyEdges = std::vector<EmptyEdge>;
using ScoreMap = std::map<EmptyVert, std::pair<int64_t /*backward*/, int64_t /*forward*/>>;

auto defaultAccess = gfx::MemoryAccessBit::NONE;
auto defaultVisibility = gfx::ShaderStageFlagBit::NONE;

constexpr uint32_t EXPECT_START_ID = 0;
constexpr uint32_t INVALID_ID = 0xFFFFFFFF;

using AccessTable = PmrFlatMap<ResourceHandle /*resourceID*/, ResourceTransition /*last-curr-status*/>;
using ExternalResMap = PmrFlatMap<PmrString /*resourceName*/, ResourceTransition /*last-curr-status*/>;

// for scoped enum only
template <typename From, typename To>
class GfxTypeConverter {
public:
    To operator()(From from) {
        return static_cast<To>(
            static_cast<std::underlying_type_t<From>>(from));
    }
};

// static auto toGfxAccess = GfxTypeConverter<AccessType, gfx::MemoryAccessBit>();
gfx::MemoryAccessBit toGfxAccess(AccessType type) {
    switch (type) {
        case AccessType::READ:
            return gfx::MemoryAccessBit::READ_ONLY;
        case AccessType::WRITE:
            return gfx::MemoryAccessBit::WRITE_ONLY;
        case AccessType::READ_WRITE:
            return gfx::MemoryAccessBit::READ_WRITE;
        default:
            return gfx::MemoryAccessBit::NONE;
    }
};

// AccessStatus.vertID : in resourceNode it's resource ID; in barrierNode it's pass ID.
AccessVertex dependencyCheck(RAG &rag, AccessTable &accessRecord, AccessVertex curVertID, const InputStatusTuple &status);
gfx::ShaderStageFlagBit getVisibilityByDescName(const LGD &lgd, uint32_t passID, const PmrString &slotName);

void addAccessNode(RAG &rag, const ResourceGraph &rg, ResourceAccessNode &node, InputStatusTuple status);
void addCopyAccessNode(RAG &rag, const ResourceGraph &rg, ResourceAccessNode &node, InputStatusTuple status, const Range &range);
void processRasterPass(RAG &rag, EmptyGraph &relationGraph, const LGD &lgd, const ResourceGraph &rescGraph, AccessTable &accessRecord, uint32_t passID, const RasterPass &pass);
void processComputePass(RAG &rag, EmptyGraph &relationGraph, const LGD &lgd, const ResourceGraph &rescGraph, AccessTable &accessRecord, uint32_t passID, const ComputePass &pass);
void processCopyPass(RAG &rag, EmptyGraph &relationGraph, const LGD & /*lgd*/, const ResourceGraph &rescGraph, AccessTable &accessRecord, uint32_t passID, const CopyPass &pass);
void processRaytracePass(RAG &rag, EmptyGraph &relationGraph, const LGD &lgd, const ResourceGraph &rescGraph, AccessTable &accessRecord, uint32_t passID, const RaytracePass &pass);
void processPresentPass(RAG &rag, EmptyGraph &relationGraph, const LGD &lgd, const ResourceGraph &rescGraph, AccessTable &accessRecord, uint32_t passID, const PresentPass &pass);

// execution order BUT NOT LOGICALLY
bool isPassExecAdjecent(uint32_t passL, uint32_t passR) { return std::abs(int(passL - passR)) == 1; }
bool isStatusDependent(const AccessStatus &lhs, const AccessStatus &rhs);
template <typename Graph>
bool tryAddEdge(uint32_t srcVertex, uint32_t dstVertex, Graph &graph);

inline EmptyGraph::vertex_descriptor add_vertex(EmptyGraph &g) { // NOLINT
    return addVertex(g);
}

// status of resource access
void buildAccessGraph(const RenderGraph &renderGraph, const LayoutGraphData &lgd, const ResourceGraph &rescGragh, ResourceAccessGraph &rag, EmptyGraph &relationGraph) {
    // what we need:
    //  - pass dependency
    //  - pass attachment access
    AccessTable accessRecord;

    size_t numPasses = 1;
    numPasses += renderGraph.rasterPasses.size();
    numPasses += renderGraph.computePasses.size();
    numPasses += renderGraph.copyPasses.size();
    numPasses += renderGraph.movePasses.size();
    numPasses += renderGraph.raytracePasses.size();

    rag.reserve(static_cast<ResourceAccessGraph::vertices_size_type>(numPasses));
    rag.resourceNames.reserve(128);
    rag.resourceIndex.reserve(128);

    auto startID = add_vertex(rag, INVALID_ID);
    CC_EXPECTS(startID == EXPECT_START_ID);

    add_vertex(relationGraph);

    for (const auto passID : makeRange(vertices(renderGraph))) {
        visitObject(
            passID, renderGraph,
            [&](const RasterPass &pass) {
                processRasterPass(rag, relationGraph, lgd, rescGragh, accessRecord, passID, pass);
            },
            [&](const ComputePass &pass) {
                processComputePass(rag, relationGraph, lgd, rescGragh, accessRecord, passID, pass);
            },
            [&](const CopyPass &pass) {
                processCopyPass(rag, relationGraph, lgd, rescGragh, accessRecord, passID, pass);
            },
            [&](const RaytracePass &pass) {
                processRaytracePass(rag, relationGraph, lgd, rescGragh, accessRecord, passID, pass);
            },
            [&](const PresentPass &pass) {
                processPresentPass(rag, relationGraph, lgd, rescGragh, accessRecord, passID, pass);
            },
            [&](const auto & /*pass*/) {
                // do nothing
            });
    }

    // make leaf node closed walk for pass reorder
    for (auto pass : rag.externalPasses) {
        if (out_degree(pass, rag) == 0) {
            add_edge(pass, rag.presentPassID, rag);
        }
    }
}

#pragma region BUILD_BARRIERS
struct BarrierVisitor : public boost::bfs_visitor<> {
    using Vertex = ResourceAccessGraph::vertex_descriptor;
    using Edge = ResourceAccessGraph::edge_descriptor;
    using Graph = ResourceAccessGraph;

    explicit BarrierVisitor(const ResourceGraph &rg, BarrierMap &barriers, ExternalResMap &extMap, ResourceNames &externalNames)
    : barrierMap(barriers), resourceGraph(rg), externalMap(extMap), externalResNames(externalNames) {
    }

    void discover_vertex(Vertex u, const Graph &g) {
    }

    void batchBarriers(Edge e, const Graph &g) {
        // what we get:
        // - beginAccess: not NONE, endAccess: not NONE ===> full barrier
        // - beginAccess: not NONE, endAccess: NONE     ===> split begin barrier
        // - beginAccess: NONE, endAccess: not NONE     ===> split end barrier

        Vertex from = source(e, g);
        Vertex to = target(e, g);

        bool isAdjacent = isPassExecAdjecent(from, to);

        const ResourceAccessNode &fromAccess = get(ResourceAccessGraph::AccessNode, g, from);
        const ResourceAccessNode &toAccess = get(ResourceAccessGraph::AccessNode, g, to);

        std::vector<AccessStatus> commonResources;
        const std::vector<AccessStatus> &fromStatus = fromAccess.attachemntStatus;
        const std::vector<AccessStatus> &toStatus = toAccess.attachemntStatus;

        std::set_intersection(fromStatus.begin(), fromStatus.end(),
                              toStatus.begin(), toStatus.end(),
                              std::back_inserter(commonResources),
                              [](const AccessStatus &lhs, const AccessStatus &rhs) {
                                  return lhs.vertID < rhs.vertID;
                              });

        // add an empty barrier anyway
        if (barrierMap.find(from) == barrierMap.end()) {
            barrierMap.emplace(from, BarrierNode{});
        }
        if (barrierMap.find(to) == barrierMap.end()) {
            barrierMap.emplace(to, BarrierNode{});
        }

        if (commonResources.empty()) {
            // this edge is a logic edge added during pass reorder,
            // no real dependency between this two vertices.
            return;
        }

        std::vector<Barrier> &srcRearBarriers = barrierMap[from].rearBarriers;
        std::vector<Barrier> &dstFrontBarriers = barrierMap[to].frontBarriers;

        // NOLINTNEXTLINE
        for (uint32_t i = 0; i < commonResources.size(); ++i) {
            uint32_t resourceID = commonResources[i].vertID;
            auto findAccessByID = [resourceID](const AccessStatus &resAccess) { return resAccess.vertID == resourceID; };
            auto fromIter = std::find_if(fromStatus.begin(), fromStatus.end(), findAccessByID);
            auto toIter = std::find_if(toStatus.begin(), toStatus.end(), findAccessByID);

            // can't happen
            CC_ASSERT(fromIter != fromStatus.end());
            CC_ASSERT(toIter != toStatus.end());

            if (!isStatusDependent(*fromIter, *toIter)) {
                continue;
            }

            auto findBarrierNodeByResID = [resourceID](const Barrier &barrier) { return resourceID == barrier.resourceID; };

            auto srcBarrierIter = std::find_if(srcRearBarriers.begin(), srcRearBarriers.end(), findBarrierNodeByResID);
            auto dstBarrierIter = std::find_if(dstFrontBarriers.begin(), dstFrontBarriers.end(), findBarrierNodeByResID);

            if (srcBarrierIter == srcRearBarriers.end()) {
                srcRearBarriers.emplace_back(Barrier{
                    resourceID,
                    isAdjacent ? gfx::BarrierType::FULL : gfx::BarrierType::SPLIT_BEGIN,
                    {
                        from,
                        (*fromIter).visibility,
                        (*fromIter).access,
                        (*fromIter).passType,
                        (*fromIter).range,
                    },
                    {
                        to,
                        (*toIter).visibility,
                        (*toIter).access,
                        (*toIter).passType,
                        (*toIter).range,
                    },
                });
                srcBarrierIter = std::prev(srcRearBarriers.end());
            } else {
                if (isAdjacent) {
                    (*srcBarrierIter).type = gfx::BarrierType::FULL;

                    (*srcBarrierIter).beginStatus.vertID = from;
                    (*srcBarrierIter).beginStatus.visibility = (*fromIter).visibility;
                    (*srcBarrierIter).beginStatus.access = (*fromIter).access;
                    (*srcBarrierIter).beginStatus.passType = (*fromIter).passType;
                    (*srcBarrierIter).beginStatus.range = (*fromIter).range;

                    (*srcBarrierIter).endStatus.vertID = to;
                    (*srcBarrierIter).endStatus.visibility = (*toIter).visibility;
                    (*srcBarrierIter).endStatus.access = (*toIter).access;
                    (*srcBarrierIter).endStatus.passType = (*toIter).passType;
                    (*srcBarrierIter).endStatus.range = (*toIter).range;
                } else {
                    if (from > (*srcBarrierIter).beginStatus.vertID) {
                        uint32_t siblingPass = (*srcBarrierIter).beginStatus.vertID;
                        (*srcBarrierIter).beginStatus.vertID = from;
                        (*srcBarrierIter).beginStatus.visibility = (*fromIter).visibility;
                        (*srcBarrierIter).beginStatus.access = (*fromIter).access;
                        (*srcBarrierIter).beginStatus.passType = (*fromIter).passType;
                        (*srcBarrierIter).beginStatus.range = (*fromIter).range;

                        auto siblingIter = std::find_if(barrierMap[siblingPass].rearBarriers.begin(), barrierMap[siblingPass].rearBarriers.end(),
                                                        [resourceID](const Barrier &barrier) {
                                                            return resourceID == barrier.resourceID;
                                                        });
                        CC_ASSERT(siblingIter != barrierMap[siblingPass].rearBarriers.end());
                        barrierMap[siblingPass].rearBarriers.erase(siblingIter);
                    }
                }
            }

            if ((*srcBarrierIter).type == gfx::BarrierType::SPLIT_BEGIN) {
                if (dstBarrierIter == dstFrontBarriers.end()) {
                    // if isAdjacent, full barrier already in src rear barriers.
                    if (!isAdjacent) {
                        dstFrontBarriers.emplace_back(Barrier{
                            resourceID,
                            gfx::BarrierType::SPLIT_END,
                            {
                                from,
                                (*fromIter).visibility,
                                (*fromIter).access,
                                (*fromIter).passType,
                                (*fromIter).range,
                            },
                            {
                                to, // next use of resc varies between resources
                                (*toIter).visibility,
                                (*toIter).access,
                                (*toIter).passType,
                                (*toIter).range,
                            },
                        });
                    }
                } else {
                    if (isAdjacent) {
                        // adjacent, barrier should be commit at fromPass, and remove this iter from dstBarriers
                        (*srcBarrierIter).type = gfx::BarrierType::FULL;

                        (*srcBarrierIter).beginStatus.vertID = from;
                        (*srcBarrierIter).beginStatus.visibility = (*fromIter).visibility;
                        (*srcBarrierIter).beginStatus.access = (*fromIter).access;
                        (*srcBarrierIter).beginStatus.passType = (*fromIter).passType;
                        (*srcBarrierIter).beginStatus.range = (*fromIter).range;

                        (*srcBarrierIter).endStatus.vertID = to;
                        (*srcBarrierIter).endStatus.visibility = (*toIter).visibility;
                        (*srcBarrierIter).endStatus.access = (*toIter).access;
                        (*srcBarrierIter).endStatus.passType = (*toIter).passType;
                        (*srcBarrierIter).endStatus.range = (*toIter).range;

                        dstFrontBarriers.erase(dstBarrierIter);
                    } else {
                        // logic but not exec adjacent
                        // and more adjacent(further from src) than another pass which hold a use of resourceID
                        // replace previous one

                        // 1 --> 2 --> 3
                        //             ↓
                        // 4 --> 5 --> 6

                        // [if] real pass order: 1 - 2 - 4 - 5 - 3 - 6

                        // 2 and 5 read from ResA, 6 writes to ResA
                        // 5 and 6 logically adjacent but not adjacent in execution order.
                        // barrier for ResA between 2 - 6 can be totally replace by 5 - 6
                        if (to < (*dstBarrierIter).endStatus.vertID) {
                            uint32_t siblingPass = (*dstBarrierIter).endStatus.vertID;
                            (*dstBarrierIter).endStatus.vertID = to;
                            (*dstBarrierIter).endStatus.visibility = (*toIter).visibility;
                            (*dstBarrierIter).endStatus.access = (*toIter).access;
                            (*dstBarrierIter).endStatus.passType = (*toIter).passType;
                            (*dstBarrierIter).endStatus.range = (*toIter).range;

                            // remove the further redundant barrier
                            auto siblingIter = std::find_if(barrierMap[siblingPass].frontBarriers.begin(), barrierMap[siblingPass].frontBarriers.end(),
                                                            [resourceID](const Barrier &barrier) {
                                                                return resourceID == barrier.resourceID;
                                                            });
                            CC_ASSERT(siblingIter != barrierMap[siblingPass].frontBarriers.end());
                            barrierMap[siblingPass].frontBarriers.erase(siblingIter);
                        }
                    }
                }
            }
        }

        //----------------------------------------------check external----------------------------------------------

        auto barrierExternalRes = [&](const AccessStatus &rescAccess, bool isFrom) {
            uint32_t rescID = rescAccess.vertID;
            bool externalRes = get(get(ResourceGraph::Traits, resourceGraph), rescID).hasSideEffects();
            Vertex vert = isFrom ? from : to;
            if (externalRes) {
                const PmrString &resName = get(ResourceGraph::Name, resourceGraph, rescID);
                auto iter = externalMap.find(resName);
                // first meet in this frame
                if (externalResNames.find(resName) == externalResNames.end()) {
                    // first meet in whole program
                    if (iter == externalMap.end()) {
                        externalMap.insert({resName,
                                            ResourceTransition{
                                                {
                                                    EXPECT_START_ID,
                                                    rescAccess.visibility,
                                                    rescAccess.access,
                                                    rescAccess.passType,
                                                    rescAccess.range,
                                                },
                                                {
                                                    vert,
                                                    rescAccess.visibility,
                                                    rescAccess.access,
                                                    rescAccess.passType,
                                                    rescAccess.range,
                                                },
                                            }});
                    } else {
                        externalMap[resName].lastStatus = externalMap[resName].currStatus;
                        externalMap[resName].currStatus = {
                            vert,
                            rescAccess.visibility,
                            rescAccess.access,
                            rescAccess.passType,
                            rescAccess.range,
                        };

                        if (isStatusDependent(externalMap[resName].lastStatus, externalMap[resName].currStatus)) {
                            barrierMap[vert].frontBarriers.emplace_back(Barrier{
                                rescID,
                                gfx::BarrierType::SPLIT_END,
                                externalMap[resName].lastStatus,
                                externalMap[resName].currStatus,
                            });
                        }
                    }
                    externalResNames.insert(resName);
                } else {
                    if ((*iter).second.currStatus.vertID < vert) {
                        //[pass: vert] is later access than in iter.
                        externalMap[resName].currStatus = {
                            INVALID_ID,
                            rescAccess.visibility,
                            rescAccess.access,
                            rescAccess.passType,
                            rescAccess.range,
                        };
                    }
                }
            }
        };

        for (const AccessStatus &rescAccess : fromAccess.attachemntStatus) {
            barrierExternalRes(rescAccess, true);
        }

        for (const AccessStatus &rescAccess : toAccess.attachemntStatus) {
            barrierExternalRes(rescAccess, false);
        }
        //---------------------------------------------------------------------------------------------------------
    }

    void examine_edge(Edge e, const Graph &g) {
        batchBarriers(e, g);
    }

    BarrierMap &barrierMap;
    const ResourceGraph &resourceGraph;
    ExternalResMap &externalMap;     // last frame to curr frame status transition
    ResourceNames &externalResNames; // first meet in this frame
};

void buildBarriers(FrameGraphDispatcher &fgDispatcher) {
    {
        auto *scratch = fgDispatcher.scratch;
        const auto &graph = fgDispatcher.graph;
        const auto &layoutGraph = fgDispatcher.layoutGraph;
        const auto &resourceGraph = fgDispatcher.resourceGraph;
        auto &relationGraph = fgDispatcher.relationGraph;
        auto &externalResMap = fgDispatcher.externalResMap;
        auto &rag = fgDispatcher.resourceAccessGraph;

        // record resource current in-access and out-access for every single node
        if (!fgDispatcher._accessGraphBuilt) {
            buildAccessGraph(graph, layoutGraph, resourceGraph, rag, relationGraph);
            fgDispatcher._accessGraphBuilt = true;
        }

        // found pass id in this map ? barriers you should commit when run into this pass
        // : or no extra barrier needed.
        BarrierMap &batchedBarriers = fgDispatcher.barrierMap;
        ResourceNames namesSet;
        {
            // _externalResNames records external resource between frames
            BarrierVisitor visitor(resourceGraph, batchedBarriers, externalResMap, namesSet);
            auto colors = rag.colors(scratch);
            boost::queue<AccessVertex> q;

            boost::breadth_first_visit(
                rag,
                EXPECT_START_ID,
                q,
                visitor,
                get(colors, rag));
        }

        // external res barrier for next frame
        for (const auto &externalPair : externalResMap) {
            const auto &transition = externalPair.second;
            auto resID = resourceGraph.valueIndex.at(externalPair.first);
            auto passID = transition.currStatus.vertID;
            if (batchedBarriers.find(passID) == batchedBarriers.end()) {
                batchedBarriers.emplace(passID, BarrierNode{});
            }
            batchedBarriers[passID].rearBarriers.emplace_back(Barrier{
                resID,
                gfx::BarrierType::SPLIT_BEGIN,
                externalResMap[externalPair.first].currStatus,
                {}});
        }
    }
}
#pragma endregion BUILD_BARRIERS

#pragma region PASS_REORDER

struct PassVisitor : boost::dfs_visitor<> {
    using Vertex = ResourceAccessGraph::vertex_descriptor;
    using Edge = ResourceAccessGraph::edge_descriptor;
    using Graph = ResourceAccessGraph;
    using InEdgeRange = std::pair<ResourceAccessGraph::in_edge_iterator, ResourceAccessGraph::in_edge_iterator>;
    using OutEdgeRange = std::pair<ResourceAccessGraph::out_edge_iterator, ResourceAccessGraph::out_edge_iterator>;

    PassVisitor(const Graph &ragIn, EmptyGraph &tcIn, CloseCircuits &circuitsIn) : _rag(ragIn), _relationGraph(tcIn), _circuits(circuitsIn) {}

    void start_vertex(Vertex u, const Graph &g) {}

    void discover_vertex(Vertex u, const Graph &g) {}

    void examine_edge(Edge e, const Graph &g) {
    }

    void tree_edge(Edge e, const Graph &g) {}

    void back_edge(Edge e, const Graph &g) {}

    void forward_or_cross_edge(Edge e, const Graph &g) {
        // the vertex which:
        // 1. is ancestor of targetID;
        // 2. sourceID is reachable at this specific vert;
        // is where the closed-path started.
        // note that `reachable` may results to multiple paths, choose the shortest one.
        auto sourceID = source(e, g);
        auto targetID = target(e, g);

        using RhsRangePair = std::pair<ResourceAccessGraph::in_edge_iterator, InEdgeRange>;

        bool foundIntersect = false;
        std::queue<RhsRangePair> vertQ;
        auto iterPair = in_edges(targetID, g);
        vertQ.emplace(RhsRangePair{iterPair.first, iterPair});

        // from source vertex on this edge back to branch point
        EdgeList rhsPath;
        bool rootEdge = true;
        while (!foundIntersect && !vertQ.empty()) {
            auto rangePair = vertQ.front();
            vertQ.pop();
            auto range = rangePair.second;
            for (auto iter = range.first; iter != range.second; ++iter) {
                auto srcID = source((*iter), g);
                if (sourceID == srcID) {
                    continue;
                }
                auto e = edge(srcID, sourceID, _relationGraph);
                auto recordIter = rootEdge ? iter : rangePair.first;
                if (!e.second) {
                    vertQ.emplace(RhsRangePair{recordIter, in_edges(srcID, g)});
                } else {
                    rhsPath = {(*iter), *recordIter};
                    foundIntersect = true;
                    break;
                }
            }
            rootEdge = false;
        }
        assert(foundIntersect);

        using LhsRangePair = std::pair<ResourceAccessGraph::out_edge_iterator, OutEdgeRange>;
        auto branchVert = source(rhsPath.first, g);
        bool found = false;
        std::queue<LhsRangePair> forwardVertQ;
        auto forwardIterPair = out_edges(branchVert, g);
        forwardVertQ.emplace(LhsRangePair{forwardIterPair.first, forwardIterPair});
        EdgeList lhsPath;
        rootEdge = true;
        while (!found && !forwardVertQ.empty()) {
            auto rangePair = forwardVertQ.front();
            forwardVertQ.pop();
            auto range = rangePair.second;
            for (auto iter = range.first; iter != range.second; ++iter) {
                if ((*iter) == rhsPath.first) {
                    continue;
                }
                auto dstID = target((*iter), g);
                auto e = edge(dstID, sourceID, _relationGraph);
                auto recordIter = rootEdge ? iter : rangePair.first;
                if (!e.second) {
                    forwardVertQ.emplace(LhsRangePair{recordIter, out_edges(dstID, g)});
                } else {
                    found = true;
                    lhsPath = {*recordIter, (*iter)};
                    break;
                }
            }
            rootEdge = true;
        }
        assert(found);
        lhsPath.second = e;

        _circuits.emplace_back(CloseCircuit{lhsPath, rhsPath});
    };

private:
    const RAG &_rag;
    EmptyGraph &_relationGraph;
    CloseCircuits &_circuits;
};

auto evaluateHeaviness(const RAG &rag, const ResourceGraph &rescGraph, EmptyVert vert, bool forward) {
    // forward:
    // -- true: how much resource this pass writes to, which has an effect of later passes;
    // -- false: how much resource this pass reads from, which is dependent from former passes.
    const ResourceAccessNode &accessNode = get(RAG::AccessNode, rag, static_cast<RAG::vertex_descriptor>(vert));
    int64_t score = 0;
    bool forceAdjacent = false;
    for (const auto &resc : accessNode.attachemntStatus) {
        int64_t eval = 0;
        auto rescID = resc.vertID;
        const ResourceDesc &desc = get(ResourceGraph::Desc, rescGraph, rescID);
        const ResourceTraits &traits = get(ResourceGraph::Traits, rescGraph, rescID);

        gfx::MemoryAccessBit filter = forward ? gfx::MemoryAccessBit::WRITE_ONLY : gfx::MemoryAccessBit::READ_ONLY;
        if (resc.access == filter) {
            continue;
        }

        switch (desc.dimension) {
            case ResourceDimension::BUFFER:
                eval = desc.width;
                break;
            case ResourceDimension::TEXTURE1D:
            case ResourceDimension::TEXTURE2D:
            case ResourceDimension::TEXTURE3D:
                eval = gfx::formatSize(desc.format, desc.width, desc.height, desc.depthOrArraySize);
                break;
        }

        if (traits.residency == ResourceResidency::MANAGED) {
            eval *= 2; // metal on macOS holds a copy of memory on CPU side in this mode.
            score += eval;
        } else if (traits.residency == ResourceResidency::MEMORYLESS) {
            forceAdjacent = true;
            score = forward ? std::numeric_limits<int64_t>::lowest() : std::numeric_limits<int64_t>::max();
            break;
        }
    }
    return std::tie(forceAdjacent, score);
};

void evaluateAndTryMerge(const RAG &rag, const ResourceGraph &rescGraph, EmptyGraph &relationGraph, const EmptyGraph &relationGraphTc, const EmptyVerts &lhsVerts, const EmptyVerts &rhsVerts) {
    assert(lhsVerts.size() >= 2);
    assert(rhsVerts.size() >= 2);

    auto evaluate = [&rag, &rescGraph](EmptyVert vert, bool forward) {
        return evaluateHeaviness(rag, rescGraph, vert, forward);
    };

    if (lhsVerts.size() == 2 || rhsVerts.size() == 2) {
        /*
               1 ----------- 2
                \       __--/
                 3 --``
            no extra choice, only 1 - 3 - 2
            */
        const EmptyVerts *shorterPath = lhsVerts.size() == 2 ? &lhsVerts : &rhsVerts;
        remove_edge((*shorterPath)[0], (*shorterPath)[1], relationGraph);
    } else {
        // fist and last pass in this circuit don't get involved in reorder.
        auto firstLhsNode = lhsVerts[1];
        auto lastLhsNode = lhsVerts[lhsVerts.size() - 2];

        const auto &lhsBackwardStatus = evaluate(firstLhsNode, false);
        bool lhsAdjacentToStart = std::get<0>(lhsBackwardStatus);
        const auto &lhsForwardStatus = evaluate(lastLhsNode, true);
        bool lhsAdjacentToEnd = std::get<0>(lhsForwardStatus);

        auto firstRhsNode = rhsVerts[1];
        auto lastRhsNode = rhsVerts[rhsVerts.size() - 2];

        const auto &rhsBackwardStatus = evaluate(firstRhsNode, true);
        bool rhsAdjacentToStart = std::get<0>(rhsBackwardStatus);
        const auto &rhsForwardStatus = evaluate(lastRhsNode, false);
        bool rhsAdjacentToEnd = std::get<0>(rhsForwardStatus);

        if (lhsAdjacentToStart || rhsAdjacentToEnd || lhsAdjacentToEnd || rhsAdjacentToStart) {
            const EmptyVerts *formerPath = &lhsVerts;
            const EmptyVerts *latterPath = &rhsVerts;
            if (rhsAdjacentToStart || lhsAdjacentToEnd) {
                swap(formerPath, latterPath);
            }

            remove_edge((*latterPath)[0], (*latterPath)[1], relationGraph);
            remove_edge((*formerPath)[formerPath->size() - 2], (*formerPath)[formerPath->size() - 1], relationGraph);

            tryAddEdge((*formerPath)[formerPath->size() - 2], (*latterPath)[1], relationGraph);
        }

        assert(lhsVerts.size() >= 3 && rhsVerts.size() >= 3);
        constexpr int64_t score = std::numeric_limits<int64_t>::lowest();
        ccstd::vector<std::queue<EmptyEdge>> candidateSections;
        std::queue<EmptyEdge> lhsSection;
        for (size_t i = 1; i < lhsVerts.size(); ++i) {
            auto tryE = edge(lhsVerts[i], lhsVerts[i - 1], relationGraphTc);
            auto tryRE = edge(lhsVerts[i - 1], lhsVerts[i], relationGraphTc);
            if (!tryE.second && !tryRE.second) {
                remove_edge(lhsVerts[i - 1], lhsVerts[i], relationGraph);
                candidateSections.emplace_back(lhsSection);
                std::queue<EmptyEdge> clearQ;
                lhsSection.swap(clearQ);
            }
            auto e = edge(lhsVerts[i], lhsVerts[i - 1], relationGraph);
            // verts comes in order, so either real edge exist or logic edge is added.
            CC_ASSERT(e.second);

            lhsSection.emplace(e.first);
        }
        if (candidateSections.empty()) {
            // if this one is a tight edge(no logic edge, dependent from one to its next),
            // keep this whole chain as a candidate.
            remove_edge(lhsVerts[0], lhsVerts[1], relationGraph);
            remove_edge(lhsVerts[lhsVerts.size() - 2], lhsVerts[lhsVerts.size() - 1], relationGraph);
            candidateSections.emplace_back(std::move(lhsSection));
        }

        std::queue<EmptyEdge> rhsSection;
        for (size_t i = 1; i < rhsVerts.size(); ++i) {
            auto tryE = edge(rhsVerts[i], rhsVerts[i - 1], relationGraphTc);
            auto tryRE = edge(rhsVerts[i - 1], rhsVerts[i], relationGraphTc);
            if (!tryE.second && !tryRE.second) {
                remove_edge(rhsVerts[i - 1], rhsVerts[i], relationGraph);
                candidateSections.emplace_back(rhsSection);
                std::queue<EmptyEdge> clearQ;
                rhsSection.swap(clearQ);
            }
            auto e = edge(rhsVerts[i], rhsVerts[i - 1], relationGraph);
            // verts comes in order, so either real edge exist or logic edge is added.
            CC_ASSERT(e.second);
            rhsSection.emplace(e.first);
        }

        // lhs verts already put in.
        if (candidateSections.size() == 1) {
            remove_edge(rhsVerts[0], rhsVerts[1], relationGraph);
            remove_edge(rhsVerts[rhsVerts.size() - 2], rhsVerts[rhsVerts.size() - 1], relationGraph);
            candidateSections.emplace_back(std::move(rhsSection));
        }

        assert(candidateSections.size() >= 2);

        ScoreMap scMap;
        auto tailVert = lhsVerts[0];
        while (!candidateSections.empty()) {
            int64_t lightest = std::numeric_limits<int64_t>::max();
            uint32_t index = 0;
            for (size_t i = 0; i < candidateSections.size(); ++i) {
                auto e = candidateSections[i].front();
                auto srcVert = source(e, relationGraph);
                auto dstVert = target(e, relationGraph);
                int64_t srcBackwardScore = 0;
                int64_t dstForwardScore = 0;
                if (scMap.find(srcVert) == scMap.end()) {
                    auto res = evaluate(srcVert, false);
                    srcBackwardScore = std::get<1>(res);
                    res = evaluate(srcVert, true);
                    auto srcForwardScore = std::get<1>(res);
                    scMap.emplace(srcVert, std::pair<int64_t, int64_t>(srcBackwardScore, srcForwardScore));
                } else {
                    srcBackwardScore = std::get<0>(scMap.at(srcVert));
                }
                if (scMap.find(dstForwardScore) == scMap.end()) {
                    auto res = evaluate(dstVert, false);
                    dstForwardScore = std::get<1>(res);
                    res = evaluate(dstVert, true);
                    auto dstBackwardScore = std::get<1>(res);
                    scMap.emplace(dstVert, std::pair<int64_t, int64_t>(dstBackwardScore, dstForwardScore));
                } else {
                    dstForwardScore = std::get<1>(scMap.at(dstVert));
                }

                auto score = dstForwardScore - srcBackwardScore;
                if (lightest > score) {
                    lightest = score;
                    index = i;
                }
            }
            auto e = candidateSections[index].front();
            candidateSections[index].pop();
            if (candidateSections[index].empty()) {
                auto iter = candidateSections.begin();
                std::advance(iter, index);
                candidateSections.erase(iter);
            }
            auto srcVert = source(e, relationGraph);
            auto dstVert = target(e, relationGraph);
            tryAddEdge(tailVert, srcVert, relationGraph);
            tailVert = dstVert;
        }

        tryAddEdge(tailVert, lhsVerts.back(), relationGraph);
    }
}

// return : can be further reduced?
bool reduce(const RAG &rag, const ResourceGraph &rescGraph, EmptyGraph &relationGraph, EmptyGraph &relationGraphTc, const CloseCircuit &circuit) {
    auto checkPath = [&relationGraph](std::stack<EmptyGraph::vertex_descriptor> &vertices, EmptyGraph::vertex_descriptor endVert, EmptyVerts &stackTrace) {
        bool simpleGraph = true;
        while (!vertices.empty()) {
            auto vert = vertices.top();
            vertices.pop();
            stackTrace.emplace_back(vert);

            if (endVert == vert) {
                break;
            }

            if (out_degree(vert, relationGraph) > 1) {
                simpleGraph = false;
                break;
            }
            auto r = out_edges(vert, relationGraph);
            for (auto rIter = r.first; rIter != r.second; ++rIter) {
                auto dstID = target(*rIter, relationGraph);
                vertices.push(dstID);
            }
            if (r.first == r.second) {
                stackTrace.pop_back();
            }
        }
        return simpleGraph;
    };

    // check if there is a sub branch on lhs
    auto lhsEdges = circuit.first;
    auto startVert = target(lhsEdges.first, relationGraph);
    auto endVert = source(lhsEdges.second, relationGraph);

    std::stack<EmptyGraph::vertex_descriptor> vertices;
    vertices.emplace(startVert);

    EmptyVerts lhsVisited;
    auto branchStartVert = source(lhsEdges.first, relationGraph);
    auto branchEndVert = target(lhsEdges.second, relationGraph);
    lhsVisited.push_back(branchStartVert);
    // check if there is a branch on lhs path
    if (!checkPath(vertices, endVert, lhsVisited)) {
        return false;
    }
    lhsVisited.push_back(branchEndVert);
    // if it's a simple graph, lhs path must can be dfs to the end at the first time.
    assert(vertices.empty());

    auto rhsEdges = circuit.second;
    startVert = target(rhsEdges.first, relationGraph);
    endVert = source(rhsEdges.second, relationGraph);
    vertices.emplace(startVert);

    EmptyVerts rhsVisited;
    rhsVisited.push_back(branchStartVert);
    if (!checkPath(vertices, endVert, rhsVisited)) {
        return false;
    }
    rhsVisited.push_back(branchEndVert);

    // merge this circuit
    // from
    /*          2 - 3 - 4
              /           \
            1               8
              \           /
                5 - 6 - 7

        to
            1 - A - B - 8 or 1 - B - A -8 depends on algorithm

            ${A} : 2 - 3 - 4
            ${B} : 5 - 6 - 7
        */

    evaluateAndTryMerge(rag, rescGraph, relationGraph, relationGraphTc, lhsVisited, rhsVisited);

    return true;
}

template <typename RelationGraph, typename TargetGraph>
void applyRelation(RelationGraph &relationGraph, const TargetGraph &targetGraph) {
    CC_EXPECTS(relationGraph.vertices.size() == targetGraph.vertices.size());

    // remove all edges
    for (auto vert : targetGraph.vertices) {
        clear_in_edges(vert, targetGraph);
        clear_out_edges(vert, targetGraph);
    }

    for (auto vert : relationGraph.vertices) {
        auto inEdges = in_edges(vert, relationGraph);
        for (auto e : makeRange(inEdges)) {
            auto srcVert = source(e, relationGraph);
            // auto checkEdge = edge(srcVert, vert, targetGraph);
            add_edge(srcVert, vert, targetGraph);
        }
    }
}

void passReorder(FrameGraphDispatcher &fgDispatcher) {
    auto *scratch = fgDispatcher.scratch;
    const auto &graph = fgDispatcher.graph;
    const auto &layoutGraph = fgDispatcher.layoutGraph;
    const auto &resourceGraph = fgDispatcher.resourceGraph;
    auto &relationGraph = fgDispatcher.relationGraph;
    auto &rag = fgDispatcher.resourceAccessGraph;

    if (!fgDispatcher._accessGraphBuilt) {
        buildAccessGraph(graph, layoutGraph, resourceGraph, rag, relationGraph);
        fgDispatcher._accessGraphBuilt = true;
    }

    {
        // determine do mem saving how many times
        EmptyGraph relationGraphTc;
        boost::transitive_closure(relationGraph, relationGraphTc);

        CloseCircuits circuits;
        std::vector<RAG::edge_descriptor> crossEdges;
        PassVisitor visitor(rag, relationGraphTc, circuits);
        auto colors = rag.colors(scratch);
        boost::depth_first_search(rag, visitor, get(colors, rag));

        float percent = 0.0F;
        uint32_t count = 0;
        auto total = circuits.size();

        float memsavePercent = 1.0F - fgDispatcher._paralellExecWeight;
        for (auto iter = circuits.begin(); (iter != circuits.end()) && (percent < memsavePercent);) {
            bool reduced = reduce(rag, resourceGraph, relationGraph, relationGraphTc, (*iter));
            if (reduced) {
                ++count;
                iter = circuits.erase(iter);
                percent = count / static_cast<float>(total);
            } else {
                ++iter;
            }
        }

        // topological sort
        bool empty = relationGraph.vertices.empty();
        ScoreMap scoreMap;
        std::queue<EmptyVert> vertQ;
        EmptyVerts candidates;

        for (size_t i = 0; i < relationGraph.vertices.size(); ++i) {
            if (in_degree(static_cast<uint32_t>(i), relationGraph) == 0) {
                if (std::find(candidates.begin(), candidates.end(), i) == candidates.end()) {
                    candidates.push_back(static_cast<uint32_t>(i));
                }
            }
        }

        std::vector<EmptyVert> candidateBuffer;
        uint32_t coloredVerts = 0;
        while (coloredVerts < relationGraph.vertices.size()) {
            // decreasing order, pop back from vector, push into queue, then it's ascending order.
            std::sort(candidates.begin(), candidates.end(), [&](EmptyVert lhsVert, EmptyVert rhsVert) {
                int64_t lhsForwardScore{0};
                int64_t rhsForwardScore{0};
                int64_t lhsBackwardScore{0};
                int64_t rhsBackwardScore{0};
                if (scoreMap.find(lhsVert) == scoreMap.end()) {
                    const auto &lhsForwardStatus = evaluateHeaviness(rag, resourceGraph, lhsVert, true);
                    lhsForwardScore = get<1>(lhsForwardStatus);
                    const auto &lhsBackwardStatus = evaluateHeaviness(rag, resourceGraph, lhsVert, false);
                    lhsBackwardScore = get<1>(lhsBackwardStatus);
                    scoreMap.emplace(lhsVert, std::pair<int64_t, int64_t>{lhsBackwardScore, lhsForwardScore});
                } else {
                    lhsBackwardScore = scoreMap[lhsVert].first;
                    lhsForwardScore = scoreMap[lhsVert].second;
                }

                if (scoreMap.find(rhsVert) == scoreMap.end()) {
                    const auto &rhsForwardStatus = evaluateHeaviness(rag, resourceGraph, rhsVert, true);
                    rhsForwardScore = get<1>(rhsForwardStatus);
                    const auto &rhsBackwardStatus = evaluateHeaviness(rag, resourceGraph, rhsVert, false);
                    rhsBackwardScore = get<1>(rhsBackwardStatus);
                    scoreMap.emplace(rhsVert, std::pair<int64_t, int64_t>{rhsBackwardScore, rhsForwardScore});
                } else {
                    rhsBackwardScore = scoreMap[rhsVert].first;
                    rhsForwardScore = scoreMap[rhsVert].second;
                }
                return lhsBackwardScore - lhsForwardScore > rhsBackwardScore - rhsForwardScore;
            });

            const auto vert = candidates.back();
            candidates.pop_back();
            vertQ.push(vert);
            if (!candidateBuffer.empty()) {
                candidates.insert(candidates.end(), candidateBuffer.begin(), candidateBuffer.end());
                candidateBuffer.clear();
            }

            for (const auto nextGeneration : makeRange(out_edges(vert, relationGraph))) {
                auto targetID = target(nextGeneration, relationGraph);
                if (in_degree(targetID, relationGraph) == 1) {
                    candidateBuffer.emplace_back(targetID);
                }
            }

            auto deprecatedEdges = out_edges(vert, relationGraph);
            for (auto iter = deprecatedEdges.first; iter < deprecatedEdges.second;) {
                remove_edge(*iter, relationGraph);
                deprecatedEdges = out_edges(vert, relationGraph);
                iter = deprecatedEdges.first;
            }

            if (candidates.empty()) {
                candidates.insert(candidates.end(), candidateBuffer.begin(), candidateBuffer.end());
                candidateBuffer.clear();
            }

            coloredVerts++;
        }

        // sort graph by the order in queue
        for (const auto &vert : makeRange(vertices(rag))) {
            clear_in_edges(vert, rag);
            clear_out_edges(vert, rag);
        }

        auto vert = vertQ.front();
        vertQ.pop();
        while (!vertQ.empty()) {
            auto nextVert = vertQ.front();
            vertQ.pop();
            add_edge(vert, nextVert, rag);
            vert = nextVert;
        }
    }
}

#pragma endregion PASS_REORDER

void memoryAliasing(FrameGraphDispatcher &fgDispatcher) {
}

#pragma region assisstantFuncDefinition
template <typename Graph>
bool tryAddEdge(uint32_t srcVertex, uint32_t dstVertex, Graph &graph) {
    auto e = edge(srcVertex, dstVertex, graph);
    if (!e.second) {
        auto res = add_edge(srcVertex, dstVertex, graph);
        CC_ENSURES(res.second);
        return true;
    }
    return false;
}

bool isStatusDependent(const AccessStatus &lhs, const AccessStatus &rhs) {
    bool res = true;
    if (lhs.passType == rhs.passType &&
        lhs.visibility == rhs.visibility &&
        lhs.access == gfx::MemoryAccessBit::READ_ONLY &&
        rhs.access == gfx::MemoryAccessBit::READ_ONLY) {
        res = false;
    }
    return res;
}

void addCopyAccessNode(RAG &rag, const ResourceGraph &rg, ResourceAccessNode &node, InputStatusTuple status, const Range &range) {
    // std::tie(passType, rescName, visibility, access) ;
    PassType passType = std::get<0>(status);
    const PmrString &rescName = std::get<1>(status);
    gfx::ShaderStageFlagBit visibility = std::get<2>(status);
    gfx::MemoryAccessBit access = std::get<3>(status);

    uint32_t rescID = rg.valueIndex.at(rescName);

    CC_EXPECTS(rg.valueIndex.find(rescName) != rg.valueIndex.end());
    if (std::find(rag.resourceNames.begin(), rag.resourceNames.end(), rescName) == rag.resourceNames.end()) {
        rag.resourceIndex.emplace(rescName, rescID);
        rag.resourceNames.emplace_back(rescName);
    }

    node.attachemntStatus.emplace_back(AccessStatus{
        rescID,
        visibility,
        access,
        passType,
        range,
    });
}

void addAccessNode(RAG &rag, const ResourceGraph &rg, ResourceAccessNode &node, InputStatusTuple status) {
    // std::tie(passType, rescName, visibility, access) ;
    PassType passType = std::get<0>(status);
    const PmrString &rescName = std::get<1>(status);
    gfx::ShaderStageFlagBit visibility = std::get<2>(status);
    gfx::MemoryAccessBit access = std::get<3>(status);

    uint32_t rescID = rg.valueIndex.at(rescName);
    auto resourceDesc = get(ResourceGraph::Desc, rg, rescID);
    Range range;
    if (resourceDesc.dimension == ResourceDimension::BUFFER) {
        range = BufferRange{0, resourceDesc.depthOrArraySize};
    } else {
        range = TextureRange{0, 1, 0, resourceDesc.mipLevels};
    }

    CC_EXPECTS(rg.valueIndex.find(rescName) != rg.valueIndex.end());
    if (std::find(rag.resourceNames.begin(), rag.resourceNames.end(), rescName) == rag.resourceNames.end()) {
        rag.resourceIndex.emplace(rescName, rescID);
        rag.resourceNames.emplace_back(rescName);
    }

    node.attachemntStatus.emplace_back(AccessStatus{
        rescID,
        visibility,
        access,
        passType,
        range,
    });
}

AccessVertex dependencyCheck(RAG &rag, AccessTable &accessRecord, AccessVertex curVertID, const InputStatusTuple &status) {
    PassType passType = std::get<0>(status);
    const PmrString &rescName = std::get<1>(status);
    gfx::ShaderStageFlagBit visibility = std::get<2>(status);
    gfx::MemoryAccessBit access = std::get<3>(status);

    AccessVertex lastVertID = INVALID_ID;
    CC_EXPECTS(rag.resourceIndex.find(rescName) != rag.resourceIndex.end());
    auto resourceID = rag.resourceIndex[rescName];
    auto iter = accessRecord.find(resourceID);
    if (iter == accessRecord.end()) {
        accessRecord.emplace(
            resourceID,
            ResourceTransition{
                {},
                {curVertID, visibility, access, passType, Range{}}});
    } else {
        ResourceTransition &trans = iter->second;
        if (access == gfx::MemoryAccessBit::READ_ONLY && trans.currStatus.access == gfx::MemoryAccessBit::READ_ONLY) {
            // current READ, no WRITE before in this frame, it's expected to be external.
            bool dirtyExternalRes = trans.lastStatus.vertID == INVALID_ID;
            if (!dirtyExternalRes) {
                tryAddEdge(trans.lastStatus.vertID, curVertID, rag);
            }
            trans.currStatus = {curVertID, visibility, access, passType, Range{}};
            lastVertID = trans.lastStatus.vertID;
            rag.externalPasses.emplace_back(curVertID);
        } else {
            bool needTransition = (trans.currStatus.access != access) || (trans.currStatus.passType != passType) || (trans.currStatus.visibility != visibility);
            if (needTransition) {
                trans.lastStatus = trans.currStatus;
                trans.currStatus = {curVertID, visibility, access, passType, Range{}};
                tryAddEdge(trans.lastStatus.vertID, curVertID, rag);
                lastVertID = trans.lastStatus.vertID;
            }
        }
    }
    return lastVertID;
}

gfx::ShaderStageFlagBit getVisibilityByDescName(const LGD &lgd, uint32_t passID, const PmrString &slotName) {
    auto vis = gfx::ShaderStageFlagBit::NONE;

    bool found = false;

    auto compare = [](const PmrString &name, const uint32_t slot) {
        return boost::lexical_cast<uint32_t>(name) == slot;
    };

    auto iter = lgd.attributeIndex.find(slotName);
    if (iter == lgd.attributeIndex.end()) {
        iter = lgd.constantIndex.find(slotName);
        CC_EXPECTS(iter != lgd.constantIndex.end());
    }
    auto nameID = iter->second;

    auto visIter = lgd.stages[passID].descriptorVisibility.find(nameID);
    CC_EXPECTS(visIter != lgd.stages[passID].descriptorVisibility.end());
    return visIter->second;
};

void processRasterPass(RAG &rag, EmptyGraph &relationGraph, const LGD &lgd, const ResourceGraph &rescGraph, AccessTable &accessRecord, uint32_t passID, const RasterPass &pass) {
    auto rlgVertID = add_vertex(relationGraph);

    auto vertID = add_vertex(rag, passID);
    auto &node = get(RAG::AccessNode, rag, vertID);
    bool dependent = false;
    for (const auto &pair : pass.rasterViews) {
        const auto &rasterView = pair.second;
        gfx::ShaderStageFlagBit visibility = getVisibilityByDescName(lgd, vertID, pair.first);
        auto access = toGfxAccess(rasterView.accessType);
        addAccessNode(rag, rescGraph, node, InputStatusTuple{PassType::RASTER, rasterView.slotName, visibility, access});
        auto lastVertId = dependencyCheck(rag, accessRecord, vertID, InputStatusTuple{PassType::RASTER, rasterView.slotName, visibility, access});
        if (lastVertId != INVALID_ID) {
            tryAddEdge(lastVertId, vertID, rag);
            tryAddEdge(lastVertId, rlgVertID, relationGraph);
            dependent = true;
        }
    }
    for (const auto &pair : pass.computeViews) {
        const auto &values = pair.second;
        gfx::ShaderStageFlagBit visibility = getVisibilityByDescName(lgd, passID, pair.first);
        for (const auto &computeView : values) {
            auto access = toGfxAccess(computeView.accessType);
            addAccessNode(rag, rescGraph, node, InputStatusTuple{PassType::COMPUTE, computeView.name, visibility, access});
            auto lastVertId = dependencyCheck(rag, accessRecord, vertID, InputStatusTuple{PassType::COMPUTE, computeView.name, visibility, access});
            if (lastVertId != INVALID_ID) {
                tryAddEdge(lastVertId, vertID, rag);
                tryAddEdge(lastVertId, rlgVertID, relationGraph);
                dependent = true;
            }
        }
    }

    if (!dependent) {
        tryAddEdge(EXPECT_START_ID, vertID, rag);
        tryAddEdge(EXPECT_START_ID, rlgVertID, relationGraph);
    }
    std::sort(node.attachemntStatus.begin(), node.attachemntStatus.end(), [](const AccessStatus &lhs, const AccessStatus &rhs) { return lhs.vertID < rhs.vertID; });
}

void processComputePass(RAG &rag, EmptyGraph &relationGraph, const LGD &lgd, const ResourceGraph &rescGraph, AccessTable &accessRecord, uint32_t passID, const ComputePass &pass) {
    auto rlgVertID = add_vertex(relationGraph);
    auto vertID = add_vertex(rag, passID);
    auto &node = get(RAG::AccessNode, rag, vertID);
    bool dependent = false;
    for (const auto &pair : pass.computeViews) {
        const auto &values = pair.second;
        gfx::ShaderStageFlagBit visibility = getVisibilityByDescName(lgd, passID, pair.first);
        for (const auto &computeView : values) {
            auto access = toGfxAccess(computeView.accessType);

            addAccessNode(rag, rescGraph, node, InputStatusTuple{PassType::COMPUTE, computeView.name, visibility, access});
            auto lastVertId = dependencyCheck(rag, accessRecord, vertID, InputStatusTuple{PassType::COMPUTE, computeView.name, visibility, access});
            if (lastVertId != INVALID_ID) {
                tryAddEdge(lastVertId, vertID, rag);
                tryAddEdge(lastVertId, rlgVertID, relationGraph);
                dependent = true;
            }
        }
    }
    if (!dependent) {
        tryAddEdge(EXPECT_START_ID, vertID, rag);
        tryAddEdge(EXPECT_START_ID, rlgVertID, relationGraph);
    }
    std::sort(node.attachemntStatus.begin(), node.attachemntStatus.end(), [](const AccessStatus &lhs, const AccessStatus &rhs) { return lhs.vertID < rhs.vertID; });
}

void processCopyPass(RAG &rag, EmptyGraph &relationGraph, const LGD & /*lgd*/, const ResourceGraph &rescGraph, AccessTable &accessRecord, uint32_t passID, const CopyPass &pass) {
    auto rlgVertID = add_vertex(relationGraph);
    auto vertID = add_vertex(rag, passID);
    auto &node = get(RAG::AccessNode, rag, vertID);
    bool dependent = false;
    for (const auto &pair : pass.copyPairs) {
        auto sourceRange = Range{
            TextureRange{
                pair.sourceFirstSlice,
                pair.sourcePlaneSlice,
                pair.mipLevels,
                pair.numSlices,
            }};
        auto targetRange = Range{
            TextureRange{
                pair.targetFirstSlice,
                pair.targetPlaneSlice,
                pair.mipLevels,
                pair.numSlices,
            }};
        addCopyAccessNode(rag, rescGraph, node, InputStatusTuple{PassType::COPY, pair.source, defaultVisibility, gfx::MemoryAccessBit::READ_ONLY}, sourceRange);
        addCopyAccessNode(rag, rescGraph, node, InputStatusTuple{PassType::COPY, pair.target, defaultVisibility, gfx::MemoryAccessBit::WRITE_ONLY}, targetRange);
        uint32_t lastVertSrc = dependencyCheck(rag, accessRecord, vertID, InputStatusTuple{PassType::COPY, pair.source, defaultVisibility, gfx::MemoryAccessBit::READ_ONLY});
        if (lastVertSrc != INVALID_ID) {
            tryAddEdge(lastVertSrc, vertID, rag);
            tryAddEdge(lastVertSrc, rlgVertID, relationGraph);
            dependent = true;
        }
        uint32_t lastVertDst = dependencyCheck(rag, accessRecord, vertID, InputStatusTuple{PassType::COPY, pair.source, defaultVisibility, gfx::MemoryAccessBit::WRITE_ONLY});
        if (lastVertDst != INVALID_ID) {
            tryAddEdge(lastVertDst, vertID, rag);
            tryAddEdge(lastVertDst, rlgVertID, relationGraph);
            dependent = true;
        }
    }
    if (!dependent) {
        tryAddEdge(EXPECT_START_ID, vertID, rag);
        tryAddEdge(EXPECT_START_ID, rlgVertID, relationGraph);
    }
    std::sort(node.attachemntStatus.begin(), node.attachemntStatus.end(), [](const AccessStatus &lhs, const AccessStatus &rhs) { return lhs.vertID < rhs.vertID; });
}

void processRaytracePass(RAG &rag, EmptyGraph &relationGraph, const LGD &lgd, const ResourceGraph &rescGraph, AccessTable &accessRecord, uint32_t passID, const RaytracePass &pass) {
    auto rlgVertID = add_vertex(relationGraph);
    auto vertID = add_vertex(rag, passID);
    auto &node = get(RAG::AccessNode, rag, vertID);
    bool dependent = false;
    for (const auto &pair : pass.computeViews) {
        const auto &values = pair.second;
        gfx::ShaderStageFlagBit visibility = getVisibilityByDescName(lgd, passID, pair.first);
        for (const auto &computeView : values) {
            auto access = toGfxAccess(computeView.accessType);
            addAccessNode(rag, rescGraph, node, InputStatusTuple{PassType::COMPUTE, computeView.name, visibility, access});
            auto lastVertId = dependencyCheck(rag, accessRecord, vertID, InputStatusTuple{PassType::COMPUTE, computeView.name, visibility, access});
            if (lastVertId != INVALID_ID) {
                tryAddEdge(lastVertId, vertID, rag);
                tryAddEdge(lastVertId, rlgVertID, relationGraph);
                dependent = true;
            }
        }
    }
    if (!dependent) {
        tryAddEdge(EXPECT_START_ID, vertID, rag);
        tryAddEdge(EXPECT_START_ID, rlgVertID, relationGraph);
    }
    std::sort(node.attachemntStatus.begin(), node.attachemntStatus.end(), [](const AccessStatus &lhs, const AccessStatus &rhs) { return lhs.vertID < rhs.vertID; });
}

void processPresentPass(RAG &rag, EmptyGraph &relationGraph, const LGD &lgd, const ResourceGraph &rescGraph, AccessTable &accessRecord, uint32_t passID, const PresentPass &pass) {
    auto rlgVertID = add_vertex(relationGraph);
    auto vertID = add_vertex(rag, passID);
    auto &node = get(RAG::AccessNode, rag, vertID);
    bool dependent = false;
    for (const auto &pair : pass.presents) {
        gfx::ShaderStageFlagBit visibility = getVisibilityByDescName(lgd, passID, pair.first);
        auto lastVertId = dependencyCheck(rag, accessRecord, vertID, InputStatusTuple{PassType::PRESENT, pair.first, visibility, gfx::MemoryAccessBit::WRITE_ONLY});
        addAccessNode(rag, rescGraph, node, InputStatusTuple{PassType::PRESENT, pair.first, defaultVisibility, gfx::MemoryAccessBit::WRITE_ONLY});
        if (lastVertId != INVALID_ID) {
            tryAddEdge(lastVertId, vertID, rag);
            tryAddEdge(lastVertId, rlgVertID, relationGraph);
            dependent = true;
        }
    }
    if (!dependent) {
        // LOG("~~~~~~~~~~ Found an empty pipeline! ~~~~~~~~~~");
        tryAddEdge(EXPECT_START_ID, vertID, rag);
        tryAddEdge(EXPECT_START_ID, rlgVertID, relationGraph);
    }

    rag.presentPassID = vertID;
    std::sort(node.attachemntStatus.begin(), node.attachemntStatus.end(), [](const AccessStatus &lhs, const AccessStatus &rhs) { return lhs.vertID < rhs.vertID; });
}

#pragma endregion assisstantFuncDefinition

} // namespace render

} // namespace cc
