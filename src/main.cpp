#include "main.hpp"
#include "ModConfig.hpp"

#include "config-utils/shared/config-utils.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "HMUI/Touchable.hpp"

#include "beatsaber-hook/shared/config/rapidjson-utils.hpp"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/filereadstream.h"

#include "UnityEngine/GameObject.hpp"

#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML.hpp"

// Need codegen includes for Unity types
#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/AudioConfiguration.hpp"

#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/RectOffset.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "UnityEngine/UI/ContentSizeFitter.hpp"
#include "UnityEngine/UI/VerticalLayoutGroup.hpp"
#include "UnityEngine/UI/HorizontalLayoutGroup.hpp"

#include "GlobalNamespace/NoteCutSoundEffect.hpp"
#include "UnityEngine/Random.hpp"

// Include Patch headers
#include "include/Patches/NoteCutSoundEffect_Random_Pitch_Patch.hpp"
#include "include/Patches/Early_Note_Spawn_Fix.hpp"
#include "include/Patches/Chain_Element_Hitsound_Patch.hpp"
#include "include/Patches/NoteCutSoundEffect_NoteWasCut_Patch.hpp"
#include "include/Patches/NoteCutSoundEffect_LateUpdate_Patch.hpp"
#include "include/Patches/AudioTimeSyncController_Update_Postfix_Patch.hpp"
// #include "include/Patches/AnotherPatch.hpp" // Example for future patches

using namespace GlobalNamespace;
using namespace UnityEngine;

// Called at the early stages of game loading
MOD_EXPORT void setup(CModInfo *info) noexcept
{
    *info = modInfo.to_c();

    getModConfig().Init(modInfo);

    Paper::Logger::RegisterFileContextId(SQLogger.tag);

    SQLogger.info("Completed setup!");
}

// Helper functions for BSML UI Creation (can be moved to a separate file later)
BSML::ToggleSetting *AddConfigToggle(UnityEngine::UI::VerticalLayoutGroup *vertical, ConfigUtils::ConfigValue<bool> &configValue, std::string_view text, std::string_view hoverHint)
{
    auto toggle = BSML::Lite::CreateToggle(vertical->get_transform(), text,
                                           configValue.GetValue(), [&configValue](bool newValue)
                                           { configValue.SetValue(newValue); });
    if (!hoverHint.empty())
    {
        BSML::Lite::AddHoverHint(toggle->get_gameObject(), hoverHint);
    }
    return toggle;
}

BSML::IncrementSetting *AddConfigIncrementFloat(UnityEngine::UI::VerticalLayoutGroup *vertical, ConfigUtils::ConfigValue<float> &configValue, std::string_view text, int decimals, float increment, float minValue, float maxValue, std::string_view hoverHint)
{
    auto incSetting = BSML::Lite::CreateIncrementSetting(vertical->get_transform(), text,
                                                         decimals, increment, configValue.GetValue(), minValue, maxValue,
                                                         [&configValue](float newValue)
                                                         { configValue.SetValue(newValue); });
    if (!hoverHint.empty())
    {
        BSML::Lite::AddHoverHint(incSetting->get_gameObject(), hoverHint);
    }
    return incSetting;
}

// Settings UI creation function
void DidActivate(UnityEngine::GameObject *gameObject, bool firstActivation)
{
    if (!firstActivation)
        return;

    auto parent = gameObject->get_transform();
    auto vertical = BSML::Lite::CreateVerticalLayoutGroup(parent);
    vertical->set_spacing(0.4f);
    vertical->set_padding(UnityEngine::RectOffset::New_ctor(2, 2, 2, 2));

    // Add Config Elements with correct hover hints from BSML
    // Booleans (Toggles)
    AddConfigToggle(vertical, getModConfig().IgnoreSaberSpeed, "Ignore Saber Speed",
                    "When enabled, hitsounds always play even when the sabers are not moving.");
    AddConfigToggle(vertical, getModConfig().StaticSoundPos, "Static Sound Position",
                    "When enabled, hitsounds are located at your feet, instead of your saber tips.");
    AddConfigToggle(vertical, getModConfig().EnableSpatialization, "Enable Spatialization",
                    "When disabled, hitsounds will be played as-is without further processing, instead of being spatialized.");
    AddConfigToggle(vertical, getModConfig().EnableChainElementHitsounds, "Enable Chain Hitsounds",
                    "When enabled, hitsounds will be played for chain elements.");

    // Floats (Increment Settings - matching PC ranges/increment)
    AddConfigIncrementFloat(vertical, getModConfig().RandomPitchMin, "Min Random Pitch", 2, 0.05f, 0.0f, 2.0f,
                            "The minimum value for randomized hitsound pitch. If minimum and maximum are equal, hitsound pitch is not randomized.");
    AddConfigIncrementFloat(vertical, getModConfig().RandomPitchMax, "Max Random Pitch", 2, 0.05f, 0.0f, 2.0f,
                            "The maximum value for randomized hitsound pitch. If minimum and maximum are equal, hitsound pitch is not randomized.");
    AddConfigIncrementFloat(vertical, getModConfig().ChainElementVolumeMultiplier, "Chain Volume Multiplier", 2, 0.05f, 0.0f, 2.0f,
                            "Volume multiplier used for chain element hitsounds.");
}

// --- Hooks Installation ---
void InstallAllHooks()
{
    SQLogger.info("Installing hooks...");
    HitsoundTweaqs::Patches::NoteCutSoundEffect_Random_Pitch::InstallHook();
    HitsoundTweaqs::Patches::Early_Note_Spawn_Fix::InstallHooks();
    HitsoundTweaqs::Patches::Chain_Element_Hitsound::InstallHook();
    HitsoundTweaqs::Patches::NoteCutSoundEffect_NoteWasCut::InstallHook();
    HitsoundTweaqs::Patches::NoteCutSoundEffect_LateUpdate::InstallHook();
    HitsoundTweaqs::Patches::AudioTimeSyncController_Update_Postfix::InstallHook();
    SQLogger.info("All hooks installed.");
}

// Called later on in the game loading - a good time to install function hooks
MOD_EXPORT "C" void late_load()
{
    il2cpp_functions::Init();

    InstallAllHooks(); // Call the central hook installation function

    SQLogger.info("Registering UI...");
    BSML::Init(); // Ensure BSML is initialized before registering UI
    BSML::Register::RegisterGameplaySetupTab("HitsoundTweaqs", DidActivate);
    // Register GameplaySetup or other UI if needed

    SQLogger.info("Applying Audio Settings...");
    // Logic from AudioSettingsVoicesManager
    const int NumVirtualVoices = 128;
    const int NumRealVoices = 64;

    auto currentConfig = UnityEngine::AudioSettings::GetConfiguration();

    currentConfig.numVirtualVoices = NumVirtualVoices;
    currentConfig.numRealVoices = NumRealVoices;

    // Resolve the actual native function target: SetConfiguration
    // Signature is static bool SetConfiguration(AudioConfiguration config)
    static auto set_config_icall = il2cpp_utils::resolve_icall<bool, ByRef<UnityEngine::AudioConfiguration>>("UnityEngine.AudioSettings::SetConfiguration_Injected");
    if (set_config_icall)
    {
        ByRef<UnityEngine::AudioConfiguration> currentConfigRef = ByRef<UnityEngine::AudioConfiguration>(&currentConfig);
        bool success = set_config_icall(currentConfigRef);
        if (success)
        {
            auto newConfig = UnityEngine::AudioSettings::GetConfiguration();
            SQLogger.info("AudioSettings SetConfiguration successful.");
            SQLogger.info("New virtual voices: {}, real voices: {}", newConfig.numVirtualVoices, newConfig.numRealVoices);
            if (newConfig.numVirtualVoices != NumVirtualVoices || newConfig.numRealVoices != NumRealVoices)
            {
                SQLogger.warn("Failed to set audio voices to the desired values, check logs above.");
            }
        }
        else
        {
            SQLogger.error("UnityEngine.AudioSettings::SetConfiguration call returned false!");
        }
    }
    else
    {
        SQLogger.error("Failed to resolve UnityEngine.AudioSettings::SetConfiguration icall!");
    }

    SQLogger.info("HitsoundTweaqs late_load complete.");
}