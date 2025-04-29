#include "main.hpp"
#include "ModConfig.hpp"
#include "include/Patches/NoteCutSoundEffect_NoteWasCut_Patch.hpp"

// Include necessary headers
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "GlobalNamespace/NoteCutSoundEffect.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp" // Needed? Maybe for AudioSettings.dspTime
#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Object.hpp"
#include "GlobalNamespace/RandomObjectPicker_1.hpp" // For badCutRandomSoundPicker

using namespace GlobalNamespace;
using namespace UnityEngine;

// Define the hook logic within the patch namespace
namespace HitsoundTweaqs::Patches::NoteCutSoundEffect_NoteWasCut
{

    // Hook and Reimplementation of NoteCutSoundEffect::NoteWasCut
    MAKE_HOOK_MATCH(Hook_NoteWasCut, &NoteCutSoundEffect::NoteWasCut,
                    void, NoteCutSoundEffect *self,
                    NoteController *noteController, ByRef<NoteCutInfo> noteCutInfo) // Use ByRef for NoteCutInfo based on C# 'in'
    {
        // --- Start Strict Reimplementation --- (Following C# structure)

        // Original: if ((Object) this._noteController != (Object) noteController) return;
        if (UnityEngine::Object::op_Inequality(self->_noteController, noteController))
        {
            // VERIFY: _noteController field access
            return;
        }

        self->_noteWasCut = true; // VERIFY: _noteWasCut field access

        // Evaluate cut result
        bool isBadCut = !self->_ignoreBadCuts && (!self->_handleWrongSaberTypeAsGood && !noteCutInfo->get_allIsOK() || self->_handleWrongSaberTypeAsGood && (!noteCutInfo->get_allExceptSaberTypeIsOK() || noteCutInfo->saberTypeOK));
        // VERIFY: _ignoreBadCuts, _handleWrongSaberTypeAsGood field access
        // VERIFY: NoteCutInfo methods get_allIsOK, get_allExceptSaberTypeIsOK, get_saberTypeOK

        if (isBadCut)
        {
            self->_audioSource->set_priority(16);
            // VERIFY: _badCutRandomSoundPicker field (type RandomObjectPicker_1<AudioClip*>*) exists
            UnityEngine::AudioClip *badClip = self->_badCutRandomSoundPicker->PickRandomObject();
            self->_audioSource->set_clip(badClip);
            self->_audioSource->set_time(0.0f);
            self->_audioSource->Play();
            self->_goodCut = false;                              // VERIFY: _goodCut field access
            self->_audioSource->set_volume(self->_badCutVolume); // VERIFY: _badCutVolume field access

            // Original: this._endDSPtime = AudioSettings.dspTime + (double) this._audioSource.clip.length / (double) this._pitch + 0.10000000149011612;
            double clipLength = badClip ? (double)badClip->get_length() : 0.0;
            self->_endDSPtime = UnityEngine::AudioSettings::get_dspTime() + clipLength / (double)self->_pitch + 0.10000000149011612;
            // VERIFY: _endDSPtime, _pitch field access
        }
        else
        {
            self->_audioSource->set_priority(24);
            self->_goodCut = true;                                // VERIFY: _goodCut field access
            self->_audioSource->set_volume(self->_goodCutVolume); // VERIFY: _goodCutVolume field access
        }

        // Original position setting line omitted here (simulating transpiler)
        // Original: this.transform.position = noteCutInfo.cutPoint;

        // --- Apply Patch Postfix Logic --- (Conditional Position Setting)
        bool goodCut = self->_goodCut; // Get the value determined above

        if (!goodCut)
        {
            // Bad cuts always use cut point
            self->_audioSource->set_spatialize(true);
            self->get_transform()->set_position(noteCutInfo->cutPoint); // VERIFY: get_cutPoint method/field
        }
        else
        {
            // Good cuts only if StaticSoundPos is false and spatialized
            bool staticPos = getModConfig().StaticSoundPos.GetValue();
            bool isSpat = self->_audioSource->get_spatialize(); // VERIFY: get_spatialize method/field
            if (!staticPos && isSpat)
            {
                self->get_transform()->set_position(noteCutInfo->cutPoint); // VERIFY: get_cutPoint method/field
            }
        }

        // DO NOT CALL ORIGINAL METHOD
    }

    // Installation function definition
    void InstallHook()
    {
        INSTALL_HOOK(SQLogger, Hook_NoteWasCut);
    }

} // namespace HitsoundTweaqs::Patches::NoteCutSoundEffect_NoteWasCut