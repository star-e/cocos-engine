import { ccclass, serializable } from 'cc.decorator';
import { RealCurve } from '../../core/curves';
import { CLASS_NAME_PREFIX_ANIM, createEvalSymbol } from '../define';
import { Channel, RealChannel, RuntimeBinding, Track } from './track';

/**
 * @en
 * A real array track animates a real array attribute of target(such as morph weights of mesh renderer).
 * Every element in the array is corresponding to a real channel.
 * @zh
 * 实数数组轨道描述目标上某个实数数组属性（例如网格渲染器的形变权重）的动画。
 * 数组中的每个元素都对应一条实数通道。
 */
@ccclass(`${CLASS_NAME_PREFIX_ANIM}RealArrayTrack`)
export class RealArrayTrack extends Track {
    /**
     * @en The number of elements in the array which this track produces.
     * If you increased the count, there will be new empty real channels appended.
     * Otherwise if you decreased the count, the last specified number channels would be removed.
     * @zh 此轨道产生的数组元素的数量。
     * 当你增加数量时，会增加新的空实数通道；当你减少数量时，最后几个指定数量的通道会被移除。
     */
    get elementCount () {
        return this._channels.length;
    }

    set elementCount (value) {
        const { _channels: channels } = this;
        const nChannels = channels.length;
        if (value < nChannels) {
            this._channels.splice(value);
        } else if (value > nChannels) {
            this._channels.push(
                ...Array.from({ length: value - nChannels },
                    () => new Channel<RealCurve>(new RealCurve())),
            );
        }
    }

    /**
     * @en The channels of the track.
     * @zh 返回此轨道的所有通道的数组。
     */
    public channels () {
        return this._channels;
    }

    /**
     * @internal
     */
    public [createEvalSymbol] () {
        return new RealArrayTrackEval(this._channels.map(({ curve }) => curve));
    }

    @serializable
    private _channels: RealChannel[] = [];
}

export class RealArrayTrackEval {
    constructor (
        private _curves: RealCurve[],
    ) {
        this._result = new Array(_curves.length).fill(0.0);
    }

    public evaluate (time: number, _runtimeBinding: RuntimeBinding) {
        const {
            _result: result,
        } = this;
        const nElements = result.length;
        for (let iElement = 0; iElement < nElements; ++iElement) {
            result[iElement] = this._curves[iElement].evaluate(time);
        }
        return this._result;
    }

    private declare _result: number[];
}
