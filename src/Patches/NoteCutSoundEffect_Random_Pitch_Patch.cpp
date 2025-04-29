#include "main.hpp" // Include main.hpp to get logger context if needed
#include "ModConfig.hpp"
#include "include/Patches/NoteCutSoundEffect_Random_Pitch_Patch.hpp"
#include "include/Patches/Chain_Element_Volume_Multiplier_Patch.hpp" // Include header for chain volume logic
#include "include/Patches/NoteCutSoundEffect_Misc_Patch.hpp"         // Include header for misc logic

// Include necessary headers for the hook and reimplementation
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "GlobalNamespace/NoteCutSoundEffect.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/NoteController.hpp"

#include "UnityEngine/Random.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AnimationCurve.hpp"
#include "UnityEngine/Vector3.hpp"

using namespace GlobalNamespace;
using namespace UnityEngine;

// Define the hook logic within the patch namespace
namespace HitsoundTweaqs::Patches::NoteCutSoundEffect_Random_Pitch
{

    // Function to apply the patch logic (simulating Prefix/Transpiler effects)
    // Modifies pitch and aheadTime directly based on config
    void ApplyPatchLogic(GlobalNamespace::NoteCutSoundEffect *self, float &pitch_ref, float &aheadTime_ref)
    {
        // Calculate config-based pitch (replaces original Random.Range)
        float randomPitchMin = getModConfig().RandomPitchMin.GetValue();
        float randomPitchMax = getModConfig().RandomPitchMax.GetValue();
        pitch_ref = UnityEngine::Random::Range(randomPitchMin, randomPitchMax);

        // Calculate the modified input aheadTime based on patch latency fix
        bool useSpatialization = getModConfig().EnableSpatialization.GetValue();
        const float hitsoundAlignOffset = 0.185f;
        // Use the original aheadTime value coming into the function for latency calculation
        float sfxLatency = useSpatialization ? (aheadTime_ref - hitsoundAlignOffset) : 0.0f;
        // Update aheadTime_ref to the modified value needed for the internal _aheadTime calculation
        aheadTime_ref = (hitsoundAlignOffset / pitch_ref + sfxLatency) * pitch_ref;
    }

    // Strict Reimplementation of NoteCutSoundEffect::Init incorporating patch logic via function calls
    MAKE_HOOK_MATCH(Hook_NoteCutSoundEffect_Init_RandomPitch,
                    &GlobalNamespace::NoteCutSoundEffect::Init,
                    void, GlobalNamespace::NoteCutSoundEffect *self,
                    UnityEngine::AudioClip *audioClip,
                    GlobalNamespace::NoteController *noteController,
                    double noteDSPTime,
                    float aheadTime, // Original aheadTime parameter
                    float missedTimeOffset,
                    float timeToPrevNote,
                    float timeToNextNote,
                    GlobalNamespace::Saber *saber,
                    bool handleWrongSaberTypeAsGood,
                    float volumeMultiplier, // Original volumeMultiplier parameter
                    bool ignoreSaberSpeed,
                    bool ignoreBadCuts)
    {
        // SQLogger.debug("NoteCutSoundEffect::Init (Strict Reimplementation with Patches) Hook Enter");

        // --- Apply Patch Logic (Simulating Prefixes) ---
        float currentPitch = 0.0f;
        float currentAheadTime = aheadTime;
        float currentVolumeMultiplier = volumeMultiplier;
        bool currentIgnoreSaberSpeed = ignoreSaberSpeed; // Start with original param

        // Apply Random Pitch / aheadTime fix
        ApplyPatchLogic(self, /* out */ currentPitch, /* in/out */ currentAheadTime);

        // Apply Chain Element Volume Multiplier fix
        HitsoundTweaqs::Patches::Chain_Element_Volume_Multiplier::ApplyPatchLogic(self, /* in/out */ currentVolumeMultiplier, noteController);

        // Apply Misc Init Patch (IgnoreSaberSpeed override, Spatialization set)
        // Needs the audioSource, get it early if needed by patches
        UnityEngine::AudioSource *audioSource = self->_audioSource; // Assume direct access ok
        HitsoundTweaqs::Patches::NoteCutSoundEffect_Misc::ApplyInitPatchLogic(self, /* in/out */ currentIgnoreSaberSpeed, audioSource);

        // --- End Apply Patch Logic ---

        // --- Start Strict Reimplementation ---
        self->_pitch = currentPitch;
        self->_ignoreSaberSpeed = currentIgnoreSaberSpeed; // Use potentially modified value
        self->_ignoreBadCuts = ignoreBadCuts;
        self->_beforeCutVolume = 0.0f;
        self->_volumeMultiplier = currentVolumeMultiplier;
        self->set_enabled(true);

        self->_audioSource->set_clip(audioClip);

        self->_noteMissedTimeOffset = missedTimeOffset;

        self->_aheadTime = currentAheadTime / self->_pitch;

        self->_timeToNextNote = timeToNextNote;
        self->_timeToPrevNote = timeToPrevNote;
        self->_saber = saber;
        self->_noteController = noteController;
        self->_handleWrongSaberTypeAsGood = handleWrongSaberTypeAsGood;
        self->_noteWasCut = false;

        // Volume calculation (User modified) - Uses _ignoreSaberSpeed and saber->bladeSpeedForLogic
        // Note: Original C# didn't seem to use the volumeMultiplier parameter in the final volume calc, just stored it.
        if (self->_ignoreSaberSpeed)
        {
            self->_beforeCutVolume = self->_goodCutVolume;        // VERIFY: goodCutVolume field exists
            self->_audioSource->set_volume(self->_goodCutVolume); // VERIFY: goodCutVolume field exists
        }
        else
        {
            self->_beforeCutVolume = 0.0f;
            self->_audioSource->set_volume(self->_speedToVolumeCurve->Evaluate(saber->bladeSpeedForLogic));
            // VERIFY: _speedToVolumeCurve field (type AnimationCurve*) exists
            // VERIFY: saber->bladeSpeedForLogic method/field exists
        }

        self->_audioSource->set_pitch(self->_pitch);
        self->_audioSource->set_priority(128);

        // Position (Modified by Transpiler Patch)
        UnityEngine::Transform *transform = self->get_transform();
        if (transform)
        {
            // SQLogger.debug("Applying Init Transpiler: Setting initial position to Zero");
            transform->set_position(UnityEngine::Vector3::get_zero());
        }
        else
        {
            SQLogger.warn("Transform is null, cannot set initial position.");
        }

        // Compute DSP Times
        self->ComputeDSPTimes(noteDSPTime, self->_aheadTime, timeToPrevNote, timeToNextNote);
        // VERIFY: ComputeDSPTimes signature match
        // VERIFY: _aheadTime field exists

        // Postfix logic from Position Init Patch ...
        // SQLogger.debug("Applying Init Postfix: Checking conditions to set position");
        bool staticPos = getModConfig().StaticSoundPos.GetValue();
        bool isSpat = self->_audioSource->get_spatialize(); // VERIFY: get_spatialize method/field
        if (!staticPos && isSpat && transform && saber)
        {
            // SQLogger.debug("Init Postfix: Not static, spatialized. Setting position to saber tip");
            // VERIFY: saber->saberBladeTopPos exists
            transform->set_position(saber->saberBladeTopPos);
        }
        else
        {
            // SQLogger.debug("Init Postfix: Condition not met (static=%d, spatialize=%d), position remains Zero.", staticPos, isSpat);
        }
        // --- End Apply Patch Postfix Logic ---

        // SQLogger.debug("NoteCutSoundEffect::Init Hook Exit");

        // DO NOT CALL ORIGINAL METHOD
    }

    // Installation function definition remains the same
    void InstallHook()
    {
        INSTALL_HOOK(SQLogger, Hook_NoteCutSoundEffect_Init_RandomPitch);
    }

} // namespace HitsoundTweaqs::Patches::NoteCutSoundEffect_Random_Pitch