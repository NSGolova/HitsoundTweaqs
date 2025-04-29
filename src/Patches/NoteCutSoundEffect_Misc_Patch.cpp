#include "main.hpp"
#include "ModConfig.hpp"
#include "include/Patches/NoteCutSoundEffect_Misc_Patch.hpp"

// Include necessary headers
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "UnityEngine/AudioSource.hpp"

using namespace GlobalNamespace;
using namespace UnityEngine;

// Define the logic within the patch namespace
namespace HitsoundTweaqs::Patches::NoteCutSoundEffect_Misc
{

    // Function to apply the patch logic for Init (simulating Prefix effect)
    void ApplyInitPatchLogic(GlobalNamespace::NoteCutSoundEffect *self, bool &ignoreSaberSpeed_ref, UnityEngine::AudioSource *audioSource)
    {
        bool ignoreSpeed = getModConfig().IgnoreSaberSpeed.GetValue();
        bool enableSpat = getModConfig().EnableSpatialization.GetValue();

        SQLogger.debug("Misc Patch: Setting ignoreSaberSpeed to %d, spatialization to %d", ignoreSpeed, enableSpat);

        // if true, always play hitsounds even if saber isn't moving
        ignoreSaberSpeed_ref = ignoreSpeed;

        // enable/disable spatialization
        if (audioSource)
        {
            audioSource->set_spatialize(enableSpat);
            // VERIFY: audioSource->set_spatialize exists
        }
        else
        {
            SQLogger.error("Cannot set spatialization, AudioSource is null in Misc Patch!");
        }
    }

} // namespace HitsoundTweaqs::Patches::NoteCutSoundEffect_Misc