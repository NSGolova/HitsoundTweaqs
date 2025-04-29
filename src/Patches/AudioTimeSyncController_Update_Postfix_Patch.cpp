#include "main.hpp"
#include "include/Patches/AudioTimeSyncController_Update_Postfix_Patch.hpp"

// Include necessary headers
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/Time.hpp" // Needed for captureFramerate

#include <cmath> // For std::abs

using namespace GlobalNamespace;
using namespace UnityEngine;

// Define the hook logic within the patch namespace
namespace HitsoundTweaqs::Patches::AudioTimeSyncController_Update_Postfix
{

    // State for the DSP Offset averaging patch
    static bool firstCorrectionDone = false;
    static int averageCount = 1;
    static double averageOffset = 0.0;
    static double lastAppliedDspTimeOffset = 0.0; // Store the last applied offset (renamed for clarity)

    // Postfix Hook for AudioTimeSyncController::Update
    MAKE_HOOK_MATCH(Hook_Update_Postfix, &AudioTimeSyncController::Update,
                    void, AudioTimeSyncController *self)
    {
        // Run original method first
        Hook_Update_Postfix(self);

        // --- Start Postfix Logic (DSP Offset Averaging from LATEST C# patch) ---
        const double maxDiscrepancy = 0.05;
        const double syncOffset = -0.0043; // Magic number from patch

        // Check state
        if (self->_state == AudioTimeSyncController::State::Stopped)
        {
            firstCorrectionDone = false;    // Reset averaging state
            lastAppliedDspTimeOffset = 0.0; // Also reset stored offset
            return;
        }

        // Skip if capturing frames
        if (Time::get_captureFramerate() != 0)
        {
            // SQLogger.debug("Skipping DSP offset patch during frame capture.");
            return;
        }

        // Check audio validity
        if (!self->_audioSource || !self->_audioSource->get_clip() || self->_audioSource->get_clip()->get_frequency() <= 0)
        {
            // Don't apply correction if invalid, keep game's value
            return;
        }

        // Calculate target offset based on current audio state
        double audioTimeSamples = (double)self->_audioSource->get_timeSamples();
        double clipFrequency = (double)self->_audioSource->get_clip()->get_frequency();
        double currentAudioTime = audioTimeSamples / clipFrequency;
        // VERIFY: _timeScale field exists
        double targetOffset = AudioSettings::get_dspTime() - (currentAudioTime / (double)self->_timeScale);

        // Update average and determine best offset to store locally
        if (!firstCorrectionDone || std::abs(averageOffset - targetOffset) > maxDiscrepancy)
        {
            // SQLogger.debug("Postfix: Resetting DSP average offset to target: %.6f", targetOffset);
            averageOffset = targetOffset;
            averageCount = 1;
            firstCorrectionDone = true;
            lastAppliedDspTimeOffset = targetOffset; // Store this as the new baseline
        }
        else
        {
            // Lock in value after some time?
            if (averageCount < 10000)
            {
                // Update cumulative average
                averageOffset = (averageOffset * averageCount + targetOffset) / (averageCount + 1);
                averageCount++;
                // SQLogger.debug("Postfix: Updating DSP average: %.6f (count %d, target %.6f)", averageOffset, averageCount, targetOffset);

                // Decide which offset (current target or last stored) is closer to the average
                if (std::abs(targetOffset - (averageOffset + syncOffset)) < std::abs(lastAppliedDspTimeOffset - (averageOffset + syncOffset)))
                {
                    // SQLogger.debug("Postfix: Storing current target as best offset: %.6f", targetOffset);
                    lastAppliedDspTimeOffset = targetOffset; // Update stored best value
                }
                // else: keep the previously stored lastAppliedDspTimeOffset as it's still closer
            }
            // else: average locked, keep using lastAppliedDspTimeOffset
        }

        // ALWAYS apply the determined best offset (stored locally) to the game's field
        // VERIFY: _dspTimeOffset field exists
        self->_dspTimeOffset = lastAppliedDspTimeOffset;
        // --- End Postfix Logic ---
    }

    // Installation function definition
    void InstallHook()
    {
        INSTALL_HOOK(SQLogger, Hook_Update_Postfix);
    }

} // namespace HitsoundTweaqs::Patches::AudioTimeSyncController_Update_Postfix