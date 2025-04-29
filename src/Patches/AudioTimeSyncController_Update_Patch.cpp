#include "main.hpp"
#include "include/Patches/AudioTimeSyncController_Update_Patch.hpp"

// Include necessary headers
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/Mathf.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/AudioSettings.hpp"

#include <cmath> // For std::abs

using namespace GlobalNamespace;
using namespace UnityEngine;

// Define the hook logic within the patch namespace
namespace HitsoundTweaqs::Patches::AudioTimeSyncController_Update
{

    // State for the DSP Offset averaging patch
    static bool firstCorrectionDone = false;
    static int averageCount = 1;
    static double averageOffset = 0.0;

    // Hook and Reimplementation of AudioTimeSyncController::Update
    // Assuming protected Update becomes public or accessible via hook
    MAKE_HOOK_MATCH(Hook_Update, &AudioTimeSyncController::Update,
                    void, AudioTimeSyncController *self)
    {
        // --- Start Strict Reimplementation (Following C# structure) ---

        // Original: if (this._lastState != this._state)
        if (self->_lastState != self->_state)
        {
            // VERIFY: _lastState, _state fields
            // LogHelper not available, using ours
            SQLogger.info("AudioTimeSyncController: state=%d->%d songTime=%.3f", (int)self->_lastState, (int)self->_state, self->_songTime);
            self->_lastState = self->_state;
        }

        // Original: if (this._state == AudioTimeSyncController.State.Stopped) return;
        if (self->_state == AudioTimeSyncController::State::Stopped)
        {
            // --- Patch Postfix Logic (Reset state on stop) ---
            // Original: if (____state == ...) firstCorrectionDone = false;
            firstCorrectionDone = false; // Reset averaging state
                                         // --- End Patch Postfix Logic ---
            return;
        }

        // Original: float num1 = Time.deltaTime * this._timeScale;
        float deltaTimeScaled = Time::get_deltaTime() * self->_timeScale; // VERIFY: _timeScale field
        // Original: this._lastFrameDeltaSongTime = num1;
        self->_lastFrameDeltaSongTime = deltaTimeScaled; // VERIFY: _lastFrameDeltaSongTime field

        // Original: if (Time.captureFramerate != 0)
        if (Time::get_captureFramerate() != 0)
        {
            self->_songTime += deltaTimeScaled; // VERIFY: _songTime field

            AudioClip *clip = self->_audioSource->get_clip(); // VERIFY: _audioSource field
            if (clip)
            {
                if (!self->_audioSource->get_loop())
                {
                    self->_songTime = Mathf::Min(self->_songTime, clip->get_length() - 0.01f);
                    self->_audioSource->set_time(self->_songTime);
                }
                else
                {
                    self->_audioSource->set_time(fmodf(self->_songTime, clip->get_length()));
                }
            }
            else
            {
                SQLogger.warn("Audio clip is null in captureFramerate block");
            }
            // First DSP offset calculation - kept by patch
            // Original: this._dspTimeOffset = this._dspTimeProvider.dspTime - (double) this._songTime;
            // Assuming _dspTimeProvider.dspTime is equivalent to AudioSettings.get_dspTime()
            self->_dspTimeOffset = AudioSettings::get_dspTime() - (double)self->_songTime;
            // VERIFY: _dspTimeOffset field

            self->_isReady = true; // VERIFY: _isReady field
        }
        // Original: else if ((double) this.timeSinceStart < (double) this._audioStartTimeOffsetSinceStart)
        else if (self->timeSinceStart < self->_audioStartTimeOffsetSinceStart)
        {
            // VERIFY: timeSinceStart, _audioStartTimeOffsetSinceStart fields
            self->_songTime += deltaTimeScaled;
        }
        else
        {
            // Original: if (!this._audioStarted)
            if (!self->_audioStarted)
            { // VERIFY: _audioStarted field
                self->_audioStarted = true;
                self->_audioSource->Play();
            }

            AudioClip *clip = self->_audioSource->get_clip();
            bool isPlaying = self->_audioSource->get_isPlaying();

            // Original: if ((UnityEngine.Object) this._audioSource.clip == (UnityEngine.Object) null || !this._audioSource.isPlaying && !this._forceNoAudioSyncOrAudioSyncErrorFixing)
            if (UnityEngine::Object::op_Equality(clip, nullptr) || (!isPlaying && !self->_forceNoAudioSyncOrAudioSyncErrorFixing))
            {
                // VERIFY: _forceNoAudioSyncOrAudioSyncErrorFixing field
                if (self->_failReportCount < 5 && self->_state == AudioTimeSyncController::State::Playing)
                {
                    // VERIFY: _failReportCount field
                    self->_failReportCount++;
                    // Log error messages
                    if (!clip)
                    {
                        SQLogger.error("AudioTimeSyncController: audio clip is null DSP=%.3f time=%.3f isPlaying=%d songTime=%.3f", AudioSettings::get_dspTime(), Time::get_timeSinceLevelLoad(), isPlaying, self->_songTime);
                    }
                    else if (self->_songTime < clip->get_length() - 1.0f)
                    {
                        SQLogger.error("AudioTimeSyncController: audio should be playing DSP=%.3f time=%.3f isPlaying=%d songTime=%.3f audioTime=%.3f", AudioSettings::get_dspTime(), Time::get_timeSinceLevelLoad(), isPlaying, self->_songTime, self->_audioSource->get_time());
                    }
                }
                // Implicit return in original code if conditions met
            }
            else
            {
                // Original audio playback sync logic
                int timeSamples = self->_audioSource->get_timeSamples();
                float audioTime = self->_audioSource->get_time();
                float timeSinceAudioStart = self->timeSinceStart - self->_audioStartTimeOffsetSinceStart;

                if (self->_prevAudioSamplePos > timeSamples)
                {                               // VERIFY: _prevAudioSamplePos field
                    self->_playbackLoopIndex++; // VERIFY: _playbackLoopIndex field
                }

                if (self->_prevAudioSamplePos == timeSamples)
                {
                    self->_inBetweenDSPBufferingTimeEstimate += deltaTimeScaled; // VERIFY: _inBetweenDSPBufferingTimeEstimate field
                }
                else
                {
                    self->_inBetweenDSPBufferingTimeEstimate = 0.0f;
                }
                self->_prevAudioSamplePos = timeSamples;

                float calculatedAudioTime = audioTime + ((float)self->_playbackLoopIndex * clip->get_length() / self->_timeScale + self->_inBetweenDSPBufferingTimeEstimate);

                // --- DSP Offset Calculation (REMOVED by Patch Transpiler) ---
                /* // Original logic removed:
                this._dspTimeOffset = AudioSettings.dspTime - (double) calculatedAudioTime / (double) this._timeScale;
                */
                // --- End Removed DSP Offset Calculation ---

                // Audio sync error fixing logic
                if (!self->_forceNoAudioSyncOrAudioSyncErrorFixing)
                {
                    float timeDiff = std::abs(timeSinceAudioStart - calculatedAudioTime);
                    if ((timeDiff > self->_forcedSyncDeltaTime || self->_state == AudioTimeSyncController::State::Paused) && !self->forcedNoAudioSync)
                    {
                        // VERIFY: _forcedSyncDeltaTime, forcedNoAudioSync fields
                        self->_audioStartTimeOffsetSinceStart = self->timeSinceStart - calculatedAudioTime;
                        timeSinceAudioStart = calculatedAudioTime; // Update variable for song time calculation
                    }
                    else
                    {
                        if (self->_fixingAudioSyncError)
                        { // VERIFY: _fixingAudioSyncError field
                            if (timeDiff < self->_stopSyncDeltaTime)
                            { // VERIFY: _stopSyncDeltaTime field
                                self->_fixingAudioSyncError = false;
                            }
                        }
                        else if (timeDiff > self->_startSyncDeltaTime)
                        { // VERIFY: _startSyncDeltaTime field
                            self->_fixingAudioSyncError = true;
                        }
                        if (self->_fixingAudioSyncError)
                        {
                            self->_audioStartTimeOffsetSinceStart = Mathf::Lerp(self->_audioStartTimeOffsetSinceStart, self->timeSinceStart - calculatedAudioTime, deltaTimeScaled * self->_audioSyncLerpSpeed);
                            // VERIFY: _audioSyncLerpSpeed field
                        }
                    }
                }

                // Song time update
                // VERIFY: _songTimeOffset, _audioLatency fields
                float newSongTime = Mathf::Max(self->_songTime, timeSinceAudioStart - (self->_songTimeOffset + self->_audioLatency));
                self->_lastFrameDeltaSongTime = newSongTime - self->_songTime;
                self->_songTime = newSongTime;
                self->_isReady = true;
            }
        }
        // --- End Strict Reimplementation ---

        // --- Patch Postfix Logic (DSP Offset Averaging) ---
        const double maxDiscrepancy = 0.05;
        const double syncOffset = -0.0043; // Magic number from patch

        // Calculate target offset (only if audio is playing and clip exists)
        if (self->_state != AudioTimeSyncController::State::Stopped && self->_audioSource && self->_audioSource->get_clip() && self->_audioSource->get_clip()->get_frequency() > 0)
        {
            double audioTimeSamples = (double)self->_audioSource->get_timeSamples();
            double clipFrequency = (double)self->_audioSource->get_clip()->get_frequency();
            double currentAudioTime = audioTimeSamples / clipFrequency;

            double targetOffset = AudioSettings::get_dspTime() - (currentAudioTime / (double)self->_timeScale);

            if (!firstCorrectionDone || std::abs(averageOffset - targetOffset) > maxDiscrepancy)
            {
                SQLogger.debug("Resetting DSP average offset to target: %.6f", targetOffset);
                averageOffset = targetOffset;
                averageCount = 1;
                firstCorrectionDone = true;
                self->_dspTimeOffset = targetOffset; // Set directly on reset
            }
            else
            {
                // Lock in value after some time?
                if (averageCount < 10000)
                {
                    // Update cumulative average
                    double prevAverage = averageOffset;
                    averageOffset = (averageOffset * averageCount + targetOffset) / (averageCount + 1);
                    averageCount++;
                    SQLogger.debug("Updating DSP average offset: %.6f -> %.6f (count %d, target %.6f)", prevAverage, averageOffset, averageCount, targetOffset);

                    // Set dspTimeOffset to whichever targetOffset encountered (current or previous stored) is closest to the average
                    if (std::abs(targetOffset - (averageOffset + syncOffset)) < std::abs(self->_dspTimeOffset - (averageOffset + syncOffset)))
                    {
                        SQLogger.debug("Updating actual DSP offset to current target: %.6f", targetOffset);
                        self->_dspTimeOffset = targetOffset;
                    }
                    else
                    {
                        // Keep previous _dspTimeOffset as it's closer to average
                        SQLogger.debug("Keeping previous actual DSP offset: %.6f", self->_dspTimeOffset);
                    }
                }
                else
                {
                    SQLogger.debug("DSP average offset locked (count %d)", averageCount);
                }
            }
        }
        else if (self->_state != AudioTimeSyncController::State::Stopped)
        {
            SQLogger.warn("Cannot calculate target DSP offset - audio source/clip invalid?");
        }
        // --- End Patch Postfix Logic ---

        // DO NOT CALL ORIGINAL METHOD
    }

    // Installation function definition
    void InstallHook()
    {
        INSTALL_HOOK(SQLogger, Hook_Update);
    }

} // namespace HitsoundTweaqs::Patches::AudioTimeSyncController_Update