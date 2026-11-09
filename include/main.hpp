#pragma once

#define INTRO_SKIP_EXPORT __attribute__((visibility("default")))
#ifdef __cplusplus
#define INTRO_SKIP_EXPORT_FUNC extern "C" INTRO_SKIP_EXPORT
#else
#define INTRO_SKIP_EXPORT_FUNC INTRO_SKIP_EXPORT
#endif

// Include the modloader header, which allows us to tell the modloader which mod this is, and the version etc.
#include "scotland2/shared/loader.hpp"

// beatsaber-hook is a modding framework that lets us call functions and fetch field values from in the game
// It also allows creating objects, configuration, and importantly, hooking methods to modify their values
#include "paper2_scotland2/shared/logger.hpp"
#include "beatsaber-hook/shared/api.hpp"
#include "beatsaber-hook/shared/hooking.hpp"

const Paper::ConstLoggerContext<sizeof(MOD_ID)>& getLogger();