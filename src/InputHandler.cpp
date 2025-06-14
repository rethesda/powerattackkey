#include "InputHandler.h"
#include "Settings.h"

InputEventHandler* InputEventHandler::GetSingleton()
{
    static InputEventHandler instance;
    return &instance;
}

RE::BSEventNotifyControl InputEventHandler::ProcessEvent(
    RE::InputEvent* const* a_event,
    RE::BSTEventSource<RE::InputEvent*>*)
{
    if (!a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    if (const auto ui{ RE::UI::GetSingleton() }) {

        // Check if any menu is open
        if (ui->GameIsPaused() || ui->IsApplicationMenuOpen() || ui->IsItemMenuOpen() || ui->IsModalMenuOpen()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (const auto control_map{ RE::ControlMap::GetSingleton() }; control_map->IsMovementControlsEnabled()) {
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (player && player->Is3DLoaded()) {
                for (auto e{ *a_event }; e != nullptr; e = e->next) {
                    if (const auto btn_event{ e->AsButtonEvent() }) {
 
                        const auto device{ btn_event->GetDevice() };
                        auto keycode{ btn_event->GetIDCode() };

                        using enum RE::INPUT_DEVICE;
                        if (device != kKeyboard && device != kGamepad && device != kMouse) {
                            return RE::BSEventNotifyControl::kContinue;
                        }
                        if (device == kGamepad) {
                            keycode = SKSE::InputMap::GamepadMaskToKeycode(keycode);
                        }
                        // logger::info("Key Pressed: {}", keycode);


                        // Update state of combo keys
                        if (Settings::comboKey>0 && keycode == Settings::comboKey) {
                            if (btn_event->IsHeld()) comboActive = true;
                            if (btn_event->IsUp()) comboActive = false;
                        }
                        if (Settings::comboKeyAlt1>0 && keycode == Settings::comboKeyAlt1) {
                            if (btn_event->IsHeld()) comboActiveAlt1 = true;
                            if (btn_event->IsUp()) comboActiveAlt1 = false;
                        }
                        if (Settings::comboKeyAlt2>0 && keycode == Settings::comboKeyAlt2) {
                            if (btn_event->IsHeld()) comboActiveAlt2 = true;
                            if (btn_event->IsUp()) comboActiveAlt2 = false;
                        }


                        // Check if player can do attacks
                        const auto playerState = player->AsActorState();
                        if (!(!player->IsInKillMove() ||  playerState->GetWeaponState() == RE::WEAPON_STATE::kDrawn ||
                            playerState->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal ||
                            playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal ||
                            playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal ||
                            playerState->GetFlyState() == RE::FLY_STATE::kNone)){
                            // logger::info("Player cannot attack currently, ignoring input");
                            return RE::BSEventNotifyControl::kContinue;
                        }


                        // Logic related to holding attack key with consecutive light attacks enabled
                        if (IsRightHandKey(device, keycode) && Settings::holdConsecutiveLA) {
                            // Reset variables when attack key is up
                            if (btn_event->IsUp()){
                                lightAttackWaiting = false;
                                lightAttackHeldTime = 0.0f;
                            }
                            bool bIsBlocking = false;
                            player->GetGraphVariableBool("Isblocking", bIsBlocking);
                            if (btn_event->IsHeld() && HasEquipedWeapon(player, false) && !bIsBlocking) {
                                // Check difference in time since the last time we entered here
                                // I apply a wait time between triggers based on setting
                                float currentLAHeldTime = btn_event->heldDownSecs;
                                if (!lightAttackWaiting) {
                                    lightAttackHeldTime = currentLAHeldTime;
                                }
                                if (currentLAHeldTime - lightAttackHeldTime > Settings::consecutiveAttacksDelay) {
                                    lightAttackWaiting = false;
                                    PerformAction(LARightHandAction, player);
                                } else {
                                    lightAttackWaiting = true;
                                }
                            }
                            return RE::BSEventNotifyControl::kContinue;
                        } 
                        
                        // Check if a power attack key is being pressed
                        if (   keycode == Settings::rightHandKey || keycode == Settings::leftHandKey || keycode == Settings::bothHandsKey
                            || keycode == Settings::rightHandKeyAlt1 || keycode == Settings::leftHandKeyAlt1 || keycode == Settings::bothHandsKeyAlt1
                            || keycode == Settings::rightHandKeyAlt2 || keycode == Settings::leftHandKeyAlt2 || keycode == Settings::bothHandsKeyAlt2) {

                            // Reset variables when any power attack key is up. 
                            if (btn_event->IsUp()){
                                powerAttackWaiting = false;
                                powerAttackHeldTime = 0.0f;
                            }

                            if (!(btn_event->IsDown() || (btn_event->IsHeld() && Settings::holdConsecutivePA))) {
                                return RE::BSEventNotifyControl::kContinue;
                            }

                            // If the power attack is being held, we repeat the same logic for consecutive light attacks
                            if (btn_event->IsHeld() && Settings::holdConsecutivePA) {
                                float currentPAHeldTime = btn_event->heldDownSecs;
                                if (!powerAttackWaiting) {
                                    powerAttackHeldTime = currentPAHeldTime;
                                }
                                // logger::info("Difference: {}",currentPAHeldTime - powerAttackHeldTime);
                                if (currentPAHeldTime - powerAttackHeldTime <= Settings::consecutiveAttacksDelay) {
                                    powerAttackWaiting = true;
                                    return RE::BSEventNotifyControl::kContinue;
                                } else {
                                    powerAttackWaiting = false;
                                }
                            }

                            // Depending of the pressed keys, check for stamina cost and equiped weapon to trigger action
                            if (keycode == Settings::rightHandKey && (Settings::comboKey<=0 || comboActive) && HasEquipedWeapon(player, false)) {
                                if (HasEnoughStamina(player, true, false)) {
                                    // logger::info("Right Hand Key Pressed");
                                    PerformAction(PARightHandAction, player);
                                }
                            } else if (keycode == Settings::leftHandKey && (Settings::comboKey<=0 || comboActive) && HasEquipedWeapon(player, true)) {
                                if (HasEnoughStamina(player, false, true)) {
                                    // logger::info("Left Hand Key Pressed");
                                    PerformAction(PALeftHandAction, player);
                                }
                            } else if (keycode == Settings::bothHandsKey && (Settings::comboKey<=0 || comboActive) && HasEquipedWeapon(player, false) && HasEquipedWeapon(player, true)) {
                                if (HasEnoughStamina(player, true, true)) {
                                    // logger::info("Both Hands Key Pressed");
                                    PerformAction(PABothHandsAction, player);
                                }

                            } else if (keycode == Settings::rightHandKeyAlt1 && (Settings::comboKeyAlt1<=0 || comboActiveAlt1) && HasEquipedWeapon(player, false)) {
                                if (HasEnoughStamina(player, true, false)) {
                                    // logger::info("Right Hand Key Alt1 Pressed");
                                    PerformAction(PARightHandAction, player);
                                }
                            } else if (keycode == Settings::leftHandKeyAlt1 && (Settings::comboKeyAlt1<=0 || comboActiveAlt1) && HasEquipedWeapon(player, true)) {
                                // logger::info("Left Hand Key Alt1 Pressed");
                                if (HasEnoughStamina(player, false, true)) {
                                    PerformAction(PALeftHandAction, player);
                                }
                            } else if (keycode == Settings::bothHandsKeyAlt1 && (Settings::comboKeyAlt1<=0 || comboActiveAlt1) && HasEquipedWeapon(player, false) && HasEquipedWeapon(player, true)) {
                                if (HasEnoughStamina(player, true, true)) {
                                    // logger::info("Both Hands Key Alt1 Pressed");
                                    PerformAction(PABothHandsAction, player);
                                }

                            } else if (keycode == Settings::rightHandKeyAlt2 && (Settings::comboKeyAlt2<=0 || comboActiveAlt2) && HasEquipedWeapon(player, false)) {
                                if (HasEnoughStamina(player, true, false)) {
                                    // logger::info("Right Hand Key Alt2 Pressed");
                                    PerformAction(PARightHandAction, player);
                                }
                            } else if (keycode == Settings::leftHandKeyAlt2 && (Settings::comboKeyAlt2<=0 || comboActiveAlt2) && HasEquipedWeapon(player, true)) {
                                // logger::info("Left Hand Key Alt2 Pressed");
                                if (HasEnoughStamina(player, false, true)) {
                                    PerformAction(PALeftHandAction, player);
                                }
                            } else if (keycode == Settings::bothHandsKeyAlt2 && (Settings::comboKeyAlt2<=0 || comboActiveAlt2) && HasEquipedWeapon(player, false) && HasEquipedWeapon(player, true)) {
                                if (HasEnoughStamina(player, true, true)) {
                                    // logger::info("Both Hands Key Alt2 Pressed");
                                    PerformAction(PABothHandsAction, player);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}

void InputEventHandler::PerformAction(RE::BGSAction* action, RE::Actor* player) {
	if (action && player) {
        std::unique_ptr<RE::TESActionData> data(RE::TESActionData::Create());
        data->source = RE::NiPointer<RE::TESObjectREFR>(player);
        data->action = action;
        typedef bool func_t(RE::TESActionData*);
        REL::Relocation<func_t> func{ RELOCATION_ID(40551, 41557) };
        func(data.get());
	}
}

void InputEventHandler::GetAttackKeys(){
    auto* controlMap = RE::ControlMap::GetSingleton();
    const auto* userEvents = RE::UserEvents::GetSingleton();

    rightAttackKeyKeyboard = controlMap->GetMappedKey(userEvents->rightAttack, RE::INPUT_DEVICE::kKeyboard);
    rightAttackKeyMouse = controlMap->GetMappedKey(userEvents->rightAttack, RE::INPUT_DEVICE::kMouse);
    rightAttackKeyGamepad = controlMap->GetMappedKey(userEvents->rightAttack, RE::INPUT_DEVICE::kGamepad);
    rightAttackKeyGamepad = SKSE::InputMap::GamepadMaskToKeycode(rightAttackKeyGamepad);
}

bool InputEventHandler::HasEquipedWeapon(const RE::Actor* player, bool leftHand) {
    RE::TESObjectWEAP* akWeapon = reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(leftHand));
    if (!akWeapon || (!akWeapon->IsBow() && !akWeapon->IsStaff() && !akWeapon->IsCrossbow() && akWeapon->IsWeapon())) return true;
    return false;
}

bool InputEventHandler::HasEquippedTwoHandedWeapon(const RE::PlayerCharacter* player) {
    const auto* rightHand = player->GetEquippedObject(false);
    const auto* rightWeapon = rightHand ? rightHand->As<RE::TESObjectWEAP>() : nullptr;
    return rightWeapon && (rightWeapon->IsTwoHandedAxe() || rightWeapon->IsTwoHandedSword());
}

 // The flashing doesn't work properly with left hands power attacks
bool InputEventHandler::HasEnoughStamina(RE::PlayerCharacter* player, bool rightHand, bool leftHand) {
    if (!Settings::requireStaminaPA) return true;
    int staminaCost = Settings::staminaCost1H;
    if (rightHand && leftHand) staminaCost = Settings::staminaCost1H*2;
    else if (rightHand && !leftHand && HasEquippedTwoHandedWeapon(player)) staminaCost = Settings::staminaCost2H;
    if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) >= staminaCost) return true;
    FlashHUDMeter(RE::ActorValue::kStamina);
    return false;
}

void InputEventHandler::FlashHUDMeter(RE::ActorValue a_av) {
    static REL::Relocation<decltype(FlashHUDMeter)> FlashHUDMenuMeter{RELOCATION_ID(51907, 52845)};
    return FlashHUDMenuMeter(a_av);
}

bool InputEventHandler::IsRightHandKey(const RE::INPUT_DEVICE device, const std::uint32_t key) const {
    switch (device) {
        case RE::INPUT_DEVICE::kKeyboard:
            return key == rightAttackKeyKeyboard;
        case RE::INPUT_DEVICE::kMouse:
            return key == rightAttackKeyMouse;
        case RE::INPUT_DEVICE::kGamepad:
            return key == rightAttackKeyGamepad;
        default:
            return false;
    }
}