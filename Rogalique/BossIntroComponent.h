#pragma once
#include "IComponent.h"

// Метка "только что появился" для босса — GET_READY должен проиграться ровно один раз, в первый же кадр после
// спавна (см. Boss.cpp: consumeJustTriggered() заведён в config.attacks той же цепочкой, что и melee/ranged/bite/
// aoe — приоритет выше всех остальных, потому что "просыпается" логически раньше, чем успевает ударить). Не таймер
// и не блокирует реальные действия босса (см. класс-комментарий Boss.h про упрощение — GET_READY чисто
// косметический, ~0.7с, шанс словить удар в эти доли секунды и так небольшой, полноценная заморозка ИИ ради этого
// не заводилась).
class BossIntroComponent : public IComponent {
public:
    bool consumeJustTriggered()
    {
        bool result = !m_consumed;
        m_consumed = true;
        return result;
    }

    void reset() override
    {
        m_consumed = false;
    }

private:
    bool m_consumed = false;
};
