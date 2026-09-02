#include "Player.h"
#include "AttackComponent.h"
#include "CameraComponent.h"
#include "ChaseTargetComponent.h"
#include "ColliderComponent.h"
#include "GameException.h"
#include "HealthComponent.h"
#include "HitFlashComponent.h"
#include "InputComponent.h"
#include "InventoryComponent.h"
#include "Log.h"
#include "LowHealthPulseComponent.h"
#include "MovementComponent.h"
#include "PlayerAnimationComponent.h"
#include "PlayerAttackComponent.h"
#include "PlayerDustTrailComponent.h"
#include "RangedAttackComponent.h"
#include "SpriteComponent.h"
#include "WeaponComponent.h"

namespace
{
    // Видимый размер спрайта нарочно больше коллайдера (size) — коллайдер остаётся маленьким для точных столкновений в узких проходах, а персонаж на экране не выглядит игрушечно мелким.
    // 96x128 — два тайла по ширине, высота по пропорции текстуры (48x64). Тень использует те же 48x64 нативного кадра, поэтому берёт тот же видимый размер, что и тело.
    const sf::Vector2f VISUAL_SIZE(96.f, 128.f);

    // 4, а не круглое число просто так у рамки Bar_C ровно 4 родных деления на кайме, при maxHp=4 сегменты HUD ложатся точно на них.
    constexpr int PLAYER_MAX_HP = 4;
    constexpr int PLAYER_ARMOR = 0;

    constexpr int PLAYER_ATTACK_DAMAGE = 2;
    // 70, не 56 — с коллайдером босса 84x84 (см. Boss.cpp/SceneFacade.cpp) минимальная физическая дистанция до
    // центра босса ~58px (половины коллайдеров 16+42), 56 почти не давало запаса и удар по боссу ощущался
    // "непопадающим" при малейшем движении. 70 даёт ~12px запаса вместо прежних ~8, и заодно
    // делает удар чуть более прощающим по обычным ботам (у тех коллайдер меньше, запас был и остаётся с большим
    // избытком).
    constexpr float PLAYER_ATTACK_RANGE = 70.f;
    const sf::Time PLAYER_ATTACK_COOLDOWN = sf::seconds(0.5f);

    // i-frames после удара — иначе орк и лучник, оказавшись рядом одновременно, могут снять HP одной пачкой
    // почти слипшихся ударов за доли секунды, что ощущается нечестно.
    const sf::Time PLAYER_HIT_INVULNERABILITY = sf::seconds(0.6f);

    // Player::LOW_HP_THRESHOLD/LOW_HP_PULSE_PERIOD (см. Player.h) — пульсация тела на "мало здоровья" (см.
    // LowHealthPulseComponent ниже) и виньетка экрана (LowHealthScreenFlashComponent, см. SceneFacade.cpp) читают
    // один и тот же порог/период оттуда, а не две независимые копии — раньше SceneFacade держал свой хардкод-
    // литерал, рассинхрон периода между телом и экраном уже был багом (см. аудит дублирования кода).

    // Пистолет — второе оружие (переключение по Q, см. WeaponComponent). minRange=0 — можно стрелять в упор,
    // конкурировать с копьём за дистанцию тут не с чем, оба оружия не активны одновременно.
    constexpr int GUN_DAMAGE = 2;
    constexpr float GUN_MIN_RANGE = 0.f;
    constexpr float GUN_MAX_RANGE = 420.f;
    const sf::Time GUN_COOLDOWN = sf::seconds(0.35f);
    constexpr float BULLET_SPEED = 520.f;
    constexpr float BULLET_HIT_RADIUS = 14.f;
    // Своей текстуры пули пока нет — Projectile рисует цветной плейсхолдер-прямоугольник, вытянутый по полёту.
    const sf::Vector2f BULLET_VISUAL_SIZE(16.f, 5.f);
    // 1 — арбалет (см. ItemDefinition.cpp "crossbow", единственный способ получить Weapon::Gun), не многозарядный
    // пистолет: один болт, потом обязательная перезарядка (R). GUN_RESERVE_AMMO — сколько болтов всего на забег,
    // расходуются по одному за перезарядку; когда кончатся, R перестаёт работать вовсе.
    constexpr int GUN_MAGAZINE_SIZE = 1;
    constexpr int GUN_RESERVE_AMMO = 10;
    const sf::Time GUN_RELOAD_DURATION = sf::seconds(1.f);
} // namespace

// См. класс-комментарий у Player::LOW_HP_PULSE_PERIOD в Player.h — sf::Time в SFML 2.5.1 не constexpr-совместим,
// поэтому определение статического члена, в отличие от LOW_HP_THRESHOLD, вынесено сюда.
const sf::Time Player::LOW_HP_PULSE_PERIOD = sf::seconds(1.4f);

Player::Player(sf::Vector2f position, sf::Vector2f size, sf::Vector2f cameraViewSize, float speed)
    : GameObject(position)
{
    // Порядок важен компоненты рисуются и обновляются в порядке добавления.
    // Камера должна применить себя к окну (в draw()) раньше, чем нарисуется спрайт.
    addComponent<CameraComponent>(cameraViewSize);
    // Перемещение читает направление у InputComponent, поэтому Input добавляем первым из этой пары.
    InputComponent& input = addComponent<InputComponent>();
    addComponent<MovementComponent>(speed);
    // Тень рисуется первой (значит под телом), тело — вторым поверх неё.
    m_shadowSprite = &addComponent<SpriteComponent>(VISUAL_SIZE);
    m_shadowSprite->setPlaceholderColor(sf::Color(0, 0, 0, 90));
    m_bodySprite = &addComponent<SpriteComponent>(VISUAL_SIZE);
    m_bodySprite->setPlaceholderColor(sf::Color::Green);
    // Пыль из-под ног на рывок/прыжок — третий слой поверх тела, PlayerAnimationComponent сам решает, когда его показывать.
    // SpriteComponent всегда что-то рисует (текстуру или плейсхолдер), явного "скрыт" у него нет — прячем через
    // альфу 0 (setColor), пока PlayerAnimationComponent не откроет её обратно на время рывка/прыжка.
    m_dustSprite = &addComponent<SpriteComponent>(VISUAL_SIZE);
    m_dustSprite->setColor(sf::Color(255, 255, 255, 0));
    // Игрок подвижен (не кинематический) кинематические соседи (стены) его останавливают.
    addComponent<ColliderComponent>(size, false);
    // Метка для ChaseComponent врага — по ней он находит игрока сам, без прямой передачи ссылки.
    addComponent<ChaseTargetComponent>();

    // HealthComponent сам проверяет maxHp/armor и кидает GameException на некорректные значения — ловим здесь и откатываемся на безопасные дефолты, чтобы одна опечатка в константах не роняла всю игру.
    HealthComponent* health = nullptr;
    try {
        health = &addComponent<HealthComponent>(PLAYER_MAX_HP, PLAYER_ARMOR, PLAYER_HIT_INVULNERABILITY);
    } catch (const GameException& e) {
        LOG_ERROR(std::string("Player: некорректные HP/броня, использую значения по умолчанию (1/0): ") + e.what());
        health = &addComponent<HealthComponent>(1, 0, PLAYER_HIT_INVULNERABILITY);
    }
    // Упрощение сложности — см. HealthComponent::setLastStandEnabled(): удар, который снёс бы
    // HP с >1 до 0, вместо этого оставляет 1 HP.
    health->setLastStandEnabled(true);

    // Мешок + экипировка (см. InventoryOverlayComponent, открывается по Tab) — базовая броня та же, что и у
    // HealthComponent выше, дальше InventoryComponent сам добавляет к ней бонус от надетой экипировки.
    addComponent<InventoryComponent>(PLAYER_ARMOR);

    // Копьё (ближний бой) и пистолет (дальний) — оба пассивны (autoAttack/autoFire=false), WeaponComponent сам
    // решает, какой из них дёргать по нажатию F/Z, в зависимости от того, что сейчас в руках (см. switchWeapon()).
    // requireTarget=false у обоих — игрок бьёт/стреляет по нажатию всегда, даже без цели поблизости (мимо), в
    // отличие от ботов (у тех requireTarget=true по умолчанию, незачем автоатаке махать в пустоту).
    AttackComponent& spearAttack = addComponent<AttackComponent>(
        "Player-Spear", PLAYER_ATTACK_DAMAGE, PLAYER_ATTACK_RANGE, PLAYER_ATTACK_COOLDOWN, false, sf::Time::Zero, nullptr, false);
    RangedAttackComponent& gunAttack = addComponent<RangedAttackComponent>("Player-Gun", GUN_DAMAGE, GUN_MIN_RANGE, GUN_MAX_RANGE,
        GUN_COOLDOWN, BULLET_SPEED, BULLET_HIT_RADIUS, "", BULLET_VISUAL_SIZE, sf::Time::Zero, nullptr, false, false);
    addComponent<WeaponComponent>(&spearAttack, &gunAttack, GUN_MAGAZINE_SIZE, GUN_RESERVE_AMMO, GUN_RELOAD_DURATION);
    addComponent<PlayerAttackComponent>();
    // Своей анимации получения урона в паке нет — мигаем телом красным на пару кадров вместо неё.
    addComponent<HitFlashComponent>(*m_bodySprite, sf::seconds(0.3f), sf::seconds(0.06f), sf::Color(255, 60, 60));
    // Дополнительное предупреждение "мало здоровья" (см. LowHealthPulseComponent) — держится, пока HP не выше
    // LOW_HP_THRESHOLD, а не короткой вспышкой на сам удар, как HitFlashComponent выше.
    addComponent<LowHealthPulseComponent>(*health, *m_bodySprite, LOW_HP_THRESHOLD, Player::LOW_HP_PULSE_PERIOD);
    // Пыль под ногами на обычный бег (см. PlayerDustTrailComponent) — в паке анимации для этого нет, поэтому
    // процедурные частицы (ParticleSystem), а не спрайтовый ролик, как у m_dustSprite (рывок/прыжок). Смещение —
    // не половина VISUAL_SIZE (64, край всего бокса спрайта, там уже пустота под ногами), а по факту непрозрачных
    // пикселей кадра Run_Down.png: ноги на y≈41 из 64 при центре кадра 32, при масштабе VISUAL_SIZE.y/64=2 это
    // ~18-20 мировых пикселей вниз от центра — раньше пыль всплывала заметно НИЖЕ персонажа, а не под ним (был баг).
    constexpr float DUST_FEET_OFFSET_Y = 20.f;
    addComponent<PlayerDustTrailComponent>(input, DUST_FEET_OFFSET_Y);

    // Подбирает нужный ролик Idle/Walk/Run/Dash/Jump/Death (и синхронную тень к нему) по направлению, спринту, рывку и здоровью, листает кадры через SpriteComponent.
    addComponent<PlayerAnimationComponent>();

    LOG_INFO("Player создан на позиции (" + std::to_string(position.x) + ", " + std::to_string(position.y) + ")");
}
