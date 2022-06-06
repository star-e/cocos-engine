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

#include "State.h"
#include "gfx-base/GFXDef-common.h"
#include "pipeline/custom/GslUtils.h"

namespace cc {
namespace framegraph {

namespace {
    /* struct AccessKey {
        ResourceType type;
        AccessStatus status;
        gfx::Format format;
    
        bool operator==(const AccessKey& other) const {
            bool res = false;
            if(type == ResourceType::BUFFER) {
                res = status.access == other.status.access && status.visibility == other.status.visibility &&
                    status.passType == other.status.passType;
            } else {
                res = status.access == other.status.access && status.visibility == other.status.visibility &&
                    status.passType == other.status.passType && format == other.format;
            }
            return false;
        }
    };

    gfx::AccessFlagBit getAccess()

    const ccstd::unordered_map<AccessKey, gfx::AccessFlagBit> accessMap = {
        {{ResourceType::BUFFER, {}}, {}}  
    }; */
}

std::pair<gfx::GFXObject*, gfx::GFXObject*> getBarrier(const ResourceBarrier& barrierInfo, const FrameGraph& graph) noexcept {
    std::pair<gfx::GFXObject*, gfx::GFXObject*> res;
    return res;
}

} // namespace framegraph
} // namespace cc
