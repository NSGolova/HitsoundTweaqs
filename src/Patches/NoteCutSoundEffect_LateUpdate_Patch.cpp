#include "main.hpp"
#include "ModConfig.hpp"
#include "include/Patches/NoteCutSoundEffect_LateUpdate_Patch.hpp"

// Include necessary headers
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "GlobalNamespace/NoteCutSoundEffect.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Mathf.hpp"
#include "UnityEngine/AnimationCurve.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector3.hpp"

using namespace GlobalNamespace;
using namespace UnityEngine;

// Define the hook logic within the patch namespace
namespace HitsoundTweaqs::Patches::NoteCutSoundEffect_LateUpdate
{

    // Hook and Reimplementation of NoteCutSoundEffect::OnLateUpdate
    MAKE_HOOK_MATCH(Hook_OnLateUpdate, &NoteCutSoundEffect::OnLateUpdate,
                    void, NoteCutSoundEffect *self)
    {
        // Hitsound_Reliability_Patches.Prefix
        const double scheduleAheadTime = 0.05;
        // Use isPlaying field as state flag (as per C# patch comments)
        // VERIFY: isPlaying field exists and is suitable for this flag purpose
        if (!self->_isPlaying && self->_startDSPTime - AudioSettings::get_dspTime() < scheduleAheadTime)
        {
            self->_isPlaying = true; // Set flag to prevent rescheduling

            if (!self->_audioSource->get_isPlaying())
            {
                self->_audioSource->set_timeSamples(0);
                self->_audioSource->PlayScheduled(self->_startDSPTime);
            }
        }
        // --- End Reliability Patch: Scheduling Prefix ---

        // --- Start Strict Reimplementation --- (Following C# structure)
        double dspTime = AudioSettings::get_dspTime();

        // Original: if (dspTime - this._endDSPtime > 0.0) this.StopPlayingAndFinish();
        if (dspTime - self->_endDSPtime > 0.0)
        {
            // VERIFY: _endDSPtime field access
            self->StopPlayingAndFinish(); // VERIFY: StopPlayingAndFinish method exists
        }
        else if (!self->_noteWasCut)
        { // VERIFY: _noteWasCut field access

            if (self->_noteStartedDissolving)
            { // VERIFY: _noteStartedDissolving field access
                // Original: this._audioSource.volume = Mathf.MoveTowards(this._audioSource.volume, 0.0f, Time.deltaTime * 4f);
                float currentVolume = self->_audioSource->get_volume();
                float newVolume = Mathf::MoveTowards(currentVolume, 0.0f, Time::get_deltaTime() * 4.0f);
                self->_audioSource->set_volume(newVolume);
            }
            else
            {
                // Original priority setting line omitted here (simulating transpiler)
                // Hitsound_Reliability_Patches.LateUpdateTranspiler
                // original: if (dspTime > this._startDSPTime + (double) this._aheadTime - 0.05000000074505806)
                // this._audioSource.priority = 32;

                // Original position setting line omitted here (simulating transpiler)
                // NoteCutSoundEffect_Transform_Position_LateUpdate_Patch.Transpiler
                // Original: this.transform.position = this._saber.saberBladeTopPosForLogic;

                // Original volume calculation block
                float goodCutVolume = self->_goodCutVolume; // VERIFY: _goodCutVolume field access
                if (!self->_ignoreSaberSpeed)
                { // VERIFY: _ignoreSaberSpeed field access
                    float evaluatedSpeed = 0.0f;
                    if (self->_saber && self->_speedToVolumeCurve)
                    { // VERIFY: _saber, _speedToVolumeCurve fields
                        // VERIFY: _saber->bladeSpeedForLogic exists
                        // VERIFY: _speedToVolumeCurve (AnimationCurve*) exists
                        evaluatedSpeed = self->_speedToVolumeCurve->Evaluate(self->_saber->bladeSpeedForLogic);
                    }
                    else
                    {
                        SQLogger.warn("Saber or speedToVolumeCurve null in OnLateUpdate volume calc");
                    }
                    // VERIFY: _audioSource->get_time(), _pitch, _aheadTime, _noteMissedTimeOffset field access
                    float timeFactor = 1.0f - Mathf::Clamp01((self->_audioSource->get_time() / self->_pitch - self->_aheadTime) / self->_noteMissedTimeOffset);
                    goodCutVolume *= evaluatedSpeed * timeFactor;
                }
                // Original: this._beforeCutVolume = (double) goodCutVolume >= (double) this._beforeCutVolume ? goodCutVolume : Mathf.Lerp(this._beforeCutVolume, goodCutVolume, Time.deltaTime * 4f);
                // VERIFY: _beforeCutVolume field access
                float newBeforeCutVolume = (double)goodCutVolume >= (double)self->_beforeCutVolume ? goodCutVolume : Mathf::Lerp(self->_beforeCutVolume, goodCutVolume, Time::get_deltaTime() * 4.0f);
                self->_beforeCutVolume = newBeforeCutVolume;

                // Original: this._audioSource.volume = this._beforeCutVolume * this._volumeMultiplier;
                // VERIFY: _volumeMultiplier field access
                self->_audioSource->set_volume(self->_beforeCutVolume * self->_volumeMultiplier);
            }
        }
        else
        { // if (_noteWasCut)
            // Original: if (!this._goodCut) return;
            if (!self->_goodCut)
            { // VERIFY: _goodCut field access
              // return; // Original code has implicit return if inside else if, explicit return here
            }
            else
            {
                // Original: this._audioSource.volume = this._goodCutVolume * this._volumeMultiplier;
                self->_audioSource->set_volume(self->_goodCutVolume * self->_volumeMultiplier);
            }
        }

        // --- Apply Patch Postfix Logic --- (Conditional Position Setting)
        bool noteWasCut = self->_noteWasCut;
        bool staticPos = getModConfig().StaticSoundPos.GetValue();
        bool isSpat = self->_audioSource->get_spatialize(); // VERIFY: get_spatialize method/field
        Transform *transform = self->get_transform();

        if (!noteWasCut && !staticPos && isSpat && transform && self->_saber)
        {
            // VERIFY: _saber->saberBladeTopPos exists
            transform->set_position(self->_saber->saberBladeTopPos);
        }

        // --- Reliability Patch: Dynamic Priority Postfix ---
        double noteTime = self->_startDSPTime + (double)self->_aheadTime;
        // VERIFY: _startDSPTime, _aheadTime field access
        if (!(self->_noteWasCut && !self->_goodCut))
        { // If not a bad cut
            // VERIFY: _noteWasCut, _goodCut field access
            int newPriority = 0;
            if (dspTime - noteTime > 0)
            { // After note time
                const float priorityFalloff = 384.0f;
                newPriority = Mathf::Clamp(32 + Mathf::RoundToInt((float)(dspTime - noteTime) * priorityFalloff), 32, 127);
            }
            else
            { // Before note time
                const float priorityRampup = 192.0f;
                newPriority = Mathf::Clamp(32 + Mathf::RoundToInt((float)(noteTime - dspTime) * priorityRampup), 32, 128);
            }
            self->_audioSource->set_priority(newPriority);
        }
        else
        {
            // Restore default bad cut priority? Original sets it to 16 in NoteWasCut.
            // The patch C# doesn't explicitly handle priority for bad cuts here.
            // Let's assume the priority set in NoteWasCut (16) persists for bad cuts.
        }
        // --- End Reliability Patch: Dynamic Priority Postfix ---

        // DO NOT CALL ORIGINAL METHOD
    }

    // Installation function definition
    void InstallHook()
    {
        INSTALL_HOOK(SQLogger, Hook_OnLateUpdate);
    }

} // namespace HitsoundTweaqs::Patches::NoteCutSoundEffect_LateUpdate