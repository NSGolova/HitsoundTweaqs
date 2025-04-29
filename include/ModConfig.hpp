#pragma once
#include "config-utils/shared/config-utils.hpp"

DECLARE_CONFIG(ModConfig)
{
    // CONFIG_VALUE(ExampleValue, bool, "Example Value", false); // Placeholder for example

    // Ported from HitsoundTweaks C# PluginConfig
    CONFIG_VALUE(IgnoreSaberSpeed, bool, "Ignore Saber Speed", false, "Volume isn't affected by swing speed.");
    CONFIG_VALUE(StaticSoundPos, bool, "Static Sound Position", false, "Hitsounds play at a fixed position, ignoring note position.");
    CONFIG_VALUE(EnableSpatialization, bool, "Enable Spatialization", true, "Hitsounds are spatialized.");
    CONFIG_VALUE(RandomPitchMin, float, "Random Pitch Min", 0.9f, "Minimum random pitch multiplier.");
    CONFIG_VALUE(RandomPitchMax, float, "Random Pitch Max", 1.2f, "Maximum random pitch multiplier.");
    CONFIG_VALUE(EnableChainElementHitsounds, bool, "Enable Chain Element Hitsounds", false, "Play hitsounds for chain links/heads.");
    CONFIG_VALUE(ChainElementVolumeMultiplier, float, "Chain Element Volume Multiplier", 0.8f, "Volume multiplier for chain element hitsounds.");

    // Add other settings as needed during porting
};