import { LayoutGraph, LayoutGraphData } from './layout-graph';
import { RenderGraph, RenderGraphValue, ResourceDesc, ResourceFlags, ResourceGraph, ResourceTraits } from './render-graph';
import { ResourceDimension } from './types';

export class RenderResource {
    protected _type: ResourceDimension = ResourceDimension.TEXTURE2D;
    protected _idx = -1;
    protected _physicalIndex = -1;
    protected _name = '';
    protected _writtenInPasses: number[] = [];
    protected _readInPasses: number[] = [];
    protected _queueFlag: RenderGraphValue = RenderGraphValue.Raster;
    constructor (type: ResourceDimension, index: number) {
        this._type = type;
        this._idx = index;
    }
    set graphQueueFlag (value: RenderGraphValue) {
        this._queueFlag = value;
    }
    get graphQueueFlag () : RenderGraphValue { return this._queueFlag; }
    get index (): number { return this._idx; }
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
    ResourceDimension.TEXTURE3D, index: number) {
        super(type, index);
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
    constructor (index: number) {
        super(ResourceDimension.BUFFER, index);
    }
}

export class RenderPass {
    protected _name = '';
    protected _graphBuild: RenderGraphBuilder | null = null;
    protected _idx = -1;
    protected _queueFlag: RenderGraphValue = RenderGraphValue.Raster;
    protected _depthStencilInput: RenderTextureResource | null = null;
    protected _depthStencilOutput: RenderTextureResource | null = null;
    protected _colorOutputs: RenderTextureResource[] = [];
    protected _colorInputs: RenderTextureResource[] = [];
    protected _blitTextureOutputs: RenderTextureResource[] = [];
    protected _blitTextureInputs: RenderTextureResource[] = [];
    protected _storageTextureInputs: RenderBufferResource[] = [];
    protected _storageTextureOutputs: RenderBufferResource[] = [];
    constructor (graph: RenderGraphBuilder, index: number, queueFlag: RenderGraphValue = RenderGraphValue.Raster) {
        this._graphBuild = graph;
        this._idx = index;
        this._queueFlag = queueFlag;
    }
    set name (value: string) {
        this._name = value;
    }
    get name () { return this._name; }
    set graphQueueFlag (value: RenderGraphValue) {
        this._queueFlag = value;
    }
    get graphQueueFlag () : RenderGraphValue { return this._queueFlag; }
    get graph (): RenderGraphBuilder { return this._graphBuild!; }
    get index () { return this._idx; }
    get colorInputs () { return this._colorInputs; }
    get colorOutputs () { return this._colorOutputs; }
    get depthStencilOutput () { return this._depthStencilOutput; }
    get depthStencilInput () { return this._depthStencilInput; }

    setDepthStencilInput (name: string) {}
    setDepthStencilOutput (name: string, info: ResourceDesc) {}
    addColorOutput (name: string, info: ResourceDesc, input = '') {}
    addTextureInput (name: string, info: ResourceDesc) {}
    addBlitTextureReadOnlyInput (name: string) {}
    addBlitTextureOutput (name: string, info: ResourceDesc, input = '') {}
    addStorageTextureOutput (name: string, info: ResourceDesc, input = '') {}
}

export class RenderGraphBuilder {
    protected _renderGraph: RenderGraph | null = null;
    protected _resourceGraph: ResourceGraph | null = null;
    protected _layoutGraph: LayoutGraphData | null = null;
    protected _passes: RenderPass[] = [];
    protected _resources: RenderResource[] = [];
    protected _passToIdx: number[] = [];
    protected _resourceToIdx: number[] = [];
    constructor (renderGraph: RenderGraph, resourceGraph: ResourceGraph, layoutGraph: LayoutGraphData) {
        this._renderGraph = renderGraph;
        this._resourceGraph = resourceGraph;
        this._layoutGraph = layoutGraph;
    }
    protected buildIO () {}
    build () { }
    addPass (name: string, queueFlag: RenderGraphValue = RenderGraphValue.Raster) {

    }
    findPass (name: string) {}
    getTextureResource (name: string) {}
}
