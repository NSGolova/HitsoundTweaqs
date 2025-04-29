#pragma once

#include "GlobalNamespace/NoteCutSoundEffect.hpp"
#include "GlobalNamespace/NoteController.hpp"

namespace HitsoundTweaqs::Patches::Chain_Element_Volume_Multiplier
{
    // Declare the function that applies the prefix logic
    void ApplyPatchLogic(GlobalNamespace::NoteCutSoundEffect *self, float &volumeMultiplier_ref, GlobalNamespace::NoteController *noteController);

    // Note: No InstallHook function needed here as logic is called from another hook
} // namespace HitsoundTweaqs::Patches::Chain_Element_Volume_Multiplier