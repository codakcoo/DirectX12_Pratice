#pragma once



class GameTimer
{
public:
	GameTimer();

	float TotalTime() const;			// 일지정지 시간 제외한 누적 시간
	float DeltaTime() const;			// 직전 프레임과의 간격

	void Reset();						// 메시지 루프 시작전 호출
	void Start();						// 일시정지 해제
	void Stop();						// 일시정지
	void Tick();						// 매 프레임 호출

private:
	double mSecondsPerCount;				// 카운트 -> 초 변환 계수
	double mDeltaTime;

	__int64 mBaseTime;
	__int64	mPausedTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped;
};

