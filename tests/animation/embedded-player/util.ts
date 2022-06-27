import { Node } from "../../../cocos/core";
import { addEmbeddedPlayerTag, AnimationClip } from "../../../cocos/core/animation/animation-clip";
import { EmbeddedPlayableState } from "../../../cocos/core/animation/embedded-player/embedded-player";
import { EmbeddedPlayer } from "../../../editor/exports/embedded-player";

export class EmbeddedPlayerHostMock {
    constructor(root: Node, private _embeddedPlayer: EmbeddedPlayer, _hostDuration: number) {
        const instantiatedPlayer = this._embeddedPlayer.playable.instantiate(root);
        this._instantiatedPlayer = instantiatedPlayer;
    }

    get wellInstantiated() {
        return !!this._instantiatedPlayer;
    }

    get randomAccessible() {
        return this._instantiatedPlayer.randomAccess;
    }

    public play(time: number) {
        this._instantiatedPlayer.play(time);
    }

    public stop() {
        this._instantiatedPlayer.stop();
    }

    public setSpeed(speed: number) {
        this._instantiatedPlayer.setSpeed(speed);
    }

    private _instantiatedPlayer: EmbeddedPlayableState | null;
}

export class AnimationClipHostEmbeddedPlayerMock {
    constructor(_root: Node, embeddedPlayer: EmbeddedPlayer, hostDuration: number) {
        const animationClip = new AnimationClip();
        animationClip.duration = hostDuration;
        animationClip[addEmbeddedPlayerTag](embeddedPlayer);
        const evaluator = animationClip.createEvaluator({ target: _root });
        this._animationEvaluator = evaluator;
    }

    public play(time: number) {
        this._animationEvaluator.notifyHostPlay(time);
    }

    public pause(time: number) {
        this._animationEvaluator.notifyHostPause(time);
    }

    public stop() {
        this._animationEvaluator.notifyHostStop();
    }

    public setSpeed(speed: number) {
        this._animationEvaluator.notifyHostSpeedChanged(speed);
    }

    public evaluateAt(time: number) {
        this._animationEvaluator.evaluate(time);
    }

    private _animationEvaluator: ReturnType<AnimationClip['createEvaluator']>;
}