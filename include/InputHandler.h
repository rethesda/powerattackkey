#pragma once

class InputEventHandler : public RE::BSTEventSink<RE::InputEvent*>
{
    public:
        static InputEventHandler* GetSingleton();
        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,RE::BSTEventSource<RE::InputEvent*>*) override;
        void GetAttackKeys();

    private:
        bool IsRightHandKey(const RE::INPUT_DEVICE device, const std::uint32_t key) const;

        static void PerformAction(RE::BGSAction* action, RE::Actor* a);
        static bool HasEquipedWeapon(const RE::Actor* player, bool leftHand);
        static bool HasEquippedTwoHandedWeapon(const RE::PlayerCharacter* player);
        static bool HasEnoughStamina(RE::PlayerCharacter* player, bool rightHand, bool leftHand);
        static void FlashHUDMeter(RE::ActorValue a_av);

        RE::BGSAction* PARightHandAction = RE::TESForm::LookupByID(0x13383)->As<RE::BGSAction>();
        RE::BGSAction* PALeftHandAction = RE::TESForm::LookupByID(0x2E2F6)->As<RE::BGSAction>();
        RE::BGSAction* PABothHandsAction = RE::TESForm::LookupByID(0x2E2F7)->As<RE::BGSAction>();
        RE::BGSAction* LARightHandAction = RE::TESForm::LookupByID(0x13005)->As<RE::BGSAction>();

        std::uint32_t rightAttackKeyKeyboard = 255;
        std::uint32_t rightAttackKeyMouse = 255;
        std::uint32_t rightAttackKeyGamepad = 255;

        bool comboActive = false;
        bool comboActiveAlt1 = false;
        bool comboActiveAlt2 = false;

        float lightAttackHeldTime = 0.0f;
        bool lightAttackWaiting = false;

        float powerAttackHeldTime = 0.0f;
        bool powerAttackWaiting = false;
        
};