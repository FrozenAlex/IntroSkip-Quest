#include "Gameplay.hpp"
#include "Config.hpp"
#include "Utils/IntroSkipUtils.hpp"
#include "beatsaber-hook/shared/safeptr.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "metacore-bs/shared/events.hpp"
#include "metacore-bs/shared/input.hpp"
#include "metacore-bs/shared/internals.hpp"
#include "GlobalNamespace/MultiplayerPlayersManager.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/ComboUIController.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/Canvas.hpp"
#include "UnityEngine/Time.hpp"
#include <optional>

namespace IntroSkip {
namespace {
    struct State {
        safe_ptr<TMPro::TextMeshProUGUI*> text;
        SkipTimePairs skips;
        size_t index = 0;
        float timeHeld = 0;
        float requiredHoldTime = 0;
        bool bothTriggers = true;
        bool triggersPressed = false;
    };
    std::optional<State> state;

    void Clear() {
        if (state && state->text && UnityW<TMPro::TextMeshProUGUI>(state->text.ptr()))
            UnityEngine::Object::Destroy(state->text->get_gameObject());
        state.reset(); // Release all managed roots and native skip data.
    }

    void SetText(bool visible) {
        if (state->text && UnityW<TMPro::TextMeshProUGUI>(state->text.ptr())) {
            auto object = state->text->get_gameObject();
            if (object->get_activeSelf() != visible) object->SetActive(visible);
        }
    }

    void TriggerHeld() {
        if (!state) return;
        using namespace MetaCore::Input;
        // MetaCore broadcasts for either hand; keep the configured both-hand rule
        // and the existing 0.85 activation threshold.
        bool left = GetAxis(Left, TriggerStrength) > 0.85f;
        bool right = GetAxis(Right, TriggerStrength) > 0.85f;
        state->triggersPressed = state->bothTriggers ? left && right : left || right;
        if (!state->triggersPressed) state->timeHeld = 0;
    }

    void TriggerReleased() {
        if (!state) return;
        state->triggersPressed = false;
        state->timeHeld = 0;
    }

    void Start() {
        Clear();
        if (!getIntroSkipConfig().isEnabled.GetValue()) return;
        namespace Game = MetaCore::Internals;
        if (!Game::referencesValid || !Game::audioTimeSyncController || !Game::beatmapData) return;
        if (UnityEngine::Object::FindObjectOfType<GlobalNamespace::MultiplayerPlayersManager*>()) return;
        auto& current = state.emplace();
        current.skips = Utils::CalculateSkipTimePairs(Game::beatmapData->i___GlobalNamespace__IReadonlyBeatmapData(), Game::songLength);
        current.requiredHoldTime = getIntroSkipConfig().minHoldTime.GetValue();
        current.bothTriggers = getIntroSkipConfig().bothTriggers.GetValue();
        TriggerHeld(); // Seed input if the triggers were already held on scene entry.
        if (current.skips.empty()) return;
        // MetaCore supplies gameplay data; only the optional HUD parent needs a lookup.
        if (auto combo = UnityEngine::Object::FindObjectOfType<GlobalNamespace::ComboUIController*>()) {
            auto parent = combo->get_transform()->get_parent();
            auto text = BSML::Helpers::CreateText<TMPro::TextMeshProUGUI*>(parent, "Press Triggers to Skip", {0, 0});
            if (!parent->GetComponent<UnityEngine::Canvas*>()) parent->get_gameObject()->AddComponent<UnityEngine::Canvas*>();
            text->get_transform()->set_localPosition({0.0f, 1.95f, 0.0f});
            text->get_transform()->set_localScale({0.025f, 0.025f, 0.025f});
            text->set_alignment(TMPro::TextAlignmentOptions::Center);
            text->set_fontSize(8);
            text->get_gameObject()->SetActive(false);
            state->text = text;
        }
    }

    void Update() {
        if (!state || state->index >= state->skips.size()) return;
        namespace Game = MetaCore::Internals;
        if (!Game::referencesValid || !Game::audioTimeSyncController ||
            !UnityW<GlobalNamespace::AudioTimeSyncController>(Game::audioTimeSyncController)) {
            Clear();
            return;
        }
        auto& current = *state;
        float songTime = Game::songTime;
        auto [start, end] = current.skips[current.index];
        if (end <= songTime + 10 * UnityEngine::Time::get_deltaTime()) {
            SetText(false);
            current.timeHeld = 0;
            ++current.index;
            return;
        }
        if (start >= songTime) return;
        SetText(true);
        bool pressed = current.triggersPressed;
        bool playing = Game::audioTimeSyncController->get_state() == GlobalNamespace::IAudioTimeSource_State::Playing;
        if (pressed && playing && current.timeHeld >= current.requiredHoldTime)
            Game::audioTimeSyncController->_audioSource->set_time(end);
        if (pressed) current.timeHeld += UnityEngine::Time::get_deltaTime();
        else current.timeHeld = 0;
    }
}

void RegisterGameplayEvents() {
    using namespace MetaCore::Events;
    AddCallback(MetaCore::Input::PressEvents, MetaCore::Input::Trigger, TriggerHeld);
    AddCallback(MetaCore::Input::HoldEvents, MetaCore::Input::Trigger, TriggerHeld);
    AddCallback(MetaCore::Input::ReleaseEvents, MetaCore::Input::Trigger, TriggerReleased);
    AddCallback(GameplaySceneStarted, Start);
    AddCallback(MetaCore::Events::Update, Update);
    AddCallback(MapEnded, Clear);
    AddCallback(MapRestarted, Clear);
    AddCallback(GameplaySceneEnded, Clear);
    AddCallback(SoftRestart, Clear);
}
}
