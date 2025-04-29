#pragma once

#include "GlobalNamespace/NoteCutSoundEffect.hpp"
#include "UnityEngine/AudioSource.hpp"

namespace HitsoundTweaqs::Patches::NoteCutSoundEffect_Misc
{
    // Declare the function that applies the prefix logic for Init
    void ApplyInitPatchLogic(GlobalNamespace::NoteCutSoundEffect *self, bool &ignoreSaberSpeed_ref, UnityEngine::AudioSource *audioSource);

    // Note: No InstallHook function needed here
} // namespace HitsoundTweaqs::Patches::NoteCutSoundEffect_Misc