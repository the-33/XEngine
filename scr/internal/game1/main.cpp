#pragma once

#include "XEngine.h"

GameObject* CreateSpriteObject(Scene* scene,
    const std::string& name,
    const Texture* tex,
    Vec3 position,
    Vec2 sizeWorld,
    bool isTrigger,
    LayerBits layer,
    LayerBits mask,
    bool addCollider = true)
{
    auto* obj = scene->CreateObject(name);
    Transform* tr = obj->transform;
    tr->position = position;

    auto* sr = obj->AddComponent<SpriteRenderer>();        // sprite
    sr->sprite = tex;
    sr->source = RectI{ 0, 0, tex->Width(), tex->Height() };

    // Ajusta scale para que la textura tenga sizeWorld
    float ppu = tex->PixelsPerUnit();
    float wUnits = tex->Width() / ppu;
    float hUnits = tex->Height() / ppu;
    Vec2 scale{ sizeWorld.x / wUnits, sizeWorld.y / hUnits };
    tr->localScale = scale;

    if (!addCollider) return obj;

    auto* col = obj->AddComponent<Collider2D>();
    col->shape = Collider2D::Shape::Box;
    col->size = { sizeWorld.x * wUnits, sizeWorld.y * hUnits };
    col->isTrigger = isTrigger;
    col->layer = layer;
    col->mask = mask;
    col->showGizmo = false;
    col->gizmoColor = isTrigger ? Color::Yellow(255) : Color::Green(255);

    return obj;
}

class EnemyDamage : public Behaviour
{
    int lives = 2;
public:

    void Damage()
    {
		lives--;
        if (lives <= 0)
        {
            if (auto* sfx = Assets->GetSFXByKey("enemy_die"))
                Sound->PlaySFX(sfx);
            gameObject->Destroy();
        }
        else
        {
            if (auto* sfx = Assets->GetSFXByKey("enemy_hit"))
                Sound->PlaySFX(sfx);
        }
	}
};


class FireballController : public Behaviour
{
    float speed = 10.f;

    void Update(float dt) override
    {
        transform->position += transform->right * speed * dt;
    }

    void OnTriggerEnter(const CollisionInfo2D& info)
    {
        if (info.other->tag == "wall") gameObject->Destroy();

        if (info.other->tag == "enemy")
        {
			info.other->GetComponent<EnemyDamage>()->Damage();
            gameObject->Destroy();
        }
    }
};

class PlayerController : public Behaviour
{
public:
    float speed = 50.0f; // unidades de mundo / segundo
    Vec2 velocity = Vec2::Zero();
    int lives = 3;
    float shootingTimer = 0.f;
    float shootingCooldown = 1.f;
	InstanceBuilder proyectile;

    void Damage()
    {
        lives--;
        if (lives <= 0)
        {
            Scenes->SetActive("level1");
            if (auto* music = Assets->GetMusicByKey("bg_music"))
            Sound->PlayMusic(music, -1, 0.7f);
        }
    }

    void Start() override
    {
        auto* rb = gameObject->GetComponent<RigidBody2D>();
        rb->gravityScale = .0f;
        rb->linearDamping = 10.f;
        rb->constraints = RigidBody2D::Constraints::FreezeRot;
        rb->collisionDetection = RigidBody2D::CollisionDetection::Continuous;

        proyectile = [](GameObject& proyectile, Scene& scene)
            {
                auto* proyectileTex = Assets->GetTextureByKey("fireball");

                proyectile.AddComponent<SpriteRenderer>()->sprite = proyectileTex;

                float ppu = proyectileTex->PixelsPerUnit();
                float wUnits = proyectileTex->Width() / ppu;
                float hUnits = proyectileTex->Height() / ppu;

                proyectile.transform->scale = { .5f / wUnits, .5f / hUnits };
                proyectile.AddComponent<Collider2D>()->isTrigger = true;
                proyectile.GetComponent<Collider2D>()->radius = (0.7f * wUnits) / 2.5f;
                proyectile.GetComponent<Collider2D>()->shape = Collider2D::Shape::Circle;
                proyectile.AddComponent<FireballController>();
		    };
    }

    void Update(float dt) override
    {
        auto& input = InputManager::GetInstance();   // Singleton 
        auto& time = TimeManager::GetInstance();    // 

        Vec2 dir{ 0,0 };
        if (input.KeyDown(SDL_SCANCODE_W)) dir.y -= 1.f;
        if (input.KeyDown(SDL_SCANCODE_S)) dir.y += 1.f;
        if (input.KeyDown(SDL_SCANCODE_A)) dir.x -= 1.f;
        if (input.KeyDown(SDL_SCANCODE_D)) dir.x += 1.f;

        if (input.KeyPressed(SDL_SCANCODE_SPACE) && shootingTimer <= 0)
        {
            auto* bullet = gameObject->scene->Instantiate("proyectile", proyectile, Vec3(transform->position->x, transform->position->y, -1));
            bullet->transform->LookAt(Input->GetMousePosWorld());
            shootingTimer = shootingCooldown;
        }
        else shootingTimer -= time.deltaTime;

        if (math::LengthSq(dir) > 0.f)
            dir = math::Normalized(dir);

        velocity = dir * speed;
    }

    void FixedUpdate(float) override
    {
        auto* rb = gameObject->GetComponent<RigidBody2D>();
        rb->AddForce(velocity, RigidBody2D::ForceMode::Force);
    }
};

class CameraFollow : public Behaviour
{
public:
    Rect worldBounds{ -21.5f, -20.5f, 43.f, 41.f }; // x,y,w,h en mundo

protected:
    void Update(float dt) override
    {
        Scene* scene = gameObject->scene;
        Camera2D* cam = scene->camera;     // puntero propiedad de Scene 
        if (!cam) return;

        Vec3 pos3 = gameObject->transform->position;
        Vec2 target{ pos3.x, pos3.y };

        cam->Follow(target, 8.f, dt);          // smoothing 8
        cam->ClampToWorld(worldBounds);
    }
};

class HUDController; // lo definimos luego

int gScore;       // marcador global sencillo

InstanceBuilder coinPrefab;

class CoinPickup : public Behaviour
{

public:
    void OnTriggerEnter(const CollisionInfo2D& info) override
    {
        // Queremos que solo cuente si el otro es el Player
        if (info.other->name != "Player")
            return;

        ++gScore;

        auto& assets = AssetManager::GetInstance();
        auto& sound = SoundManager::GetInstance();
        if (auto* sfx = assets.GetSFXByKey("coin"))
            sound.PlaySFX(sfx);        

        gameObject->scene->Instantiate("coin", coinPrefab, Vec3(
            Random->Range(-19.f, 19.f),
            Random->Range(-19.f, 19.f),
            0.f)
        );

        gameObject->Destroy();
    }
};

class EnemyFollow : public Behaviour
{
public:
    float speed = 3.f;

    bool forward = true;
    GameObject* target = nullptr;

    void Start() override
    {
        target = gameObject->scene->Find("Player");
    }

    void Update(float dt) override
    {
        Vec3 targetPos = target->transform->position;
        gameObject->transform->position = math::MoveTowards((Vec3) gameObject->transform->position, targetPos, speed * dt);
    }

    void OnTriggerEnter(const CollisionInfo2D& info) override
    {
        if (info.other->name != "Player")
            return;

        // hit: reset score y sonido
        gScore = 0;
        auto& assets = AssetManager::GetInstance();
        auto& sound = SoundManager::GetInstance();
        if (auto* sfx = assets.GetSFXByKey("hit"))
            sound.PlaySFX(sfx);

        Vec2 forceDirection = (Vec2) math::Normalized(info.other->transform->position - gameObject->transform->position);
        info.other->GetComponent<RigidBody2D>()->AddForce(forceDirection * 20.f, RigidBody2D::ForceMode::Impulse);
        info.other->GetComponent<PlayerController>()->Damage();
    }
};

class EnemySpawner : public Behaviour
{
    float frequence = 5.f;
    float timer = 0.f;

    void Update(float dt) override
    {
        if (timer < frequence)
        {
            timer += dt;
            return;
        }

        timer = 0;

        auto* texEnemy = Assets->GetTextureByKey("enemy");
        GameObject* enemy = CreateSpriteObject(
            gameObject->scene, "Enemy", texEnemy,
            Vec3{ Random->Range(-19.f, 19.f), Random->Range(-19.f, 19.f), 0.f },
            Vec2{ 1.f, 1.f },
            true,
            1u << 3,
            0xFFFFFFFFu
        );
        enemy->AddComponent<EnemyFollow>();
        enemy->AddComponent<EnemyDamage>();
        enemy->GetComponent<Collider2D>()->shape = Collider2D::Shape::Circle;
        enemy->GetComponent<Collider2D>()->radius = enemy->GetComponent<Collider2D>()->size->x / 2.5f;
        enemy->tag = "enemy";
    }
};

Engine* gEnginePtr;   // puntero global al Engine

class HUDController : public Behaviour
{
protected:
    void Update(float) override
    {
        auto& ui = UIManager::GetInstance();
        auto& assets = AssetManager::GetInstance();

        int lives = gameObject->scene->Find("Player")->GetComponent<PlayerController>()->lives;

        // Marcador arriba izquierda (10,10 px)
        ui.ProgressBar(lives / 3.f, Rect(10.f, 10.f, 160.f, 12.f));

        std::string text = "Score: " + std::to_string(gScore);
        ui.Label(text, 11.f, 30.f);
        
        // Botón Restart arriba derecha
        int wWidth = Window->GetDrawableSize().x;

        Rect btnRect{ wWidth - 140.f, 10.f, 120.f, 40.f };
        bool clicked = false;
        clicked = ui.Button("btn_restart", btnRect, "Restart");

        if (clicked && gEnginePtr) {
            Scenes->SetActive("level1");

            if (auto* music = Assets->GetMusicByKey("bg_music"))
                Sound->PlayMusic(music, -1, 0.7f);
            gScore = 0;
        }
    }
};

void InitGame(Engine& engine)
{
    gEnginePtr = &engine;

    // 1) Cargar assets
    auto* assets = Assets;
    assets->LoadTexture("textures/player.png", "player", 100.f);
    assets->LoadTexture("textures/enemy.png", "enemy", 100.f);
    assets->LoadTexture("textures/wall.png", "wall", 100.f);
    assets->LoadTexture("textures/coin.png", "coin", 100.f);
    assets->LoadTexture("textures/floor.png", "floor", 100.f);
    assets->LoadTexture("textures/fireball.png", "fireball", 100.f);

    assets->LoadFont("fonts/ui_font.ttf", "ui_default", 24);

    assets->LoadMusic("audio/music.ogg", "bg_music");
    assets->LoadSFX("audio/coin.wav", "coin");
    assets->LoadSFX("audio/hit.wav", "hit");
    assets->LoadSFX("audio/enemy_hit.wav", "enemy_hit");
    assets->LoadSFX("audio/enemy_die.wav", "enemy_die");

    RenderManager::BackgroundLayer layer;

    layer.mode = RenderManager::BackgroundMode::TileWorldPhysicalScaled;

    Texture* floorTex = Assets->GetTextureByKey("floor");
    layer.tex = floorTex;

    float ppu = floorTex->PixelsPerUnit();
    float wUnits = floorTex->Width() / ppu;
    float hUnits = floorTex->Height() / ppu;
    layer.scale = { 1.f / wUnits, 1.f / hUnits };

    Renderer->AddParallaxLayer(layer);

    // 2) Música
    auto* sound = Sound;
    if (auto* music = assets->GetMusicByKey("bg_music"))
        sound->PlayMusic(music, -1, 0.7f);

    // 3) Crear escena vacía
    auto* scenes = Scenes;

    // 10) Registrar y activar escena
    scenes->Register("level1", [] (Scene* scn) {

        auto* assets = Assets;

        // 4) Player
        auto* texPlayer = assets->GetTextureByKey("player");
        GameObject* player = CreateSpriteObject(
            scn, "Player", texPlayer,
            Vec3{ 0.f, 0.f, 1.f },
            Vec2{ 1.f, 1.f },
            false,      // no trigger
            1u << 0,    // capa jugador
            0xFFFFFFFFu // colisiona con todo
        );
        player->AddComponent<PlayerController>();
        player->AddComponent<CameraFollow>();  // la cámara seguirá a este objeto
        player->AddComponent<RigidBody2D>();
        player->GetComponent<Collider2D>()->shape = Collider2D::Shape::Circle;
        player->GetComponent<Collider2D>()->radius = player->GetComponent<Collider2D>()->size->x / 2.5f;


        // 5) Paredes (arena simple)
        auto* texWall = assets->GetTextureByKey("wall");
        // Suponiendo tamaño 1x1 world cada tile, crea un rectángulo de 10x10
        for (int x = -20; x <= 20; ++x) {
            // top
            auto* wallTop = CreateSpriteObject(scn, "WallTop", texWall,
                Vec3{ (float)x, -20.f, 0.f },
                Vec2{ 1.f, 1.f }, false, 1u << 1, 0xFFFFFFFFu, false);

            // bottom
            auto* wallBot = CreateSpriteObject(scn, "WallBot", texWall,
                Vec3{ (float)x, 20.f, 0.f },
                Vec2{ 1.f, 1.f }, false, 1u << 1, 0xFFFFFFFFu, false);
        }
        for (int y = -20; y <= 20; ++y) {
            // left
            auto* wallLeft = CreateSpriteObject(scn, "WallLeft", texWall,
                Vec3{ -21.f, (float)y, 0.f },
                Vec2{ 1.f, 1.f }, false, 1u << 1, 0xFFFFFFFFu, false);

            // right
            auto* wallRight = CreateSpriteObject(scn, "WallRight", texWall,
                Vec3{ 21.f, (float)y, 0.f },
                Vec2{ 1.f, 1.f }, false, 1u << 1, 0xFFFFFFFFu, false);
        }

        float ppu = texWall->PixelsPerUnit();
        float wUnits = texWall->Width() / ppu;
        float hUnits = texWall->Height() / ppu;
        Vec2 scaleHWalls{ 41.f / wUnits, 1.f / hUnits };
        Vec2 scaleVWalls{ 1.f / wUnits, 39.f / hUnits };
        Vec2 sizeHWalls{ 1.f * wUnits, 1.f * hUnits };
        Vec2 sizeVWalls{ 1.f * wUnits, 1.f * hUnits };

        auto * wallUpCol = scn->CreateObject("WallUpCollision");
        wallUpCol->transform->position = Vec3(0, -20, 0.f);
        wallUpCol->transform->scale = scaleHWalls;
        wallUpCol->AddComponent<Collider2D>()->size = sizeHWalls;
        wallUpCol->tag = "wall";

        auto* wallDownCol = scn->CreateObject("WallDownCollision");
        wallDownCol->transform->position = Vec3(0, 20, 0.f);
        wallDownCol->transform->scale = scaleHWalls;
        wallDownCol->AddComponent<Collider2D>()->size = sizeHWalls;
        wallDownCol->tag = "wall";

        auto* wallLeftCol = scn->CreateObject("WallLeftCollision");
        wallLeftCol->transform->position = Vec3(-21, 0, 0.f);
        wallLeftCol->transform->scale = scaleVWalls;
        wallLeftCol->AddComponent<Collider2D>()->size = sizeVWalls;
        wallLeftCol->tag = "wall";

        auto* wallRightCol = scn->CreateObject("WallRightCollision");
        wallRightCol->transform->position = Vec3(21, 0, 0.f);
        wallRightCol->transform->scale = scaleVWalls;
        wallRightCol->AddComponent<Collider2D>()->size = sizeVWalls;
        wallRightCol->tag = "wall";

        coinPrefab = [](GameObject& go, Scene&)
            {
                auto* texCoin = Assets->GetTextureByKey("coin");

                go.AddComponent<SpriteRenderer>()->sprite = texCoin;
                float ppu = texCoin->PixelsPerUnit();
                float wUnits = texCoin->Width() / ppu;
                float hUnits = texCoin->Height() / ppu;
                go.transform->scale = { 0.5f / wUnits, 0.5f / hUnits };
                go.AddComponent<Collider2D>()->isTrigger = true;
                go.AddComponent<CoinPickup>();
                go.GetComponent<Collider2D>()->shape = Collider2D::Shape::Circle;
                go.GetComponent<Collider2D>()->radius = (0.7f * wUnits) / 2.5f;
                go.GetComponent<Collider2D>()->mask = 1u << 0;
                go.GetComponent<Collider2D>()->layer = 0xFFFFFFFFu;
            };
        
        for (int i = 0; i < 20; i++)
        {
            scn->Instantiate("coin", coinPrefab, Vec3(
                Random->Range(-19.f, 19.f),
                Random->Range(-19.f, 19.f),
                0.f
            ));
        }

        // 8) HUD (sin sprite, solo Behaviour)
        GameObject* hud = scn->CreateObject("HUD and spawner");
        hud->AddComponent<HUDController>();
        hud->AddComponent<EnemySpawner>();

        // 9) Configurar cámara inicial (opcional)
        Camera2D* cam = scn->camera;     // Scene tiene un Camera2D interno 
        if (cam) {
            cam->center = Vec2{ 0.f, 0.f };
            cam->viewBase = Vec2{ 20.f, 11.25f }; // 16:9 world size, por ejemplo
        }
    });;

    scenes->SetActive("level1");
}

int main(int, char**)
{
    if (!Engine::Start("../../../XEngine_CONFIG.json"))   // inicializa SDL, Window, Render, Time, Input, Assets, Sound, Collision, UI, SceneManager… 
        return -1;

    Engine::SetInitCallback(InitGame);

    // (Opcional) callbacks de lógica global
    Engine::SetUpdateCallback([](float dt)
    {
        // lógica global fuera de escenas si quieres
    });

    int returnValue = Engine::Run();   // bucle principal: input, fixedUpdate, update, render, colisiones, UI… 

    return returnValue;
}