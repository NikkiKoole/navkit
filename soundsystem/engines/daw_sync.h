// daw_sync.h — Single source of truth for syncing DawState → engine contexts.
// Included by daw_audio.h, sample_chop.h, song_render.c, and any offline renderer.
//
// Requires: effects.h (fx.*, setBus*(), dubLoop.*, rewind.*),
//           synth.h (synthCtx), daw_state.h (DawState), sequencer.h (Pattern)

#ifndef DAW_SYNC_H
#define DAW_SYNC_H

// Sync DawState fields → global engine contexts (effects, synth, buses, tape, rewind).
// `pat` is the current pattern (for scale overrides). Pass NULL to skip overrides.
static void dawSyncEngineStateFromEx(const DawState *d, const Pattern *pat) {
    // Scale lock
    synthCtx->scaleLockEnabled = d->scaleLockEnabled;
    synthCtx->scaleRoot = d->scaleRoot;
    synthCtx->scaleType = d->scaleType;

    if (pat) {
        const PatternOverrides *ovr = &pat->overrides;
        if (ovr->flags & PAT_OVR_SCALE) {
            synthCtx->scaleLockEnabled = true;
            synthCtx->scaleRoot = ovr->ovrScaleRoot;
            synthCtx->scaleType = ovr->ovrScaleType;
        }
    }

    // Master FX
    fx.octaverEnabled  = d->masterFx.octaverOn;
    fx.octaverMix      = d->masterFx.octaverMix;
    fx.octaverSubLevel = d->masterFx.octaverSubLevel;
    fx.octaverTone     = d->masterFx.octaverTone;
    fx.tremoloEnabled  = d->masterFx.tremoloOn;
    fx.tremoloRate     = d->masterFx.tremoloRate;
    fx.tremoloDepth    = d->masterFx.tremoloDepth;
    fx.tremoloShape    = d->masterFx.tremoloShape;
    fx.leslieEnabled   = d->masterFx.leslieOn;
    fx.leslieSpeed     = d->masterFx.leslieSpeed;
    fx.leslieDrive     = d->masterFx.leslieDrive;
    fx.leslieBalance   = d->masterFx.leslieBalance;
    fx.leslieDoppler   = d->masterFx.leslieDoppler;
    fx.leslieMix       = d->masterFx.leslieMix;
    fx.wahEnabled      = d->masterFx.wahOn;
    fx.wahMode         = d->masterFx.wahMode;
    fx.wahRate         = d->masterFx.wahRate;
    fx.wahSensitivity  = d->masterFx.wahSensitivity;
    fx.wahFreqLow      = d->masterFx.wahFreqLow;
    fx.wahFreqHigh     = d->masterFx.wahFreqHigh;
    fx.wahResonance    = d->masterFx.wahResonance;
    fx.wahMix          = d->masterFx.wahMix;
    fx.distEnabled     = d->masterFx.distOn;
    fx.distDrive       = d->masterFx.distDrive;
    fx.distTone        = d->masterFx.distTone;
    fx.distMix         = d->masterFx.distMix;
    fx.distMode        = d->masterFx.distMode;
    fx.crushEnabled    = d->masterFx.crushOn;
    fx.crushBits       = d->masterFx.crushBits;
    fx.crushRate       = d->masterFx.crushRate;
    fx.crushMix        = d->masterFx.crushMix;
    fx.chorusEnabled   = d->masterFx.chorusOn;
    fx.chorusBBD       = d->masterFx.chorusBBD;
    fx.chorusRate      = d->masterFx.chorusRate;
    fx.chorusDepth     = d->masterFx.chorusDepth;
    fx.chorusMix       = d->masterFx.chorusMix;
    fx.flangerEnabled  = d->masterFx.flangerOn;
    fx.flangerRate     = d->masterFx.flangerRate;
    fx.flangerDepth    = d->masterFx.flangerDepth;
    fx.flangerFeedback = d->masterFx.flangerFeedback;
    fx.flangerMix      = d->masterFx.flangerMix;
    fx.phaserEnabled   = d->masterFx.phaserOn;
    fx.phaserRate      = d->masterFx.phaserRate;
    fx.phaserDepth     = d->masterFx.phaserDepth;
    fx.phaserMix       = d->masterFx.phaserMix;
    fx.phaserFeedback  = d->masterFx.phaserFeedback;
    fx.phaserStages    = d->masterFx.phaserStages;
    fx.ringModEnabled  = d->masterFx.ringModOn;
    fx.ringModFreq     = d->masterFx.ringModFreq;
    fx.ringModMix      = d->masterFx.ringModMix;
    fx.combEnabled     = d->masterFx.combOn;
    fx.combFreq        = d->masterFx.combFreq;
    fx.combFeedback    = d->masterFx.combFeedback;
    fx.combMix         = d->masterFx.combMix;
    fx.combDamping     = d->masterFx.combDamping;
    fx.tapeEnabled     = d->masterFx.tapeOn;
    fx.tapeSaturation  = d->masterFx.tapeSaturation;
    fx.tapeWow         = d->masterFx.tapeWow;
    fx.tapeFlutter     = d->masterFx.tapeFlutter;
    fx.tapeHiss        = d->masterFx.tapeHiss;
    fx.delayEnabled    = d->masterFx.delayOn;
    fx.delayTime       = d->masterFx.delayTime;
    fx.delayFeedback   = d->masterFx.delayFeedback;
    fx.delayTone       = d->masterFx.delayTone;
    fx.delayMix        = d->masterFx.delayMix;
    fx.reverbEnabled   = d->masterFx.reverbOn;
    fx.reverbFDN       = d->masterFx.reverbFDN;
    fx.reverbSize      = d->masterFx.reverbSize;
    fx.reverbDamping   = d->masterFx.reverbDamping;
    fx.reverbPreDelay  = d->masterFx.reverbPreDelay;
    fx.reverbMix       = d->masterFx.reverbMix;
    fx.reverbBass      = d->masterFx.reverbBass;

    // Master EQ
    fx.eqEnabled       = d->masterFx.eqOn;
    fx.eqLowGain       = d->masterFx.eqLowGain;
    fx.eqHighGain      = d->masterFx.eqHighGain;
    fx.eqLowFreq       = d->masterFx.eqLowFreq;
    fx.eqHighFreq      = d->masterFx.eqHighFreq;
    fx.subBassBoost    = d->masterFx.subBassBoost;

    // BPM (for beat-synced effects)
    fx.bpm              = d->transport.bpm;

    // Vinyl sim
    fx.vinylEnabled     = d->masterFx.vinylOn;
    fx.vinylCrackle     = d->masterFx.vinylCrackle;
    fx.vinylNoise       = d->masterFx.vinylNoise;
    fx.vinylWarp        = d->masterFx.vinylWarp;
    fx.vinylWarpRate    = d->masterFx.vinylWarpRate;
    fx.vinylToneLP      = d->masterFx.vinylTone;

    // Multiband
    fx.mbEnabled        = d->masterFx.mbOn;
    fx.mbLowCrossover   = d->masterFx.mbLowCross;
    fx.mbHighCrossover  = d->masterFx.mbHighCross;
    fx.mbLowGain        = d->masterFx.mbLowGain;
    fx.mbMidGain        = d->masterFx.mbMidGain;
    fx.mbHighGain       = d->masterFx.mbHighGain;
    fx.mbLowDrive       = d->masterFx.mbLowDrive;
    fx.mbMidDrive       = d->masterFx.mbMidDrive;
    fx.mbHighDrive      = d->masterFx.mbHighDrive;

    // Master Compressor
    fx.compEnabled     = d->masterFx.compOn;
    fx.compThreshold   = d->masterFx.compThreshold;
    fx.compRatio       = d->masterFx.compRatio;
    fx.compAttack      = d->masterFx.compAttack;
    fx.compRelease     = d->masterFx.compRelease;
    fx.compMakeup      = d->masterFx.compMakeup;
    fx.compKnee        = d->masterFx.compKnee;

    // Sidechain (audio-follower mode)
    fx.sidechainEnabled = d->sidechain.on;
    fx.sidechainSource  = d->sidechain.source;
    fx.sidechainTarget  = d->sidechain.target;
    fx.sidechainDepth   = d->sidechain.depth;
    fx.sidechainAttack  = d->sidechain.attack;
    fx.sidechainRelease = d->sidechain.release;
    fx.sidechainHPFreq  = d->sidechain.hpFreq;

    // Sidechain envelope (note-triggered mode)
    fx.scEnvEnabled = d->sidechain.envOn;
    fx.scEnvSource  = d->sidechain.envSource;
    fx.scEnvTarget  = d->sidechain.envTarget;
    fx.scEnvDepth   = d->sidechain.envDepth;
    fx.scEnvAttack  = d->sidechain.envAttack;
    fx.scEnvHold    = d->sidechain.envHold;
    fx.scEnvRelease = d->sidechain.envRelease;
    fx.scEnvCurve   = d->sidechain.envCurve;
    fx.scEnvHPFreq  = d->sidechain.envHPFreq;

    // Per-bus mixer params
    for (int b = 0; b < NUM_BUSES; b++) {
        setBusVolume(b, d->mixer.volume[b]);
        setBusPan(b, d->mixer.pan[b]);
        setBusMute(b, d->mixer.mute[b]);
        setBusSolo(b, d->mixer.solo[b]);
        setBusReverbSend(b, d->mixer.reverbSend[b]);
        setBusDelaySend(b, d->mixer.delaySend[b]);
        setBusOctaver(b, d->mixer.octaverOn[b], d->mixer.octaverMix[b],
                      d->mixer.octaverSubLevel[b], d->mixer.octaverTone[b]);
        setBusTremolo(b, d->mixer.tremoloOn[b], d->mixer.tremoloRate[b],
                      d->mixer.tremoloDepth[b], d->mixer.tremoloShape[b]);
        setBusLeslie(b, d->mixer.leslieOn[b], d->mixer.leslieSpeed[b],
                     d->mixer.leslieDrive[b], d->mixer.leslieBalance[b],
                     d->mixer.leslieDoppler[b], d->mixer.leslieMix[b]);
        setBusWah(b, d->mixer.wahOn[b], d->mixer.wahMode[b], d->mixer.wahRate[b],
                  d->mixer.wahSensitivity[b], d->mixer.wahFreqLow[b], d->mixer.wahFreqHigh[b],
                  d->mixer.wahResonance[b], d->mixer.wahMix[b]);
        setBusFilter(b, d->mixer.filterOn[b], d->mixer.filterCut[b],
                     d->mixer.filterRes[b], d->mixer.filterType[b]);
        setBusDistortion(b, d->mixer.distOn[b], d->mixer.distDrive[b],
                         d->mixer.distMix[b]);
        mixerCtx->bus[b].distMode = d->mixer.distMode[b];
        setBusEQ(b, d->mixer.eqOn[b], d->mixer.eqLowGain[b], d->mixer.eqHighGain[b]);
        setBusEQFreqs(b, d->mixer.eqLowFreq[b], d->mixer.eqHighFreq[b]);
        setBusChorus(b, d->mixer.chorusOn[b], d->mixer.chorusRate[b],
                     d->mixer.chorusDepth[b], d->mixer.chorusMix[b],
                     d->mixer.chorusDelay[b], d->mixer.chorusFB[b]);
        mixerCtx->bus[b].chorusBBD = d->mixer.chorusBBD[b];
        setBusPhaser(b, d->mixer.phaserOn[b], d->mixer.phaserRate[b],
                     d->mixer.phaserDepth[b], d->mixer.phaserMix[b],
                     d->mixer.phaserFB[b], d->mixer.phaserStages[b]);
        setBusComb(b, d->mixer.combOn[b], d->mixer.combFreq[b],
                   d->mixer.combFB[b], d->mixer.combMix[b], d->mixer.combDamping[b]);
        setBusWingie(b, d->mixer.wingieOn[b], d->mixer.wingieMode[b],
                     d->mixer.wingiePitch[b], d->mixer.wingiePitch2[b], d->mixer.wingiePitch3[b],
                     d->mixer.wingieDecay[b], d->mixer.wingieMix[b]);
        for (int _wk = 0; _wk < WINGIE_NUM_PARTIALS; _wk++)
            setBusWingiePartial(b, _wk, d->mixer.wingieCaveOn[b][_wk]);
        setBusPitchShift(b, d->mixer.pitchOn[b], d->mixer.pitchSemitones[b], d->mixer.pitchMix[b]);
        setBusRingMod(b, d->mixer.ringModOn[b], d->mixer.ringModFreq[b],
                      d->mixer.ringModMix[b]);
        setBusDelay(b, d->mixer.delayOn[b], d->mixer.delayTime[b],
                    d->mixer.delayFB[b], d->mixer.delayMix[b]);
        setBusDelaySync(b, d->mixer.delaySync[b], d->mixer.delaySyncDiv[b]);
        // Per-bus compressor
        mixerCtx->bus[b].compEnabled = d->mixer.compOn[b];
        mixerCtx->bus[b].compThreshold = d->mixer.compThreshold[b];
        mixerCtx->bus[b].compRatio = d->mixer.compRatio[b];
        mixerCtx->bus[b].compAttack = d->mixer.compAttack[b];
        mixerCtx->bus[b].compRelease = d->mixer.compRelease[b];
        mixerCtx->bus[b].compMakeup = d->mixer.compMakeup[b];
    }

    // Tape/dub loop
    dubLoop.enabled      = d->tapeFx.enabled;
    dubLoop.headTime[0]  = d->tapeFx.headTime;
    dubLoop.feedback     = d->tapeFx.feedback;
    dubLoop.mix          = d->tapeFx.mix;
    if (d->tapeFx.throwBus >= 0 && dubLoop.throwActive) {
        dubLoop.inputSource = DUB_INPUT_BUS_DRUM0 + d->tapeFx.throwBus;
    } else {
        dubLoop.inputSource = d->tapeFx.inputSource;
    }
    dubLoop.saturation   = d->tapeFx.saturation;
    dubLoop.toneHigh     = d->tapeFx.toneHigh;
    dubLoop.noise        = d->tapeFx.noise;
    dubLoop.degradeRate  = d->tapeFx.degradeRate;
    dubLoop.wow          = d->tapeFx.wow;
    dubLoop.flutter      = d->tapeFx.flutter;
    dubLoop.drift        = d->tapeFx.drift;
    dubLoop.speedTarget  = d->tapeFx.speedTarget;
    dubLoop.speedSlew    = d->tapeFx.speedSlew;
    dubLoop.preReverb    = d->tapeFx.preReverb;

    // Tape stop
    fx.tapeStopTime      = d->tapeFx.tapeStopTime;
    fx.tapeStopCurve     = d->tapeFx.tapeStopCurve;
    fx.tapeStopSpinBack  = d->tapeFx.tapeStopSpinBack;
    fx.tapeStopSpinTime  = d->tapeFx.tapeStopSpinTime;

    // Beat repeat
    fx.beatRepeatDiv     = d->tapeFx.beatRepeatDiv;
    // Note: fx.djfxLoopDiv synced below
    fx.beatRepeatDecay   = d->tapeFx.beatRepeatDecay;
    fx.beatRepeatPitch   = d->tapeFx.beatRepeatPitch;
    fx.beatRepeatMix     = d->tapeFx.beatRepeatMix;
    fx.beatRepeatGate    = d->tapeFx.beatRepeatGate;

    // DJFX looper
    fx.djfxLoopDiv       = d->tapeFx.djfxLoopDiv;

    // Master speed (state in EffectsContext, accessed via macro)
    halfSpeedActive      = d->masterSpeed;

    // Rewind
    rewind.rewindTime    = d->tapeFx.rewindTime;
    rewind.curve         = d->tapeFx.rewindCurve;
    rewind.minSpeed      = d->tapeFx.rewindMinSpeed;
    rewind.vinylNoise    = d->tapeFx.rewindVinyl;
    rewind.wobble        = d->tapeFx.rewindWobble;
    rewind.filterSweep   = d->tapeFx.rewindFilter;
}

#endif // DAW_SYNC_H
