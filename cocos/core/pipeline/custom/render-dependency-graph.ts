import { LayoutGraph, LayoutGraphData } from './layout-graph';
import { AccessType, RenderGraph, RenderGraphValue, ResourceDesc, ResourceFlags, ResourceGraph, ResourceTraits, SceneData } from './render-graph';
import { QueueHint, ResourceDimension } from './types';

export class RenderResource {
    protected _type: ResourceDimension = ResourceDimension.TEXTURE2D;
    // protected _idx = -1;
    protected _physicalIndex = -1;
    protected _name = '';
    protected _writtenInPasses: number[] = [];
    protected _readInPasses: number[] = [];
    // protected _queueFlag: RenderGraphValue = RenderGraphValue.Raster;
    constructor (type: ResourceDimension) {
        this._type = type;
        // this._idx = index;
    }
    // set graphQueueFlag (value: RenderGraphValue) {
    //     this._queueFlag = value;
    // }
    // get graphQueueFlag () : RenderGraphValue { return this._queueFlag; }
    // get index (): number { return this._idx; }
    get type (): ResourceDimension { return this._type; }
    set type (value: ResourceDimension) { this._type = value; }
    get name (): string { return this._name; }
    set name (value: string) { this._name = value; }
    set writtenInPass (index: number) {
        this._writtenInPasses.push(index);
    }
    get writtenPasses (): number[] { return this._writtenInPasses; }
    set readInPass (index: number) {
        this._readInPasses.push(index);
    }
    get readPasses (): number[] { return this._readInPasses; }
}

export class RenderTextureResource extends RenderResource {
    protected _info: ResourceDesc | null = null;
    protected _trait: ResourceTraits | null = null;
    protected _transient = false;
    constructor (type: ResourceDimension.TEXTURE1D |
    ResourceDimension.TEXTURE2D |
    ResourceDimension.TEXTURE3D, desc: ResourceDesc, traits: ResourceTraits) {
        super(type);
        this.setResourceTraits(traits);
        this.setAttachmentInfo(desc);
    }
    set transientState (enable: boolean) {
        this._transient = enable;
    }
    get transientState () { return this._transient; }
    setAttachmentInfo (info: ResourceDesc) {
        this._info = info;
    }
    getAttachmentInfo () {
        return this._info;
    }
    setResourceTraits (traits: ResourceTraits) {
        this._trait = traits;
    }
    getResourceTraits () {
        return this._trait;
    }
    addImageUsage (flags: ResourceFlags) {
        if (this._info) {
            this._info.flags |= flags;
        }
    }
    getImageUsage () {
        return this._info && this._info.flags;
    }
}

// There are no detailed implementations of BufferInfo yet, just inheritance
export class RenderBufferResource extends RenderResource {
    constructor () {
        super(ResourceDimension.BUFFER);
    }
}

export class RenderPass {
    protected _name = '';
    protected _graphBuild: RenderDependencyGraph | null = null;
    protected _graphFlag: RenderGraphValue = RenderGraphValue.Raster;
    protected _queueHint: QueueHint = QueueHint.RENDER_OPAQUE;
    protected _outputs: RenderResource[] = [];
    protected _inputs: RenderResource[] = [];
    protected _sceneData: SceneData | null = null;
    constructor (graph: RenderDependencyGraph, graphFlag: RenderGraphValue = RenderGraphValue.Raster) {
        this._graphBuild = graph;
        this._graphFlag = graphFlag;
    }
    set name (value: string) {
        this._name = value;
    }
    get name () { return this._name; }
    set graphFlag (value: RenderGraphValue) {
        this._graphFlag = value;
    }
    get graphFlag (): RenderGraphValue { return this._graphFlag; }
    get sceneData (): SceneData | null { return this._sceneData; }
    set sceneData (value: SceneData | null) { this._sceneData = value; }
    set queueHint (value: QueueHint) { this._queueHint = value; }
    get queueHint (): QueueHint { return this._queueHint; }
    get graph (): RenderDependencyGraph { return this._graphBuild!; }
    get inputs () { return this._inputs; }
    get outputs () { return this._outputs; }
    addOutput (resTex: RenderTextureResource) {}
    addInput (resTex: RenderTextureResource) {}
}

export class RenderDependencyGraph {
    protected _renderGraph: RenderGraph;
    protected _resourceGraph: ResourceGraph;
    protected _layoutGraph: LayoutGraphData;
    protected _passes: RenderPass[] = [];
    protected _resources: RenderResource[] = [];
    protected _passToIdx: number[] = [];
    protected _resourceToIdx: number[] = [];
    constructor (renderGraph: RenderGraph, resourceGraph: ResourceGraph, layoutGraph: LayoutGraphData) {
        this._renderGraph = renderGraph;
        this._resourceGraph = resourceGraph;
        this._layoutGraph = layoutGraph;
    }
    protected _createRenderResource () {

    }
    protected _createRenderPass (passIdx: number, renderPass: RenderPass | null = null): RenderPass {
        const passName = this._renderGraph.vertexName(passIdx);
        const layoutName = this._renderGraph.getLayout(passIdx);
        const renderData = this._renderGraph.getData(passIdx);
        const graphDataType = this._renderGraph.id(passIdx);
        const childs = this._renderGraph.children(passIdx);
        switch (graphDataType) {
        case RenderGraphValue.Blit:
            break;
        case RenderGraphValue.Compute:
            break;
        case RenderGraphValue.Copy:
            break;
        case RenderGraphValue.Dispatch:
            break;
        case RenderGraphValue.Move:
            break;
        case RenderGraphValue.Present:
            break;
        case RenderGraphValue.Queue:
            break;
        case RenderGraphValue.Raster: {
            const rasterPass = this._renderGraph.tryGetRaster(passIdx);
            if (!renderPass) {
                renderPass = new RenderPass(this, RenderGraphValue.Raster);
                renderPass.name = passName;
            }
            if (rasterPass) {
                for (const resIdx of this._resourceGraph.vertices()) {
                    const resName = this._resourceGraph.vertexName(resIdx);
                    const rasterView = rasterPass.rasterViews.get(resName);
                    if (rasterView) {
                        const resDesc = this._resourceGraph.getDesc(resIdx);
                        const resTraits = this._resourceGraph.getTraits(resIdx);
                        switch (resDesc.dimension) {
                        case ResourceDimension.TEXTURE1D:
                        case ResourceDimension.TEXTURE2D:
                        case ResourceDimension.TEXTURE3D: {
                            const renderTex = new RenderTextureResource(resDesc.dimension, resDesc, resTraits);
                            switch (rasterView.accessType) {
                            case AccessType.WRITE:
                                renderPass.addOutput(renderTex);
                                break;
                            case AccessType.READ:
                                renderPass.addInput(renderTex);
                                break;
                            case AccessType.READ_WRITE:
                                renderPass.addInput(renderTex);
                                renderPass.addOutput(renderTex);
                                break;
                            default:
                            }
                        }
                            break;
                        case ResourceDimension.BUFFER:
                            break;
                        default:
                        }
                    }
                }
            }
        }
            break;
        case RenderGraphValue.Raytrace:
            break;
        case RenderGraphValue.Scene:
            break;
        default:
        }
        for (const key of childs) {
            console.log(key, childs);
        }
        return renderPass!;
    }
    protected _buildIO () {
        for (const idx of this._renderGraph.vertices()) {
            const renderPass = this._createRenderPass(idx);
        }
    }
    build () {
        // determine rendergraph inputs and outputs, and resources that are neither
        this._buildIO();
    }
    addPass (name: string, queueFlag: RenderGraphValue = RenderGraphValue.Raster) {

    }
    findPass (name: string) {}
    getTextureResource (name: string) {}
}
