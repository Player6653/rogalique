#pragma once

// Превращает "зажато сейчас" в "только что нажали" (фронт, а не каждый кадр, пока держат) — тот же код раньше
// был скопирован в каждый компонент отдельно (InputComponent, PauseToggleComponent, MenuOverlayComponent,
// CreditsOverlayComponent, SettingsOverlayComponent, PlayerAttackComponent), с риском разъехаться при правке.
// Не привязан к SFML/клавиатуре нарочно — "зажато сейчас" вызывающий код получает откуда угодно (клавиша,
// кнопка мыши, их комбинация через ||).
class KeyEdge {
public:
    // Вызывать каждый кадр, пока компонент активно слушает этот ввод. true — ровно на кадре перехода
    // не-зажато -> зажато.
    bool poll(bool heldNow)
    {
        bool pressed = heldNow && !m_heldLastFrame;
        m_heldLastFrame = heldNow;
        return pressed;
    }

    // Обновляет "было зажато" без вычисления фронта — нужно звать, пока компонент НЕ обрабатывает ввод (невидим,
    // под модальным оверлеем и т.п.). Иначе клавиша, отпущенная за это время, читалась бы как "только что нажали"
    // в первом кадре, когда компонент снова начинает слушать ввод (см. баг в MenuOverlayComponent, который эта
    // синхронизация и чинит).
    void sync(bool heldNow)
    {
        m_heldLastFrame = heldNow;
    }

private:
    bool m_heldLastFrame = false;
};
