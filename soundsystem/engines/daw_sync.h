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
    fx.distEnabled     = d->masterFx.distOn;
    fx.distDrive       = d->masterFx.distDrive;
    fx.distTone        = d->masterFx.distTone;
    fx.distMix         = d->masterFx.distMix;
    fx.crushEnabled    = d->masterFx.crushOn;
    fx.crushBits       = d->masterFx.crushBits;
    fx.crushRate       = d->masterFx.crushRate;
    fx.crushMix        = d->masterFx.crushMix;
    fx.chorusEnabled   = d->masterFx.chorusOn;
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
        setBusFilter(b, d->mixer.filterOn[b], d->mixer.filterCut[b],
                     d->mixer.filterRes[b], d->mixer.filterType[b]);
        setBusDistortion(b, d->mixer.distOn[b], d->mixer.distDrive[b],
                         d->mixer.distMix[b]);
        setBusEQ(b, d->mixer.eqOn[b], d->mixer.eqLowGain[b], d->mixer.eqHighGain[b]);
        setBusEQFreqs(b, d->mixer.eqLowFreq[b], d->mixer.eqHighFreq[b]);
        setBusChorus(b, d->mixer.chorusOn[b], d->mixer.chorusRate[b],
                     d->mixer.chorusDepth[b], d->mixer.chorusMix[b],
                     d->mixer.chorusDelay[b], d->mixer.chorusFB[b]);
        setBusPhaser(b, d->mixer.phaserOn[b], d->mixer.phaserRate[b],
                     d->mixer.phaserDepth[b], d->mixer.phaserMix[b],
                     d->mixer.phaserFB[b], d->mixer.phaserStages[b]);
        setBusComb(b, d->mixer.combOn[b], d->mixer.combFreq[b],
                   d->mixer.combFB[b], d->mixer.combMix[b], d->mixer.combDamping[b]);
        setBusDelay(b, d->mixer.delayOn[b], d->mixer.delayTime[b],
                    d->mixer.delayFB[b], d->mixer.delayMix[b]);
        setBusDelaySync(b, d->mixer.delaySync[b], d->mixer.delaySyncDiv[b]);
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

    // Rewind
    rewind.rewindTime    = d->tapeFx.rewindTime;
    rewind.curve         = d->tapeFx.rewindCurve;
    rewind.minSpeed      = d->tapeFx.rewindMinSpeed;
    rewind.vinylNoise    = d->tapeFx.rewindVinyl;
    rewind.wobble        = d->tapeFx.rewindWobble;
    rewind.filterSweep   = d->tapeFx.rewindFilter;
}

#endif // DAW_SYNC_H
