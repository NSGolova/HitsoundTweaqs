#include "main.hpp"
#include "ModConfig.hpp"
#include "include/Patches/Chain_Element_Hitsound_Patch.hpp"

// Include necessary headers
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "GlobalNamespace/NoteCutSoundEffectManager.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/ColorType.hpp"

using namespace GlobalNamespace;

// Define the hook logic within the patch namespace
namespace HitsoundTweaqs::Patches::Chain_Element_Hitsound
{

    // Hook for NoteCutSoundEffectManager.IsSupportedNote (Postfix)
    MAKE_HOOK_MATCH(Hook_IsSupportedNote, &NoteCutSoundEffectManager::IsSupportedNote,
                    bool, NoteCutSoundEffectManager *self, NoteData *noteData) // Return type is bool
    {
        // Call original first for postfix behavior
        bool originalResult = Hook_IsSupportedNote(self, noteData);

        if (noteData && noteData->gameplayType == NoteData::GameplayType::BurstSliderElement && noteData->colorType != ColorType::None)
        {
            bool enableChainHitsounds = getModConfig().EnableChainElementHitsounds.GetValue();
            // SQLogger.debug("IsSupportedNote Postfix: Note is BurstSliderElement, returning %d based on config", enableChainHitsounds);
            return enableChainHitsounds; // Override result based on config
        }

        // Otherwise, return the original result
        return originalResult;
    }

    // Installation function definition
    void InstallHook()
    {
        INSTALL_HOOK(SQLogger, Hook_IsSupportedNote);
    }

} // namespace HitsoundTweaqs::Patches::Chain_Element_Hitsound