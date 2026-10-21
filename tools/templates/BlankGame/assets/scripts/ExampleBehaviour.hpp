#pragma once
#include <Behaviour.h>

class ExampleBehaviour final : public Behaviour
{
public:
    void Update(float dt) override;
};

#include <Transform.h>
#include <GameObject.h>
#include <cmath>

inline void ExampleBehaviour::Update(float dt)
{
    (void)dt;
    // Your gameplay code belongs in assets/scripts/.
    // Example: auto* tr = gameObject->GetComponent<Transform>();
}
