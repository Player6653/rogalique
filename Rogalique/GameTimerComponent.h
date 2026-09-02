#pragma once
#include "IComponent.h"
#include <SFML/System/Time.hpp>

// Секундомер текущего забега — считает время, пока идёт активный геймплей (не на паузе, не после смерти/победы,
// см. update() в .cpp), останавливается ровно в момент победы (см. GameWorld::isVictory) и дальше не идёт, пока
// висит экран победы. reset() обнуляет — общий хвост тех же обработчиков меню, что сбрасывают весь остальной
// ростер ("Начать"/"Играть заново"/"В главное меню" и т.п., см. SceneFacade.cpp). Живёт в UI-дереве (как и
// остальной HUD, см. HudTextComponent), поэтому тикает, даже когда мировое дерево (m_root) на паузе — иначе на
// экране паузы таймер сам не остановился бы вовремя (m_root просто не обновлялся бы, но объект в uiRoot — да).
class GameTimerComponent : public IComponent {
public:
    void update(sf::Time dt) override;
    void reset() override;

    sf::Time getElapsed() const
    {
        return m_elapsed;
    }

private:
    sf::Time m_elapsed;
    bool m_stopped = false;
};
