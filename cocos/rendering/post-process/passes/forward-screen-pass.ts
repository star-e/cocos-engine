import { Camera } from '../../../render-scene/scene';
import { buildForwardPass } from '../../custom/define';
import { Pipeline } from '../../custom/pipeline';
import { BasePass } from './base-pass';

export class ForwardScreenPass extends BasePass {
    name: string = 'forward-screen-pass';
    render (camera: Camera, ppl: Pipeline): void {
        buildForwardPass(camera, ppl, false, true);
    }
}
