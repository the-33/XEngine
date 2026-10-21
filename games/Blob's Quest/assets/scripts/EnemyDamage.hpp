#pragma once
#include <Behaviour.h>
class EnemyDamage final : public Behaviour
{
    int lives = 2;
public:
    void Damage();
};

#include <XEngine.h>
inline void EnemyDamage::Damage()
{
    --lives;
    if (lives <= 0) {
        if (auto* sfx = Assets->GetSFXByKey("enemy_die")) Sound->PlaySFX(sfx);
        gameObject->Destroy();
    } else {
        if (auto* sfx = Assets->GetSFXByKey("enemy_hit")) Sound->PlaySFX(sfx);
    }
}
