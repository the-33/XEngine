#include "XEngine.h"

float gSpeedMultiplier = 1.f;
float gScore = 0.f;

class PrincessController : public Behaviour
{
public:
    bool isOnGround = false;
    bool isDead = false;

private:
    // --- Ajustes ---
    float jumpImpulse = 15.f;        // impulso inicial (VelocityChange)
    float holdBoostPerSec = 20.f;    // impulso adicional mientras se mantiene pulsado (VelocityChange por segundo)
    float maxHoldTime = 0.18f;       // tiempo máximo de mantenimiento
    float jumpCutMultiplier = 0.45f; // al soltar pronto: recorta vy (0..1)

    // --- Estado ---
    bool  isJumping = false;
    float holdTimer = 0.f;

    bool JumpPressed_() const
    {
		if (isDead) return false;
        auto& input = InputManager::GetInstance();
        return input.KeyPressed(SDL_SCANCODE_W)
            || input.KeyPressed(SDL_SCANCODE_UP)
            || input.KeyPressed(SDL_SCANCODE_SPACE);
    }

    bool JumpHeld_() const
    {
		if (isDead) return false;
        auto& input = InputManager::GetInstance();
        return input.KeyDown(SDL_SCANCODE_W)
            || input.KeyDown(SDL_SCANCODE_UP)
            || input.KeyDown(SDL_SCANCODE_SPACE);
    }

    bool JumpReleased_() const
    {
		if (isDead) return true;
        auto& input = InputManager::GetInstance();
        return input.KeyReleased(SDL_SCANCODE_W)
            || input.KeyReleased(SDL_SCANCODE_UP)
            || input.KeyReleased(SDL_SCANCODE_SPACE);
    }

    void Update(float dt) override
    {
        auto* rb = gameObject->GetComponent<RigidBody2D>();
        if (!rb) return;

        // 1) Inicio del salto (solo flanco)
        if (JumpPressed_() && isOnGround)
        {
            rb->AddForce(Vec2(0.f, -jumpImpulse), RigidBody2D::ForceMode::VelocityChange);

            isOnGround = false;
            isJumping = true;
            holdTimer = 0.f;
        }

        // 2) Mientras se mantiene pulsado: empuje extra (limitado)
        if (isJumping && JumpHeld_())
        {
            holdTimer += dt;
            if (holdTimer < maxHoldTime)
            {
                // VelocityChange por segundo -> multiplicamos por dt
                rb->AddForce(Vec2(0.f, -holdBoostPerSec * dt), RigidBody2D::ForceMode::VelocityChange);
            }
        }

        // 3) Si se suelta pronto: “jump cut”
        if (isJumping && JumpReleased_())
        {
            Vec2 v = rb->velocity;

            // Si aún va hacia arriba (con la gravedad +Y, subir = vy < 0)
            if (v.y < 0.f)
            {
                v.y *= jumpCutMultiplier; // menos negativo => sube menos
                rb->velocity = v;
            }

            isJumping = false;
        }

        // 4) Si ya empieza a caer, se acabó el “hold”
        if (isJumping && rb->velocity->y > 0.f)
            isJumping = false;
    }

    void OnCollisionEnter(const CollisionInfo2D& info) override
    {
        if (info.other && info.other->name == "ground")
        {
            isOnGround = true;
            isJumping = false;
            holdTimer = 0.f;
        }
    }

    void OnCollisionExit(const CollisionInfo2D& info) override
    {
        if (info.other && info.other->name == "ground")
        {
            isOnGround = false;
        }
    }

    void OnTriggerEnter(const CollisionInfo2D& info) override
    {
        if (info.other && info.other->tag == "cactus" && !isDead)
        {
            isDead = true;
			gSpeedMultiplier = 0.f;
        }
	}
};

class PrincessAnimator : public Behaviour
{
	Texture* runSpritesheet = nullptr;
	Texture* jumpSpritesheet = nullptr;
	Texture* fallSpritesheet = nullptr;
	Texture* dieSpritesheet = nullptr;

    Texture* currentSheet = nullptr;

    int runFrameCount = 8;
    int jumpFrameCount = 3;
    int fallFrameCount = 3;
    int dieFrameCount = 11;

	float runFrameDuration = 0.05f;
	float jumpFrameDuration = 0.2f;
	float fallFrameDuration = 0.2f;
	float dieFrameDuration = 0.05f;

	int currentFrame = 0;
	int frameCount = 0; 
	float frameDuration = 0.6f;
	float frameTimer = 0.f;

    SpriteRenderer* sr;
	RigidBody2D* rb;

    void UpdateSprite()
    {
        sr->sprite = currentSheet;
        int frameWidth = currentSheet->Width() / frameCount;
        sr->source = RectI{ currentFrame * frameWidth, 0, frameWidth, currentSheet->Height() };
    }

    void Start() override
    {
        runSpritesheet = AssetManager::GetInstance().GetTextureByKey("princess-run");
        jumpSpritesheet = AssetManager::GetInstance().GetTextureByKey("princess-jump");
        fallSpritesheet = AssetManager::GetInstance().GetTextureByKey("princess-fall");
		dieSpritesheet = AssetManager::GetInstance().GetTextureByKey("princess-die");

		sr = gameObject->GetComponent<SpriteRenderer>();
		rb = gameObject->GetComponent<RigidBody2D>();

        currentSheet = runSpritesheet;
        frameCount = runFrameCount;
        frameDuration = runFrameDuration;
		frameTimer = frameDuration;

		UpdateSprite();
    }

    void Update(float dt) override
    {
        if (!rb || !sr) return;
        float vy = rb->velocity->y;

		bool isdead = gameObject->GetComponent<PrincessController>()->isDead;

        if (isdead)
        {
            if (currentSheet != dieSpritesheet) currentFrame = 0;
            currentSheet = dieSpritesheet;
            frameCount = dieFrameCount;
            frameDuration = dieFrameDuration;
		}
        else if (gameObject->GetComponent<PrincessController>()->isOnGround)
        {
            if (currentSheet != runSpritesheet) currentFrame = 0;
            currentSheet = runSpritesheet;
            frameCount = runFrameCount;
            frameDuration = runFrameDuration;
        }
        else
        {
            if (vy > 0.1f)
            {
                if (currentSheet != jumpSpritesheet) currentFrame = 0;
                currentSheet = jumpSpritesheet;
                frameCount = jumpFrameCount;
                frameDuration = jumpFrameDuration;
            }
            else if (vy < -0.1f)
            {
                if (currentSheet != fallSpritesheet) currentFrame = 0;
                currentSheet = fallSpritesheet;
                frameCount = fallFrameCount;
                frameDuration = fallFrameDuration;
            }
        }

        // Actualiza el frame
        if (frameTimer >= 0.f) frameTimer -= (dt * ((currentSheet == runSpritesheet && gSpeedMultiplier != 0.f) ? gSpeedMultiplier : 1.f));
        else
        {
            currentFrame++;
            if (currentFrame >= frameCount)
            {
                if (!isdead) currentFrame = 0;
                else currentFrame = frameCount - 1;
            }

			frameTimer = frameDuration;
        }

        UpdateSprite();
	}
};

InstanceBuilder princessPrefab = [](GameObject& princess, Scene& scene){
    auto* princessTex = AssetManager::GetInstance().GetTextureByKey("princess-run");

    princess.AddComponent<SpriteRenderer>()->sprite = princessTex;
	princess.GetComponent<SpriteRenderer>()->pivot01 = Vec2(0.5f, 1.f);

    float ppu = princessTex->PixelsPerUnit();
    float wUnits = (princessTex->Width()/8) / ppu;
    float hUnits = princessTex->Height() / ppu;

    princess.transform->scale = Vec2(1.0f / hUnits, 1.0f / hUnits) * 2.5f;

    princess.AddComponent<Collider2D>();
    princess.GetComponent<Collider2D>()->shape = Collider2D::Shape::Box;
    princess.GetComponent<Collider2D>()->size = Vec2(.3f * wUnits, 0.5f * hUnits);
    princess.GetComponent<Collider2D>()->localOffset = Vec2(0.f, -.14f);

	princess.AddComponent<RigidBody2D>()->constraints = RigidBody2D::Constraints::FreezePosX | RigidBody2D::Constraints::FreezeRot;
	princess.GetComponent<RigidBody2D>()->mass = 1.0f;
	princess.GetComponent<RigidBody2D>()->gravityScale = 4.0f;
	princess.GetComponent<RigidBody2D>()->restitution = 0.0f;
	princess.GetComponent<RigidBody2D>()->collisionDetection = RigidBody2D::CollisionDetection::Continuous;
	princess.AddComponent<PrincessController>();
	princess.AddComponent<PrincessAnimator>();
};

float gScreenBorderLeftWorld;
float gScreenBorderRightWorld;

class CactusController : public Behaviour
{
    float speed = 10.f;

    void Update(float dt) override
    {
        gameObject->transform->position -= Vec3::Right() * speed * gSpeedMultiplier * dt;
        if (gameObject->transform->position->x < gScreenBorderLeftWorld - 5.f)
        {
			gameObject->Destroy();
        }
	}
};

class CactusSpawner : public Behaviour
{
public:
    InstanceBuilder cactusPrefab1;
	InstanceBuilder cactusPrefab2;
	InstanceBuilder cactusPrefab3;

private:
    float spawnTimer = 0.f;
    float minSpawnInterval = .5f;
    float maxSpawnInterval = 2.f;

	float maxSpeedMultiplier = 3.f;
	float speedIncreasePerSecond = 0.01f;

    void Update(float dt) override
    {
        spawnTimer -= dt;
        if (spawnTimer <= 0.f && gSpeedMultiplier > 0.f)
        {
            spawnTimer = Random->Range(minSpawnInterval, maxSpawnInterval);
            int choice = Random->Range(0, 3);
            InstanceBuilder* prefab = nullptr;
            if (choice == 0) prefab = &cactusPrefab1;
            else if (choice == 1) prefab = &cactusPrefab2;
            else prefab = &cactusPrefab3;
            gameObject->scene->Instantiate("cactus", *prefab, Vec3(gScreenBorderRightWorld + 2.f, 3.f, 0.f))->tag = "cactus";
        }

        if (gSpeedMultiplier > 0.f)
		{
			gSpeedMultiplier += speedIncreasePerSecond * dt;
			gSpeedMultiplier = std::min(gSpeedMultiplier, maxSpeedMultiplier);

			gScore += 10.f * gSpeedMultiplier * dt;
		}
	}
};

InstanceBuilder cactusPrefab1 = [](GameObject& cactus, Scene& scene) {
    auto* tex = AssetManager::GetInstance().GetTextureByKey("cactus-1");
    cactus.AddComponent<SpriteRenderer>()->sprite = tex;
	cactus.GetComponent<SpriteRenderer>()->pivot01 = Vec2(0.5f, 1.f);
    float ppu = tex->PixelsPerUnit();
    float wUnits = tex->Width() / ppu;
    float hUnits = tex->Height() / ppu;
	float randomScale = Random->Range(1.f, 1.5f);
    cactus.transform->scale = { randomScale / hUnits, randomScale / hUnits };
    cactus.transform->position = {0.f, 3.f, 0.f};
    cactus.AddComponent<Collider2D>()->shape = Collider2D::Shape::Box;
    cactus.GetComponent<Collider2D>()->size = Vec2(0.9f * wUnits, 0.9f * hUnits);
    cactus.GetComponent<Collider2D>()->localOffset = Vec2(0.f, -.09f);
    cactus.GetComponent<Collider2D>()->isTrigger = true;
	cactus.AddComponent<CactusController>();
	};

InstanceBuilder cactusPrefab2 = [](GameObject& cactus, Scene& scene) {
    auto* tex = AssetManager::GetInstance().GetTextureByKey("cactus-2");
    cactus.AddComponent<SpriteRenderer>()->sprite = tex;
	cactus.GetComponent<SpriteRenderer>()->pivot01 = Vec2(0.5f, 1.f);
    float ppu = tex->PixelsPerUnit();
    float wUnits = tex->Width() / ppu;
    float hUnits = tex->Height() / ppu;
    float randomScale = Random->Range(1.f, 1.5f);
	cactus.transform->position = { 0.f, 3.f, 0.f };
    cactus.transform->scale = { randomScale / hUnits, randomScale / hUnits };
    cactus.AddComponent<Collider2D>()->shape = Collider2D::Shape::Box;
    cactus.GetComponent<Collider2D>()->size = Vec2(0.9f * wUnits, 0.9f * hUnits);
    cactus.GetComponent<Collider2D>()->localOffset = Vec2(0.f, -.07f);
    cactus.GetComponent<Collider2D>()->isTrigger = true;
    cactus.AddComponent<CactusController>();
	};

InstanceBuilder cactusPrefab3 = [](GameObject& cactus, Scene& scene) {
    auto* tex = AssetManager::GetInstance().GetTextureByKey("cactus-3");
    cactus.AddComponent<SpriteRenderer>()->sprite = tex;
	cactus.GetComponent<SpriteRenderer>()->pivot01 = Vec2(0.5f, 1.f);
    float ppu = tex->PixelsPerUnit();
    float wUnits = tex->Width() / ppu;
    float hUnits = tex->Height() / ppu;
    float randomScale = Random->Range(1.f, 1.5f);
	cactus.transform->position = { 0.f, 3.f, 0.f };
    cactus.transform->scale = { randomScale / hUnits, randomScale / hUnits };
    cactus.AddComponent<Collider2D>()->shape = Collider2D::Shape::Box;
    cactus.GetComponent<Collider2D>()->size = Vec2(0.9f * wUnits, 0.9f * hUnits);
    cactus.GetComponent<Collider2D>()->localOffset = Vec2(0.f, -.09f);
    cactus.GetComponent<Collider2D>()->isTrigger = true;
    cactus.AddComponent<CactusController>();
	};

class ObjectThreadmill : public Behaviour
{
public:
	InstanceBuilder prefab;
	float speed = 2.f;
    float objectsWidth = 1.f;
    std::string name = "threadmill_obj";
private:
    float screenBorderLeftWorld;
	float screenBorderRightWorld;

	float firstObjectPosX = 0.f;

	std::vector<GameObject*> objects;

    void Start() override
    {
        auto screenWidth = Window->GetDrawableSize().x;

        this->screenBorderLeftWorld = gScreenBorderLeftWorld;
        this->screenBorderRightWorld = gScreenBorderRightWorld;

        firstObjectPosX = screenBorderRightWorld;
		float x = firstObjectPosX;

        while(x > screenBorderLeftWorld - objectsWidth)
        {
			GameObject* obj = gameObject->scene->Instantiate(name, prefab, Vec3(x, gameObject->transform->position->y, gameObject->transform->position->z));
            auto* sr = obj->GetComponent<SpriteRenderer>();
            if (sr && sr->sprite)
            {
                float ppu = sr->sprite->PixelsPerUnit();
                float wUnits = sr->sprite->Width() / ppu;         // ancho en unidades "sin escala"
                float realWidthWorld = wUnits * obj->transform->scale->x; // ancho real en mundo
                objectsWidth = realWidthWorld;
                sr->pivot01 = Vec2(0.f, 0.f);
            }
            objects.push_back(obj);
            x -= objectsWidth;
        }
    }

    void Update(float dt) override
    {
        const float gap = 0.0f;
        const float step = objectsWidth - gap;

        // 1) Mover todos
        for (auto* obj : objects)
            obj->transform->position -= Vec3::Right() * speed * gSpeedMultiplier * dt;

        // 2) Encontrar el más a la derecha (maxX)
        float maxX = -1e30f;
        for (auto* obj : objects)
            maxX = std::max(maxX, obj->transform->position->x);

        // 3) Wrap: si se sale por la izquierda, lo pongo a la derecha del maxX
        for (auto* obj : objects)
        {
            if (obj->transform->position->x < screenBorderLeftWorld - objectsWidth)
            {
                obj->transform->position = Vec3(maxX + step, obj->transform->position->y, obj->transform->position->z);
                maxX = obj->transform->position->x; // por si se salen varios en el mismo frame
            }
        }
    }
};

InstanceBuilder groundPrefab = [](GameObject& ground, Scene& scene) {
    auto* groundTex = AssetManager::GetInstance().GetTextureByKey("ground");
    ground.AddComponent<SpriteRenderer>()->sprite = groundTex;
    float ppu = groundTex->PixelsPerUnit();
    float wUnits = groundTex->Width() / ppu;
    float hUnits = groundTex->Height() / ppu;
    ground.transform->scale = { 3.f / hUnits, 3.f / hUnits };
    };

InstanceBuilder skyPrefab = [](GameObject& sky, Scene& scene) {
    auto* skyTex = AssetManager::GetInstance().GetTextureByKey("sky");
    sky.AddComponent<SpriteRenderer>()->sprite = skyTex;
    float ppu = skyTex->PixelsPerUnit();
    float wUnits = skyTex->Width() / ppu;
    float hUnits = skyTex->Height() / ppu;
    sky.transform->scale = { 8.f / hUnits, 8.f / hUnits };
    };

InstanceBuilder bigRockPrefab = [](GameObject& rock, Scene& scene) {
    auto* rockTex = AssetManager::GetInstance().GetTextureByKey("rock-big");
    rock.AddComponent<SpriteRenderer>()->sprite = rockTex;
    float ppu = rockTex->PixelsPerUnit();
    float wUnits = rockTex->Width() / ppu;
    float hUnits = rockTex->Height() / ppu;
    rock.transform->scale = { 2.f / hUnits, 2.f / hUnits };
    };

InstanceBuilder smallRockPrefab = [](GameObject& rock, Scene& scene) {
    auto* rockTex = AssetManager::GetInstance().GetTextureByKey("rock-small");
    rock.AddComponent<SpriteRenderer>()->sprite = rockTex;
    float ppu = rockTex->PixelsPerUnit();
    float wUnits = rockTex->Width() / ppu;
    float hUnits = rockTex->Height() / ppu;
    rock.transform->scale = { 1.5f / hUnits, 1.5f / hUnits };
    };

InstanceBuilder cloud1Prefab = [](GameObject& cloud, Scene& scene) {
    auto* tex = AssetManager::GetInstance().GetTextureByKey("cloud-big");
    cloud.AddComponent<SpriteRenderer>()->sprite = tex;

    float ppu = tex->PixelsPerUnit();
    float hUnits = tex->Height() / ppu;

    constexpr float targetH = 1.2f;
    cloud.transform->scale = { targetH / hUnits, targetH / hUnits };
    };

InstanceBuilder cloud2Prefab = [](GameObject& cloud, Scene& scene) {
    auto* tex = AssetManager::GetInstance().GetTextureByKey("cloud-small");
    cloud.AddComponent<SpriteRenderer>()->sprite = tex;

    float ppu = tex->PixelsPerUnit();
    float hUnits = tex->Height() / ppu;

    constexpr float targetH = 0.9f;
    cloud.transform->scale = { targetH / hUnits, targetH / hUnits };
    };

class HUDController : public Behaviour
{
    void Render() override
    {
        if (gSpeedMultiplier > 0.f)
        {
            UI->Label("Score: " + std::to_string(static_cast<int>(gScore)), 10.f, 10.f);
        }
        else
        {
            UI->Label("Score: " + std::to_string(static_cast<int>(gScore)),
                { ((float)Window->GetDrawableSize().x / 2) - (50.f / 2.f), ((float)Window->GetDrawableSize().y / 2) - 80.f, 50.f, 50.f },
                *Assets->GetFontByKey("ui_default_big"),
                Color::DarkGray(),
				AlignH::Center,
				AlignV::Middle,
				0.f
            );

            bool clicked = UI->ImageButton(
                "ui-button",
                { ((float)Window->GetDrawableSize().x / 2) - (50.f / 2.f), ((float)Window->GetDrawableSize().y / 2) - (50.f / 2.f), 50.f, 50.f },
                *Assets->GetTextureByKey("ui-button")
            );

            if (clicked)
            {
                Scenes->SetActive("level1");
                gSpeedMultiplier = 1.f;
				gScore = 0.f;
            }
        }
    }
};

void InitGame(Engine& engine)
{
	Assets->LoadTexture("textures/ground.png", "ground");
	Assets->LoadTexture("textures/princess-run.png", "princess-run");
	Assets->LoadTexture("textures/princess-jump.png", "princess-jump");
	Assets->LoadTexture("textures/princess-fall.png", "princess-fall");
	Assets->LoadTexture("textures/princess-die.png", "princess-die");
	Assets->LoadTexture("textures/sky.png", "sky");
	Assets->LoadTexture("textures/back-1.png", "rock-big");
	Assets->LoadTexture("textures/back-2.png", "rock-small");
	Assets->LoadTexture("textures/cloud-1.png", "cloud-big");
	Assets->LoadTexture("textures/cloud-2.png", "cloud-small");
	Assets->LoadTexture("textures/cactus-1.png", "cactus-1");
	Assets->LoadTexture("textures/cactus-2.png", "cactus-2");
	Assets->LoadTexture("textures/cactus-3.png", "cactus-3");
	Assets->LoadTexture("textures/ui-btn-down.png", "ui-button");
	Assets->LoadFont("fonts/ui-font-default.ttf", "ui_default_big", 50);

    Scenes->Register("level1", [](Scene* scn) {

        scn->camera->center = Vec2(0.f, 0.f);

        auto screenWidth = Window->GetDrawableSize().x;

        gScreenBorderLeftWorld = scn->camera->ScreenToWorld(Vec2(0.f, 0.f)).x;
        gScreenBorderRightWorld = scn->camera->ScreenToWorld(Vec2(screenWidth, 0.f)).x;

        // Princesa
        {
            GameObject* princess = scn->Instantiate("princess", princessPrefab, Vec3(-6.f, 0.f, 2.f));
        }

        {
            GameObject* groundThreadmill = scn->CreateObject("ground_threadmill");
            groundThreadmill->transform->position = Vec3(0.f, 3.f, 0.f);
            auto* groundTM = groundThreadmill->AddComponent<ObjectThreadmill>();
            groundTM->prefab = groundPrefab;
            groundTM->speed = 10.f;
        }
		
        {
            GameObject* groundCollision = scn->CreateObject("ground");
            groundCollision->transform->position = Vec3(0.f, 3.f, 0.f);
            groundCollision->transform->scale = Vec2(math::Abs(gScreenBorderRightWorld - gScreenBorderLeftWorld), 1.f);
            groundCollision->AddComponent<Collider2D>()->localOffset = Vec2(0.f, .5f);
            auto* rb = groundCollision->AddComponent<RigidBody2D>();
            rb->bodyType = RigidBody2D::BodyType::Static;
        }

        {
            GameObject* skyThreadmill = scn->CreateObject("sky_threadmill");
            skyThreadmill->transform->position = Vec3(0.f, -5.7f, -1.f);
            auto* skyTM = skyThreadmill->AddComponent<ObjectThreadmill>();
            skyTM->prefab = skyPrefab;
            skyTM->speed = 1.f;
        }

        {
            GameObject* rockThreadmill = scn->CreateObject("rock_threadmill");
            rockThreadmill->transform->position = Vec3(0.f, 1.f, -0.5f);
            auto* rockTM = rockThreadmill->AddComponent<ObjectThreadmill>();
            rockTM->prefab = bigRockPrefab;
            rockTM->speed = 6.f;
        }

        {
            GameObject* rockThreadmill = scn->CreateObject("rock_threadmill");
            rockThreadmill->transform->position = Vec3(0.f, 1.5f, -0.5f);
            auto* rockTM = rockThreadmill->AddComponent<ObjectThreadmill>();
            rockTM->prefab = smallRockPrefab;
            rockTM->speed = 9.f;
        }

        {
            GameObject* cloudTMObj = scn->CreateObject("cloud_threadmill");
            cloudTMObj->transform->position = Vec3(0.f, -1.0f, -1.0f);
            auto* tm = cloudTMObj->AddComponent<ObjectThreadmill>();
            tm->prefab = cloud1Prefab;
            tm->speed = 2.f;
        }

        {
            GameObject* cloudTMObj = scn->CreateObject("cloud_threadmill");
            cloudTMObj->transform->position = Vec3(0.f, -0.3f, -0.8f);
            auto* tm = cloudTMObj->AddComponent<ObjectThreadmill>();
            tm->prefab = cloud2Prefab;
            tm->speed = 3.0f;
        }

        {
            GameObject* cactusSpawner = scn->CreateObject("cactus_spawner");
            auto* spawner = cactusSpawner->AddComponent<CactusSpawner>();
            spawner->cactusPrefab1 = cactusPrefab1;
            spawner->cactusPrefab2 = cactusPrefab2;
			spawner->cactusPrefab3 = cactusPrefab3;
        }

        {
            GameObject* hud = scn->CreateObject("HUD");
            hud->AddComponent<HUDController>();
		}
    });

	Scenes->SetActive("level1");
}

int main(int, char**)
{
    if (!Engine::Start("../../../XEngine_CONFIG.json"))
        return -1;

    Engine::SetInitCallback(InitGame);

    Engine::SetUpdateCallback([](float dt)
    {
            
    });

    int returnValue = Engine::Run();

    return returnValue;
}