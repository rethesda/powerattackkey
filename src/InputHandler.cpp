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

        if (const auto control_map{ RE::ControlMap::GetSingleton() }; control_map->IsMovementControlsEnabled()) {
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (player && player->Is3DLoaded()) {

                for (auto e{ *a_event }; e != nullptr; e = e->next) {
                    if (const auto btn_event{ e->AsButtonEvent() }) {
 
                        const auto device{ btn_event->GetDevice() };
                        auto keycode{ btn_event->GetIDCode() };

                        using enum RE::INPUT_DEVICE;
                        if (device != kKeyboard && device != kGamepad && device != kMouse) return RE::BSEventNotifyControl::kContinue;
                        if (device == kGamepad) keycode = SKSE::InputMap::GamepadMaskToKeycode(keycode);
                        if (device == kMouse) keycode = keycode + 256;

                        // Update state of combo keys
                        if (Settings::comboKey>0 && keycode == Settings::comboKey) comboActive = btn_event->IsPressed();
                        if (Settings::comboKeyAlt1>0 && keycode == Settings::comboKeyAlt1) comboActiveAlt1 = btn_event->IsPressed();
                        if (Settings::comboKeyAlt2>0 && keycode == Settings::comboKeyAlt2) comboActiveAlt2 = btn_event->IsPressed();

                        // Check if any menu is open
                        if (ui->GameIsPaused() || ui->IsApplicationMenuOpen() || ui->IsItemMenuOpen() || ui->IsModalMenuOpen() ||
                            ui->IsMenuOpen("Dialogue Menu") || ui->IsMenuOpen("Console") || ui->IsMenuOpen("TweenMenu") || ui->IsMenuOpen("LevelUp Menu")) {
                            return RE::BSEventNotifyControl::kContinue;
                        }

                        if (IsRightHandKey(device, keycode)) rightHandKeyPressed = btn_event->IsPressed();
                        if (IsLeftHandKey(device, keycode)) leftHandKeyPressed = btn_event->IsPressed();

                        bool isRightHandEquiped = HasEquipedWeapon(player, false);
                        bool isRightHandUnarmed = IsHandUnarmed(player, false);
                        bool isLeftHandEquiped = HasEquipedWeapon(player, true);
                        bool isLeftHandUnarmed = IsHandUnarmed(player, true);

                        bool isPowerAttackKey = keycode == Settings::rightHandKey || keycode == Settings::leftHandKey || keycode == Settings::bothHandsKey
                                               || keycode == Settings::rightHandKeyAlt1 || keycode == Settings::leftHandKeyAlt1 || keycode == Settings::bothHandsKeyAlt1
                                               || keycode == Settings::rightHandKeyAlt2 || keycode == Settings::leftHandKeyAlt2 || keycode == Settings::bothHandsKeyAlt2;

                        // Reset variables when keys are is first pressed or up
                        if ((btn_event->IsDown() || btn_event->IsUp()) && isPowerAttackKey){
                            powerAttackWaiting = false;
                            powerAttackHeldTime = 0.0f;
                        }
                        if ((btn_event->IsDown() || btn_event->IsUp()) && IsRightHandKey(device, keycode)){
                            rightAttackWaiting = false;
                            rightAttackHeldTime = 0.0f;
                        }
                        if ((btn_event->IsDown() || btn_event->IsUp()) && IsLeftHandKey(device, keycode)){
                            leftAttackWaiting = false;
                            leftAttackHeldTime = 0.0f;
                        }
                                
                        bool bIsBlocking = false;
                        player->GetGraphVariableBool("Isblocking", bIsBlocking);
                        bool bIsAttacking = false;
                        player->GetGraphVariableBool("IsAttacking", bIsAttacking);

                        // Check if player cannot do attacks
                        const auto playerState = player->AsActorState();
                        if (!(!player->IsInKillMove() &&  playerState->GetWeaponState() == RE::WEAPON_STATE::kDrawn &&
                            playerState->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal && playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
                            playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal && playerState->GetFlyState() == RE::FLY_STATE::kNone)){
                            // logger::info("Player cannot attack currently, ignoring input");
                            return RE::BSEventNotifyControl::kContinue;
                        }

                        // Check if any power attack key is being pressed
                        if (isPowerAttackKey && (btn_event->IsDown() || (btn_event->IsHeld() && Settings::holdConsecutivePA && bIsAttacking))) {
                            // If the power attack key is held, apply a wait time between actions
                            if (btn_event->IsHeld() && Settings::holdConsecutivePA && bIsAttacking) {
                                float currentPowerAttackHeldTime = btn_event->heldDownSecs;
                                if (!powerAttackWaiting) powerAttackHeldTime = currentPowerAttackHeldTime;
                                // logger::info("Difference: {}",currentPowerAttackHeldTime - powerAttackHeldTime);
                                // Check if wait time expired
                                if (currentPowerAttackHeldTime - powerAttackHeldTime <= Settings::consecutiveAttacksDelay) {
                                    powerAttackWaiting = true;
                                    return RE::BSEventNotifyControl::kContinue;
                                } else {
                                    powerAttackWaiting = false;
                                }
                            }

                            bool isUsingCombo = comboActive && !(comboActiveAlt1 || comboActiveAlt2);
                            bool isUsingComboAlt1 = comboActiveAlt1 && !(comboActive || comboActiveAlt2);
                            bool isUsingComboAlt2 = comboActiveAlt2 && !(comboActive || comboActiveAlt1);
                            bool isKeyWithCombo = false;

                            if (!(btn_event->IsHeld() && bIsBlocking)){

                                // Depending of the pressed keys, check for equiped weapon to trigger action
                                // Combos take priority
                                if (isUsingCombo && Settings::comboKey>0){
                                    if (keycode == Settings::rightHandKey && isRightHandEquiped){
                                        if (PerformRightHandPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                    } else if (keycode == Settings::leftHandKey && isLeftHandEquiped && (!isLeftHandUnarmed || (isLeftHandUnarmed && isRightHandUnarmed))){
                                        if (PerformLeftHandPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                    } else if (keycode == Settings::bothHandsKey){
                                        if (PerformBothHandsPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                    }
                                    if (keycode == Settings::rightHandKey || keycode == Settings::leftHandKey || keycode == Settings::bothHandsKey) isKeyWithCombo = true;
                                }
                                if (isUsingComboAlt1 && Settings::comboKeyAlt1>0){
                                    if (keycode == Settings::rightHandKeyAlt1 && isRightHandEquiped){
                                        if (PerformRightHandPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                    } else if (keycode == Settings::leftHandKeyAlt1 && isLeftHandEquiped && (!isLeftHandUnarmed || (isLeftHandUnarmed && isRightHandUnarmed))){
                                        if (PerformLeftHandPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                    } else if (keycode == Settings::bothHandsKeyAlt1){
                                        if (PerformBothHandsPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                    }
                                    if (keycode == Settings::rightHandKeyAlt1 || keycode == Settings::leftHandKeyAlt1 || keycode == Settings::bothHandsKeyAlt1) isKeyWithCombo = true;
                                }
                                if (isUsingComboAlt2 && Settings::comboKeyAlt2>0){
                                    if (keycode == Settings::rightHandKeyAlt2 && isRightHandEquiped){
                                        if (PerformRightHandPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                    } else if (keycode == Settings::leftHandKeyAlt2 && isLeftHandEquiped && (!isLeftHandUnarmed || (isLeftHandUnarmed && isRightHandUnarmed))){
                                        if (PerformLeftHandPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                    } else if (keycode == Settings::bothHandsKeyAlt2){
                                        if (PerformBothHandsPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                    }
                                    if (keycode == Settings::rightHandKeyAlt2 || keycode == Settings::leftHandKeyAlt2 || keycode == Settings::bothHandsKeyAlt2) isKeyWithCombo = true;
                                }

                                // If key is not part of a combo or if the related combo is disabled.
                                if (((keycode == Settings::rightHandKey && Settings::comboKey<=0) || 
                                     (keycode == Settings::rightHandKeyAlt1 && Settings::comboKeyAlt1<=0) || 
                                     (keycode == Settings::rightHandKeyAlt2 && Settings::comboKeyAlt2<=0) ) && isRightHandEquiped && !isKeyWithCombo){
                                    if (PerformRightHandPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                }else if (( (keycode == Settings::leftHandKey && Settings::comboKey<=0) || 
                                            (keycode == Settings::leftHandKeyAlt1 && Settings::comboKeyAlt1<=0) || 
                                            (keycode == Settings::leftHandKeyAlt2 && Settings::comboKeyAlt2<=0)) &&
                                            isLeftHandEquiped && (!isLeftHandUnarmed || (isLeftHandUnarmed && isRightHandUnarmed)) && !isKeyWithCombo){
                                    if (PerformLeftHandPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                }else if (( (keycode == Settings::bothHandsKey && Settings::comboKey<=0)) || 
                                            (keycode == Settings::bothHandsKeyAlt1 && Settings::comboKeyAlt1<=0) || 
                                            (keycode == Settings::bothHandsKeyAlt2 && Settings::comboKeyAlt2<=0) && !isKeyWithCombo){
                                    if (PerformBothHandsPA(player)) return RE::BSEventNotifyControl::kContinue; 
                                }

                            }
                        }

                        // Logic related to holding attack key with consecutive light attacks enabled
                        if (rightHandKeyPressed && leftHandKeyPressed && Settings::holdConsecutiveLA && Settings::consecutiveDualAttacks && bIsAttacking) {
                            if (btn_event->IsHeld() && isLeftHandEquiped && isRightHandEquiped && !bIsBlocking) {
                                // If the attack key is held, apply a wait time between actions
                                float currentRightAttackHeldTime = btn_event->heldDownSecs;
                                float currentLeftAttackHeldTime = btn_event->heldDownSecs;
                                if (!rightAttackWaiting) rightAttackHeldTime = currentRightAttackHeldTime;
                                if (!leftAttackWaiting) leftAttackHeldTime = currentLeftAttackHeldTime;
                                // logger::info("Difference: {}",currentPowerAttackHeldTime - powerAttackHeldTime);
                                // Check if wait time expired
                                if (currentRightAttackHeldTime - rightAttackHeldTime <= Settings::consecutiveAttacksDelay ||
                                    currentLeftAttackHeldTime - leftAttackHeldTime <= Settings::consecutiveAttacksDelay) {
                                    rightAttackWaiting = true;
                                    leftAttackWaiting = true;
                                    PerformAction(LABothHandsAction, player);
                                } else {
                                    rightAttackWaiting = false;
                                    leftAttackWaiting = false;
                                }
                            }
                            return RE::BSEventNotifyControl::kContinue;
                        } else if (IsRightHandKey(device, keycode) && Settings::holdConsecutiveLA && bIsAttacking) {
                            if (btn_event->IsHeld() && isRightHandEquiped && !bIsBlocking) {
                                // If the attack key is held, apply a wait time between actions
                                float currentRightAttackHeldTime = btn_event->heldDownSecs;
                                if (!rightAttackWaiting) rightAttackHeldTime = currentRightAttackHeldTime;
                                // logger::info("Difference: {}",currentPowerAttackHeldTime - powerAttackHeldTime);
                                // Check if wait time expired
                                if (currentRightAttackHeldTime - rightAttackHeldTime <= Settings::consecutiveAttacksDelay) {
                                    rightAttackWaiting = true;
                                    PerformAction(LARightHandAction, player);
                                } else {
                                    rightAttackWaiting = false;
                                }
                            }
                            return RE::BSEventNotifyControl::kContinue;
                        } else if (IsLeftHandKey(device, keycode) && Settings::holdConsecutiveLA && bIsAttacking){
                            if (btn_event->IsHeld() && isLeftHandEquiped && !bIsBlocking) {
                                // If the attack key is held, apply a wait time between actions
                                float currentLeftAttackHeldTime = btn_event->heldDownSecs;
                                if (!leftAttackWaiting) leftAttackHeldTime = currentLeftAttackHeldTime;
                                // logger::info("Difference: {}",currentPowerAttackHeldTime - powerAttackHeldTime);
                                // Check if wait time expired
                                if (currentLeftAttackHeldTime - leftAttackHeldTime <= Settings::consecutiveAttacksDelay) {
                                    leftAttackWaiting = true;
                                    PerformAction(LALeftHandAction, player);
                                } else {
                                    leftAttackWaiting = false;
                                }
                            }
                            return RE::BSEventNotifyControl::kContinue;
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

bool InputEventHandler::PerformRightHandPA(RE::PlayerCharacter* player) {
    bool isRightHandUnarmed = IsHandUnarmed(player, false);
    bool bInJumpState = false;
    player->GetGraphVariableBool("bInJumpState", bInJumpState);

    if (HasEnoughStamina(player, true, false)) {
        if (((isRightHandUnarmed || bInJumpState) && !Settings::usingMCO)) PerformAction(LARightHandAction, player);
        PerformAction(PARightHandAction, player);
        return true;
    }
    return false;
}

bool InputEventHandler::PerformLeftHandPA(RE::PlayerCharacter* player) {
    bool isRightHandEquiped = HasEquipedWeapon(player, false);
    bool isRightHandUnarmed = IsHandUnarmed(player, false);
    bool isLeftHandEquiped = HasEquipedWeapon(player, true);
    bool isLeftHandUnarmed = IsHandUnarmed(player, true);
    bool bothHandsEquiped = isRightHandEquiped && isLeftHandEquiped;
    bool bothHandsUnarmed = isRightHandUnarmed && isLeftHandUnarmed;
    bool bothHandsWeaponsEquiped = bothHandsEquiped && !(isRightHandUnarmed || isLeftHandUnarmed);

    if (HasEnoughStamina(player, false, true)) {
        if (!(Settings::usingMCO && (bothHandsWeaponsEquiped || bothHandsUnarmed))) PerformAction(LALeftHandAction, player);
        PerformAction(PALeftHandAction, player);
        return true;
    }
    return false;
}

bool InputEventHandler::PerformBothHandsPA(RE::PlayerCharacter* player) {
    bool bInJumpState = false;
    player->GetGraphVariableBool("bInJumpState", bInJumpState);
    if (HasEnoughStamina(player, true, true)) {
        if (bInJumpState) PerformAction(LABothHandsAction, player);
        PerformAction(PABothHandsAction, player);
        return true;
    }
    return false;
}

void InputEventHandler::GetAttackKeys(){
    auto* controlMap = RE::ControlMap::GetSingleton();
    const auto* userEvents = RE::UserEvents::GetSingleton();

    rightAttackKeyKeyboard = controlMap->GetMappedKey(userEvents->rightAttack, RE::INPUT_DEVICE::kKeyboard);
    rightAttackKeyMouse = controlMap->GetMappedKey(userEvents->rightAttack, RE::INPUT_DEVICE::kMouse);
    rightAttackKeyGamepad = controlMap->GetMappedKey(userEvents->rightAttack, RE::INPUT_DEVICE::kGamepad);
    rightAttackKeyGamepad = SKSE::InputMap::GamepadMaskToKeycode(rightAttackKeyGamepad);

    leftAttackKeyKeyboard = controlMap->GetMappedKey(userEvents->leftAttack, RE::INPUT_DEVICE::kKeyboard);
    leftAttackKeyMouse = controlMap->GetMappedKey(userEvents->leftAttack, RE::INPUT_DEVICE::kMouse);
    leftAttackKeyGamepad = controlMap->GetMappedKey(userEvents->leftAttack, RE::INPUT_DEVICE::kGamepad);
    leftAttackKeyGamepad = SKSE::InputMap::GamepadMaskToKeycode(leftAttackKeyGamepad);
}

bool InputEventHandler::HasEquipedWeapon(const RE::PlayerCharacter* player, bool leftHand) {
    RE::TESObjectWEAP* akWeapon = reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(leftHand));
    if ((leftHand && HasEquippedTwoHandedWeapon(player))) return false;
    if (!akWeapon || (!akWeapon->IsBow() && !akWeapon->IsStaff() && !akWeapon->IsCrossbow() && akWeapon->IsWeapon())) return true;
    return false;
}

bool InputEventHandler::IsHandUnarmed(const RE::PlayerCharacter* player, bool leftHand) {
    RE::TESObjectWEAP* akWeapon = reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(leftHand));
    if (!akWeapon || akWeapon->IsHandToHandMelee()) return true;
    return false;
}

bool InputEventHandler::HasEquippedTwoHandedWeapon(const RE::PlayerCharacter* player) {
    const auto* rightHand = player->GetEquippedObject(false);
    const auto* rightWeapon = rightHand ? rightHand->As<RE::TESObjectWEAP>() : nullptr;
    return rightWeapon && (rightWeapon->IsTwoHandedAxe() || rightWeapon->IsTwoHandedSword());
}

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
            return key-256 == rightAttackKeyMouse;
        case RE::INPUT_DEVICE::kGamepad:
            return key == rightAttackKeyGamepad;
        default:
            return false;
    }
}

bool InputEventHandler::IsLeftHandKey(const RE::INPUT_DEVICE device, const std::uint32_t key) const {
    switch (device) {
        case RE::INPUT_DEVICE::kKeyboard:
            return key == leftAttackKeyKeyboard;
        case RE::INPUT_DEVICE::kMouse:
            return key-256 == leftAttackKeyMouse;
        case RE::INPUT_DEVICE::kGamepad:
            return key == leftAttackKeyGamepad;
        default:
            return false;
    }
}