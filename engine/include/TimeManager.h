#pragma once

#include "Singleton.h"
#include "Property.h"
#include "Engine.h"

class TimeManager : public Singleton<TimeManager>
{
	friend class Singleton<TimeManager>;
	friend class Engine;

private:
	float Delta() const noexcept { return delta; }
	inline float FixedDelta() const noexcept { return fixedDt; }
	double SinceStart() const noexcept;
	void SetTimeScale(float s) noexcept;
	float TimeScale() const noexcept { return timeScale_; }
	uint64_t GetHighResTimestamp() const noexcept;

	TimeManager() = default;
	~TimeManager() = default;

	bool Init(float fixedDt = 1.f / 60.f) noexcept;
	void Tick() noexcept;

	TimeManager(const TimeManager&) = delete;
	TimeManager& operator=(const TimeManager&) = delete;
	TimeManager(TimeManager&&) = delete;
	TimeManager& operator=(TimeManager&&) = delete;

	double startTicks = 0.0;
	double prevTicks = 0.0;
	float delta = 0.f;
	float timeScale_ = 1.f;
	float fixedDt = 1.f / 60.f;

public:
	using DeltaProp = PropertyRO < TimeManager, float,
		&TimeManager::Delta>;
	DeltaProp deltaTime{ this };

	using TimeStampProp = PropertyRO < TimeManager, uint64_t,
		&TimeManager::GetHighResTimestamp>;
	TimeStampProp highResTimeStamp{ this };

	using FixedDeltaProp = PropertyRO < TimeManager, float,
		&TimeManager::FixedDelta>;
	FixedDeltaProp fixedDeltaTime{ this };

	using SinceStartProp = PropertyRO < TimeManager, double,
		&TimeManager::SinceStart>;
	SinceStartProp timeSinceStart{ this };

	using TimeScaleProp = Property<TimeManager, float,
		&TimeManager::TimeScale,
		&TimeManager::SetTimeScale>;
	TimeScaleProp timeScale{ this };
};