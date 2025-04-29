#include "main.hpp"
#include "ModConfig.hpp"
#include "include/Patches/Chain_Element_Volume_Multiplier_Patch.hpp"

// Include necessary headers
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "GlobalNamespace/NoteData.hpp" // Needed for GameplayType check

using namespace GlobalNamespace;

// Define the logic within the patch namespace
namespace HitsoundTweaqs::Patches::Chain_Element_Volume_Multiplier
{

    // Function to apply the patch logic (simulating Prefix effect)
    void ApplyPatchLogic(GlobalNamespace::NoteCutSoundEffect *self, float &volumeMultiplier_ref, GlobalNamespace::NoteController *noteController)
    {
        // apply chain element volume multiplier
        if (noteController && noteController->noteData && noteController->noteData->gameplayType == NoteData::GameplayType::BurstSliderElement)
        {
            float multiplier = getModConfig().ChainElementVolumeMultiplier.GetValue();
            // SQLogger.debug("Applying Chain Element Volume Multiplier: %.2f to original %.2f", multiplier, volumeMultiplier_ref);
            volumeMultiplier_ref *= multiplier;
        }
    }

} // namespace HitsoundTweaqs::Patches::Chain_Element_Volume_Multiplier