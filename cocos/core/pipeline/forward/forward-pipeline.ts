/*
 Copyright (c) 2020 Xiamen Yaji Software Co., Ltd.

 https://www.cocos.com/

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
 */

/**
 * @packageDocumentation
 * @module pipeline
 */

import { ccclass, displayOrder, type, serializable } from 'cc.decorator';
import { DEV, EDITOR } from 'internal:constants';
import { assert } from '../../platform';
import { RenderPipeline, IRenderPipelineInfo } from '../render-pipeline';
import { ForwardFlow } from './forward-flow';
import { RenderTextureConfig } from '../pipeline-serialization';
import { ShadowFlow } from '../shadow/shadow-flow';
import { UBOGlobal, UBOShadow, UBOCamera, UNIFORM_SHADOWMAP_BINDING,
    UNIFORM_SPOT_LIGHTING_MAP_TEXTURE_BINDING, supportsR32FloatTexture } from '../define';
import { Swapchain, RenderPass, Format, LoadOp, StoreOp, ClearFlagBit, Color } from '../../gfx';
import { builtinResMgr } from '../../builtin';
import { Texture2D } from '../../assets/texture-2d';
import { Camera, DirectionalLight, Light, LightType, Shadows, ShadowType, SKYBOX_FLAG } from '../../renderer/scene';
import { errorID } from '../../platform/debug';
import { PipelineSceneData } from '../pipeline-scene-data';
import { PipelineEventType } from '../pipeline-event';
import { decideProfilerCamera } from '../pipeline-funcs';
import { sceneCulling, validPunctualLightsCulling } from '../scene-culling';
import { AccessType, AttachmentType, RasterView } from '../custom/render-graph';
import { WebPipeline } from '../custom/web-pipeline';
import { Setter } from '../custom/pipeline';
import { QueueHint, ResourceResidency } from '../custom/types';
import { DeviceResourceGraph } from '../custom/executor';
import { createCustomPipeline } from '../custom';

const PIPELINE_TYPE = 0;

function _setShadowCameraValues (queue: Setter, light: Readonly<Light>, shadows: Readonly<Shadows>) {
    queue.setMat4('cc_matLightPlaneProj', shadows.matLight);
    switch (light.type) {
    case LightType.DIRECTIONAL:
        // if (!shadows.fixedArea) {
        // } else {
        // }
        // queue.setMat4('cc_matLightView');
        // queue.setMat4('cc_matLightViewProj');
        // queue.setFloat4('cc_shadowInvProjDepthInfo');
        // queue.setFloat4('cc_shadowProjDepthInfo');
        // queue.setFloat4('cc_shadowProjInfo');
        // queue.setFloat4('cc_shadowNFLSInfo');
        // queue.setFloat4('cc_shadowWHPBInfo');
        // queue.setFloat4('cc_shadowLPNNInfo');
        // queue.setFloat4('cc_shadowColor');
        break;
    case LightType.SPOT:
        break;
    default:
        throw Error('Unsupported light type for shadow');
    }
}

/**
 * @en The forward render pipeline
 * @zh 前向渲染管线。
 */
@ccclass('ForwardPipeline')
export class ForwardPipeline extends RenderPipeline {
    @type([RenderTextureConfig])
    @serializable
    @displayOrder(2)
    protected renderTextures: RenderTextureConfig[] = [];

    protected _postRenderPass: RenderPass | null = null;

    public get postRenderPass (): RenderPass | null {
        return this._postRenderPass;
    }

    _renderAutomata!: WebPipeline;

    _shadowFlow!: ShadowFlow;
    _forwardFlow!: ForwardFlow;

    public initialize (info: IRenderPipelineInfo): boolean {
        super.initialize(info);
        return true;
    }

    public activate (swapchain: Swapchain): boolean {
        if (EDITOR) { console.info('Forward render pipeline initialized.'); }

        if (DEV) {
            assert(this._flows.length === 0, 'flows.length should be zero');
        }

        this._renderAutomata = new WebPipeline();

        this._shadowFlow = new ShadowFlow();
        this._shadowFlow.initialize(ShadowFlow.initInfo);
        this._flows.push(this._shadowFlow);

        this._forwardFlow = new ForwardFlow();
        this._forwardFlow.initialize(ForwardFlow.initInfo);
        this._flows.push(this._forwardFlow);

        this._macros = { CC_PIPELINE_TYPE: PIPELINE_TYPE };
        this._pipelineSceneData = new PipelineSceneData();

        if (!super.activate(swapchain)) {
            return false;
        }

        if (!this._activeRenderer(swapchain)) {
            errorID(2402);
            return false;
        }

        return true;
    }

    protected _buildShadowPass (automata: WebPipeline,
        light: Readonly<Light>,
        shadows: Readonly<Shadows>,
        passName: Readonly<string>,
        width: Readonly<number>,
        height: Readonly<number>,
        bCastShadow: Readonly<boolean>) {
        if (!automata.resourceGraph.contains(this._dsShadowMap)) {
            const format = supportsR32FloatTexture(this.device) ? Format.R32F : Format.RGBA8;
            automata.addRenderTexture(this._dsShadowMap, format, width, height);
        }
        const pass = automata.addRasterPass(width, height, '_', passName);
        pass.addRasterView(this._dsShadowMap, new RasterView('_',
            AccessType.WRITE, AttachmentType.DEPTH_STENCIL,
            LoadOp.CLEAR, StoreOp.STORE,
            ClearFlagBit.DEPTH_STENCIL,
            new Color(0, 0, 0, 0)));
        if (bCastShadow) {
            const queue = pass.addQueue(QueueHint.COUNT);
            queue.addScene(`${passName}_shadowScene`);
            _setShadowCameraValues(queue, light, shadows);
        }
    }

    protected _buildShadowPasses (automata: WebPipeline, validLights: Light[],
        mainLight: Readonly<DirectionalLight> | null,
        pplScene: Readonly<PipelineSceneData>,
        name: Readonly<string>): void {
        const shadows = pplScene.shadows;
        if (!shadows.enabled || shadows.type !== ShadowType.ShadowMap) {
            return;
        }
        const castShadowObjects = pplScene.castShadowObjects;
        const validPunctualLights = pplScene.validPunctualLights;

        // force clean up
        validLights.length = 0;

        // pick spot lights
        let numSpotLights = 0;
        for (let i = 0; numSpotLights < shadows.maxReceived && i < validPunctualLights.length; ++i) {
            const light = validPunctualLights[i];
            if (light.type === LightType.SPOT) {
                validLights.push(light);
                ++numSpotLights;
            }
        }

        // build shadow map
        const bCastShadow = castShadowObjects.length !== 0;
        const mapWidth = bCastShadow ? shadows.size.x : 1;
        const mapHeight = bCastShadow ? shadows.size.y : 1;

        // main light
        if (mainLight) {
            const passName = `${name}-MainLight`;
            this._buildShadowPass(automata, mainLight, shadows,
                passName, mapWidth, mapHeight, bCastShadow);
        }
        // spot lights
        for (let i = 0; i !== validLights.length; ++i) {
            const passName = `${name}-SpotLight${i.toString()}`;
            this._buildShadowPass(automata, validLights[i], shadows,
                passName, mapWidth, mapHeight, bCastShadow);
        }
    }

    protected _ensureEnoughSize (cameras: Camera[]) {
        let newWidth = this._width;
        let newHeight = this._height;
        for (let i = 0; i < cameras.length; ++i) {
            const window = cameras[i].window;
            newWidth = Math.max(window.width, newWidth);
            newHeight = Math.max(window.height, newHeight);
        }
        if (newWidth !== this._width || newHeight !== this._height) {
            this._width = newWidth;
            this._height = newHeight;
        }
    }

    private readonly _validLights: Light[] = [];
    private readonly _dsShadowMap = 'dsShadowMap';
    private readonly _dsForwardPassRT = 'dsForwardPassColor';
    private readonly _dsForwardPassDS = 'dsForwardPassDS';
    public render (cameras: Camera[]) {
        if (cameras.length === 0) {
            return;
        }
        // build graph
        const automata = this._renderAutomata;
        automata.beginFrame(this._pipelineSceneData);
        for (let i = 0; i < cameras.length; i++) {
            const camera = cameras[i];
            if (camera.scene) {
                this._buildShadowPasses(automata, this._validLights,
                    camera.scene.mainLight,
                    this._pipelineSceneData,
                    `Camera${i.toString()}`);

                const window = camera.window;
                const width = Math.floor(window.width);
                const height = Math.floor(window.height);
                if (!this._renderAutomata.resourceGraph.contains(this._dsForwardPassRT)) {
                    this._renderAutomata.addRenderTexture(this._dsForwardPassRT, Format.RGBA8, width, height, ResourceResidency.Backbuffer);
                    this._renderAutomata.addRenderTexture(this._dsForwardPassDS, Format.DEPTH_STENCIL, width, height);
                }
                const forwardPass = automata.addRasterPass(width, height, '_', `CameraForwardPass${i.toString()}`);
                if (this._renderAutomata.resourceGraph.contains(this._dsShadowMap)) {
                    forwardPass.addRasterView(this._dsShadowMap, new RasterView('_',
                        AccessType.READ, AttachmentType.RENDER_TARGET,
                        LoadOp.CLEAR, StoreOp.DISCARD,
                        ClearFlagBit.NONE,
                        new Color(0, 0, 0, 0)));
                }
                const passView = new RasterView('_',
                    AccessType.WRITE, AttachmentType.RENDER_TARGET,
                    LoadOp.CLEAR, StoreOp.STORE,
                    ClearFlagBit.NONE,
                    new Color(0, 0, 0, 0));
                if (!(camera.clearFlag & ClearFlagBit.COLOR)) {
                    if (camera.clearFlag & SKYBOX_FLAG) {
                        passView.loadOp = LoadOp.DISCARD;
                    } else {
                        passView.loadOp = LoadOp.LOAD;
                        passView.accessType = AccessType.READ_WRITE;
                    }
                } else {
                    passView.clearColor.x = camera.clearColor.x;
                    passView.clearColor.y = camera.clearColor.y;
                    passView.clearColor.z = camera.clearColor.z;
                    passView.clearColor.w = camera.clearColor.w;
                }
                const passDSView = new RasterView('_',
                    AccessType.WRITE, AttachmentType.DEPTH_STENCIL,
                    LoadOp.CLEAR, StoreOp.STORE,
                    ClearFlagBit.DEPTH_STENCIL,
                    new Color(0, 0, 0, 0));
                if ((camera.clearFlag & ClearFlagBit.DEPTH_STENCIL) !== ClearFlagBit.DEPTH_STENCIL) {
                    if (!(camera.clearFlag & ClearFlagBit.DEPTH)) passDSView.loadOp = LoadOp.LOAD;
                    if (!(camera.clearFlag & ClearFlagBit.STENCIL)) passDSView.loadOp = LoadOp.LOAD;
                }
                forwardPass.addRasterView(this._dsForwardPassRT, passView);
                forwardPass.addRasterView(this._dsForwardPassDS, passDSView);
                forwardPass
                    .addQueue(QueueHint.RENDER_OPAQUE)
                    .addSceneOfCamera(camera);
                forwardPass
                    .addQueue(QueueHint.RENDER_TRANSPARENT)
                    .addSceneOfCamera(camera);
                automata.addPresentPass(`CameraPresentPass${i.toString()}`, this._dsForwardPassRT);
            }
        }
        automata.endFrame();
        // execute
        this._commandBuffers[0].begin();
        this.emit(PipelineEventType.RENDER_FRAME_BEGIN, cameras);
        this._ensureEnoughSize(cameras);
        decideProfilerCamera(cameras);

        for (let i = 0; i < cameras.length; i++) {
            const camera = cameras[i];
            if (camera.scene) {
                this.emit(PipelineEventType.RENDER_CAMERA_BEGIN, camera);
                validPunctualLightsCulling(this, camera);
                sceneCulling(this, camera);
                this._pipelineUBO.updateGlobalUBO(camera.window);
                this._pipelineUBO.updateCameraUBO(camera);

                this._shadowFlow.render(camera);

                for (let j = 0, len = this._forwardFlow.stages.length; j < len; j++) {
                    if (this._forwardFlow.stages[j].enabled) {
                        this._forwardFlow.stages[j].render(camera);
                    }
                }

                this.emit(PipelineEventType.RENDER_CAMERA_END, camera);
            }
        }
        this.emit(PipelineEventType.RENDER_FRAME_END, cameras);
        this._commandBuffers[0].end();
        this._device.queue.submit(this._commandBuffers);
    }

    public destroy () {
        this._destroyUBOs();
        this._destroyQuadInputAssembler();
        const rpIter = this._renderPasses.values();
        let rpRes = rpIter.next();
        while (!rpRes.done) {
            rpRes.value.destroy();
            rpRes = rpIter.next();
        }

        this._commandBuffers.length = 0;

        return super.destroy();
    }

    private _activeRenderer (swapchain: Swapchain) {
        const device = this.device;

        this._commandBuffers.push(device.commandBuffer);

        const shadowMapSampler = this.globalDSManager.pointSampler;
        this._descriptorSet.bindSampler(UNIFORM_SHADOWMAP_BINDING, shadowMapSampler);
        this._descriptorSet.bindTexture(UNIFORM_SHADOWMAP_BINDING, builtinResMgr.get<Texture2D>('default-texture').getGFXTexture()!);
        this._descriptorSet.bindSampler(UNIFORM_SPOT_LIGHTING_MAP_TEXTURE_BINDING, shadowMapSampler);
        this._descriptorSet.bindTexture(UNIFORM_SPOT_LIGHTING_MAP_TEXTURE_BINDING, builtinResMgr.get<Texture2D>('default-texture').getGFXTexture()!);
        this._descriptorSet.update();

        return true;
    }

    private _destroyUBOs () {
        if (this._descriptorSet) {
            this._descriptorSet.getBuffer(UBOGlobal.BINDING).destroy();
            this._descriptorSet.getBuffer(UBOShadow.BINDING).destroy();
            this._descriptorSet.getBuffer(UBOCamera.BINDING).destroy();
            this._descriptorSet.getTexture(UNIFORM_SHADOWMAP_BINDING).destroy();
            this._descriptorSet.getTexture(UNIFORM_SPOT_LIGHTING_MAP_TEXTURE_BINDING).destroy();
        }
    }
}
