#include "main.hpp"
#include "ModConfig.hpp"
#include "include/Patches/Early_Note_Spawn_Fix.hpp"

// Include necessary headers
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "GlobalNamespace/NoteCutSoundEffectManager.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/ColorType.hpp"
#include "GlobalNamespace/NoteCutSoundEffect.hpp"
#include "GlobalNamespace/SaberManager.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/BeatmapObjectManager.hpp"
#include "GlobalNamespace/BeatmapCallbacksController.hpp" // Assuming InitData source
#include "GlobalNamespace/PlayerSpecificSettings.hpp"     // Assuming InitData source
#include "GlobalNamespace/RandomObjectPicker_1.hpp"
#include "GlobalNamespace/MemoryPoolContainer_1.hpp"
#include "GlobalNamespace/LazyCopyHashSet_1.hpp"
#include "System/Collections/Generic/List_1.hpp"

#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Mathf.hpp"

#include <vector>
#include <cmath>  // For std::abs
#include <limits> // For numeric_limits

using namespace GlobalNamespace;
using namespace UnityEngine;

// Define the hook logic within the patch namespace
namespace HitsoundTweaqs::Patches::Early_Note_Spawn_Fix
{

    // Storage for queued notes
    // Use static vector within the namespace to mimic class member in C#
    static std::vector<NoteController *> initQueue;

    // Hook for NoteCutSoundEffectManager.HandleNoteWasSpawned
    // NOW a full reimplementation incorporating Early Spawn Fix and Proximity Check Patch logic
    MAKE_HOOK_MATCH(Hook_HandleNoteWasSpawned, &NoteCutSoundEffectManager::HandleNoteWasSpawned,
                    void, NoteCutSoundEffectManager *self, NoteController *noteController)
    {
        // --- Early Spawn Fix Logic --- (Prefix part)
        AudioTimeSyncController *audioTimeSyncController = self->_audioTimeSyncController;
        if (!audioTimeSyncController)
        {
            return; // Skip processing this note entirely
        }
        if (audioTimeSyncController->state != AudioTimeSyncController::State::Playing)
        {
            initQueue.push_back(noteController);
            return; // Queue and prevent rest of logic
        }

        // --- Start Strict Reimplementation (Following C# structure + Proximity Patch modifications) ---
        NoteData *noteData = noteController->noteData;
        if (!noteData)
        {
            return;
        }

        // Original: if (!this.IsSupportedNote(noteData)) return;
        if (!self->IsSupportedNote(noteData))
        {
            // We hooked IsSupportedNote separately for chain elements
            return;
        }

        // Original: float volumeMultiplier = noteData.cutSfxVolumeMultiplier;
        float volumeMultiplier = noteData->cutSfxVolumeMultiplier; // VERIFY: field exists
        // Original: if ((double) volumeMultiplier <= 0.0) return;
        if (volumeMultiplier <= 0.0f)
        {
            return;
        }

        // Original: float timeScale = this._audioTimeSyncController.timeScale;
        float timeScale = audioTimeSyncController->timeScale;

        // --- Proximity Check Patch Modification --- (Replaces original time check)
        // Original: if (noteData.colorType == ColorType.ColorA && (double) noteData.time < (double) this._prevNoteATime + 1.0 / 1000.0 || ...)
        // Patched: if ((noteData.colorType == ColorType.ColorA && Mathf.Abs(noteData.time - ____prevNoteATime) < 0.001f) || ...)
        if ((noteData->colorType == ColorType::ColorA && std::abs(noteData->time - self->_prevNoteATime) < 0.001f) ||
            (noteData->colorType == ColorType::ColorB && std::abs(noteData->time - self->_prevNoteBTime) < 0.001f))
        {
            // VERIFY: _prevNoteATime, _prevNoteBTime fields exist
            return;
        }
        // --- End Proximity Check Patch Modification ---

        // --- Previous Note Volume Reduction Patch Modification ---
        // Original logic was NOPed out by transpiler and reimplemented in prefix/postfix
        // Reimplementing the *patched* logic here directly:
        bool reduceOwnVolumeLater = false;
        if (std::abs(noteData->time - self->_prevNoteATime) < 0.001f || std::abs(noteData->time - self->_prevNoteBTime) < 0.001f)
        {
            if (noteData->colorType == ColorType::ColorA && self->_prevNoteBSoundEffect && self->_prevNoteBSoundEffect->enabled)
            {
                // VERIFY: _prevNoteBSoundEffect field (NoteCutSoundEffect*) exists
                self->_prevNoteBSoundEffect->_volumeMultiplier *= 0.9f; // Patch uses *=, not =
                // VERIFY: _volumeMultiplier field access on NoteCutSoundEffect
                reduceOwnVolumeLater = true;
            }
            else if (noteData->colorType == ColorType::ColorB && self->_prevNoteASoundEffect && self->_prevNoteASoundEffect->enabled)
            {
                // VERIFY: _prevNoteASoundEffect field (NoteCutSoundEffect*) exists
                self->_prevNoteASoundEffect->_volumeMultiplier *= 0.9f;
                reduceOwnVolumeLater = true;
            }
        }
        // --- End Previous Note Volume Reduction Patch Modification ---

        // Spawn sound effect
        // Original: NoteCutSoundEffect noteCutSoundEffect1 = this._noteCutSoundEffectPoolContainer.Spawn();
        NoteCutSoundEffect *newSoundEffect = self->_noteCutSoundEffectPoolContainer->Spawn();
        // VERIFY: _noteCutSoundEffectPoolContainer field (ObjectPool_1<NoteCutSoundEffect*>*) exists & Spawn() method
        if (!newSoundEffect)
        {
            return;
        }

        // Original: noteCutSoundEffect1.transform.SetPositionAndRotation(this.transform.localPosition, Quaternion.identity);
        newSoundEffect->get_transform()->SetPositionAndRotation(self->get_transform()->get_localPosition(), Quaternion::get_identity());

        // Original: noteCutSoundEffect1.didFinishEvent.Add((INoteCutSoundEffectDidFinishEvent) this);
        // Needs event handling translation - Assuming AddListener pattern or similar
        newSoundEffect->_didFinishEvent->Add((GlobalNamespace::INoteCutSoundEffectDidFinishEvent *)self);

        // Assign saber and update previous note data
        Saber *saber = nullptr;
        if (noteData->colorType == ColorType::ColorA)
        {
            self->_prevNoteATime = noteData->time;
            saber = self->_saberManager->leftSaber; // VERIFY: _saberManager field & leftSaber property/field
            self->_prevNoteASoundEffect = newSoundEffect;
        }
        else if (noteData->colorType == ColorType::ColorB)
        {
            self->_prevNoteBTime = noteData->time;
            saber = self->_saberManager->rightSaber; // VERIFY: _saberManager field & rightSaber property/field
            self->_prevNoteBSoundEffect = newSoundEffect;
        }

        // Determine audio clip and base volume multiplier
        // Original: bool flag2 = (double) noteData.timeToPrevColorNote < (double) this._beatAlignOffset;
        bool useShortSound = noteData->timeToPrevColorNote < self->_beatAlignOffset;
        // VERIFY: timeToPrevColorNote field on NoteData, _beatAlignOffset field on self

        // Original: AudioClip audioClip = flag2 ? this._randomShortCutSoundPicker.PickRandomObject() : this._randomLongCutSoundPicker.PickRandomObject();
        AudioClip *audioClip = useShortSound ? self->_randomShortCutSoundPicker->PickRandomObject() : self->_randomLongCutSoundPicker->PickRandomObject();
        // VERIFY: _randomShortCutSoundPicker, _randomLongCutSoundPicker fields (RandomObjectPicker_1<AudioClip*>*) exist & PickRandomObject()

        // Base volume calculation
        float baseVolumeMultiplier = 1.0f;
        // Logic from original code for flag1 / flag2 based volume adjustment:
        // Original: if (flag1) num1 = 0.9f; else if (flag2) num1 = 0.9f;
        // flag1 corresponds to our reduceOwnVolumeLater. flag2 corresponds to useShortSound.
        if (reduceOwnVolumeLater)
        {
            baseVolumeMultiplier = 0.9f;
        }
        else if (useShortSound)
        {
            // This part is slightly different in the patch prefix/postfix:
            // The patch calculates __state based on reduceVolume && !flag2.
            // If __state is true, the Postfix applies *= 0.9f to the new sound.
            // So, if we reduced the *previous* volume (reduceOwnVolumeLater=true) AND it was a *long* sound (useShortSound=false),
            // then we should apply the reduction here to the *new* sound effect.
            if (!useShortSound)
            { // Check if the condition from the patch postfix applies
                baseVolumeMultiplier = 0.9f;
            }
            // If it's a short sound but didn't overlap opposite color, original code also used 0.9f
            else
            {
                baseVolumeMultiplier = 0.9f;
            }
        } // If long sound and no overlap, base is 1.0f

        // Get InitData values (assuming stored on self or accessible)
        // bool handleWrongCuts = self->handleWrongSaberTypeAsGood; // From original C#, might be field on self or InitData
        // bool ignoreBadCuts = self->_initData.ignoreBadCuts; // Assuming _initData exists
        bool handleWrongCuts = self->handleWrongSaberTypeAsGood;                       // Assuming field exists based on C# usage
        bool ignoreBadCuts = self->_initData ? self->_initData->ignoreBadCuts : false; // VERIFY: _initData field & ignoreBadCuts
        bool useTestClips = self->_useTestAudioClips;                                  // VERIFY: _useTestAudioClips field exists

        // Call Init on the new sound effect
        newSoundEffect->Init(audioClip,
                             noteController,
                             noteData->time / timeScale + audioTimeSyncController->dspTimeOffset + audioTimeSyncController->songTimeOffset,
                             self->_beatAlignOffset, // aheadTime
                             0.15f,                  // missedTimeOffset
                             noteData->timeToPrevColorNote / timeScale,
                             noteData->timeToNextColorNote / timeScale,
                             saber,
                             handleWrongCuts,
                             baseVolumeMultiplier * volumeMultiplier, // Combined volume
                             useTestClips,                            // ignoreSaberSpeed param in Init - C# seems to map _useTestAudioClips here?
                             ignoreBadCuts);
        // VERIFY: Mapping of useTestAudioClips to ignoreSaberSpeed param in Init! C# trace is confusing.

        // --- Max Active Sound Effects Check (REMOVED by Patch) ---
        /* // Original logic removed by NoteCutSoundEffectManager_Max_Active_SoundEffects_Patch transpiler
        auto activeItems = self->_noteCutSoundEffectPoolContainer->get_activeItems();
        int maxSoundEffects = 64;
        if (activeItems->get_Count() <= maxSoundEffects) {
            SQLogger.debug("HandleNoteWasSpawned Reimplementation Exit (Pool count ok)");
            return;
        }
        NoteCutSoundEffect* oldestSoundEffect = nullptr;
        float oldestTime = std::numeric_limits<float>::max();
        for (int i = 0; i < activeItems->get_Count(); ++i) {
            NoteCutSoundEffect* currentEffect = activeItems->get_Item(i);
            if (!currentEffect) continue;
            if (currentEffect->_startDSPTime < oldestTime) {
                oldestTime = currentEffect->_startDSPTime;
                oldestSoundEffect = currentEffect;
            }
        }
        if (oldestSoundEffect) {
            SQLogger.debug("Pool limit reached, stopping oldest sound effect.");
            oldestSoundEffect->StopPlayingAndFinish();
        } else {
            SQLogger.warning("Pool limit reached but failed to find oldest sound effect to stop.");
        }
        */
        // --- End Max Active Sound Effects Check ---
        // DO NOT CALL ORIGINAL METHOD
    }

    // Hook for NoteCutSoundEffectManager.LateUpdate (from Early Spawn Fix)
    MAKE_HOOK_MATCH(Hook_LateUpdate, &NoteCutSoundEffectManager::LateUpdate,
                    void, NoteCutSoundEffectManager *self)
    {
        // Original LateUpdate logic first
        Hook_LateUpdate(self);

        // --- Early Spawn Fix Logic --- (Postfix part)
        AudioTimeSyncController *audioTimeSyncController = self->_audioTimeSyncController;
        if (!audioTimeSyncController)
        {
            // Error logged previously if null during HandleNoteWasSpawned
            return;
        }
        if (audioTimeSyncController->state == AudioTimeSyncController::State::Playing && !initQueue.empty())
        {
            SQLogger.debug("Processing %zu queued NoteControllers in LateUpdate", initQueue.size());
            auto queueCopy = initQueue;
            initQueue.clear();
            for (NoteController *item : queueCopy)
            {
                if (item && item->m_CachedPtr.m_value)
                {
                    // Call the REIMPLEMENTED HandleNoteWasSpawned directly
                    // This ensures queued notes also get the proximity patch logic
                    self->HandleNoteWasSpawned(item);
                }
                else
                {
                    SQLogger.warn("Skipping null NoteController in LateUpdate queue processing");
                }
            }
        }
        // --- End Early Spawn Fix Logic ---
    }

    // Installation function definition
    void InstallHooks()
    {
        INSTALL_HOOK(SQLogger, Hook_HandleNoteWasSpawned);
        INSTALL_HOOK(SQLogger, Hook_LateUpdate);
    }

} // namespace HitsoundTweaqs::Patches::Early_Note_Spawn_Fix