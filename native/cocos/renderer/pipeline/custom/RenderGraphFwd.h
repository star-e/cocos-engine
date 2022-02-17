// clang-format off
#pragma once
#include <boost/variant2/variant.hpp>
#include "cocos/renderer/gfx-base/GFXDef-common.h"
#include "cocos/renderer/pipeline/PipelineSceneData.h"
#include "cocos/renderer/pipeline/custom/RenderCommonFwd.h"
#include "cocos/scene/Camera.h"

namespace cc {

namespace render {

enum class ResourceFlags : uint32_t;
enum class TextureLayout;

struct ResourceDesc;
struct ResourceTraits;
struct ResourceGraph;

enum class AttachmentType;
enum class AccessType;

struct RasterView;

enum class ClearValueType;

struct ComputeView;
struct RasterSubpass;
struct SubpassGraph;
struct RasterPassData;
struct ComputePassData;
struct CopyPair;
struct CopyPassData;
struct MovePair;
struct MovePassData;
struct RaytracePassData;
struct Queue_;
struct Scene_;
struct Dispatch_;
struct Blit_;
struct Present_;
struct RenderQueueData;
struct SceneData;
struct Dispatch;
struct Blit;
struct PresentPassData;
struct RenderData;
struct RenderGraph;

} // namespace render

} // namespace cc

// clang-format on
