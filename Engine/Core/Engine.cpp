#include "Engine.h"

#include <iostream>
#include "Level/level.h"
#include "Utils/Utils.h"
#include "Render/ScreenBuffer.h"

Engine* Engine::instance = nullptr;

BOOL WINAPI ConsoleMessageProcedure(DWORD ctrlType)
{
	switch (ctrlType)
	{
	case CTRL_CLOSE_EVENT:
	{
		Engine::Get().CleanUp(); // Engine::Get().~Engine(); // 와 같다
		return false;
	}
	}
	return true;
}

Engine::Engine()
{
	instance = this;

	// 콘솔 커서 끄기
	_CONSOLE_CURSOR_INFO consoleCursorInfo;
	consoleCursorInfo.bVisible = false;
	consoleCursorInfo.dwSize = 1;
	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &consoleCursorInfo); // 커서가 보이지 않도록 

	// 엔진 설정 로드
	LoadEngineSettings();

	// 랜덤 종자값(Seed) 생성
	srand(static_cast<unsigned int>(time(nullptr)));

	//
	// 랜더 관련 초기화
	//

	// 이미지 버퍼 생성.
	Vector2 screenSize(settings.width, settings.height);
	imageBuffer = new ImageBuffer(((int)screenSize.x + 1) * (int)screenSize.y + 1);

	ClearImageBuffer(); // 버퍼 초기화 (문자 버퍼).

	// 두 개의 버퍼 생성.
	renderTargets[0] = new ScreenBuffer(GetStdHandle(STD_OUTPUT_HANDLE), screenSize);
	renderTargets[1] = new ScreenBuffer(screenSize);

	Present(); // 버퍼 교환.

	//
	// 콘솔창 이벤트 등록
	//

	SetConsoleCtrlHandler(ConsoleMessageProcedure, TRUE);

	// cls 호출.
	system("cls");
}

Engine::~Engine()
{
	CleanUp();
}

void Engine::Run()
{
	//float currentTime = timeGetTime();
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	LARGE_INTEGER previousTime = currentTime;

	// 하드웨어의 정밀도(주파수) 가져오기
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	// 타겟 프레임 설정
	float targetFrameRate = (settings.framerate != 0.0f) ? settings.framerate : 60.f;

	float oneFrameTime = 1.f / targetFrameRate;

	while (true)
	{
		if (isQuit)
		{
			break;
		}

		QueryPerformanceCounter(&currentTime);
		float deltaTime = (currentTime.QuadPart - previousTime.QuadPart) / (float)frequency.QuadPart;

		input.ProcessInput();

		if (deltaTime >= oneFrameTime) // 고정 프레임
		{
			// ========== EVENT ==========
			BeginPlaye();
			Tick(deltaTime);
			Render();
			// ========== EVENT ==========

			// 제목에 FPS 출력
			char title[50] = {};
			sprintf_s(title, 50, "(h: %d, w: %d)FPS: %f", settings.height, settings.width, (1.f / deltaTime));
			SetConsoleTitleA(title);

			previousTime = currentTime;

			input.SavePreviousKeyStates();

			if (mainLevel) // 이전 프레임에 추가 및 삭제 요청 처리
			{
				mainLevel->ProcessAddAndDestroyActors();
			}
		}
	}

	//SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7 /*= FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED*/); // 출력 색상 정리.
	Utils::SetConsoleTextColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
}

void Engine::WriteToBuffer(const Vector2& position, const char* image, Color color, int sortingOrder)
{
	// 문자열 길이.
	int length = static_cast<int>(strlen(image));

	// 문자열 기록.
	for (int ix = 0; ix < length; ++ix)
	{
		// @Todo: 화면 버퍼 크기 안 넘어가게 예외처리 필요함.

		// 기록할 문자 위치.
		int index = ((int)position.y * (settings.width)) + (int)position.x + ix;

		if (imageBuffer->sortingOrderArray[index] > sortingOrder)
		{
			continue;
		}

		// 버퍼에 문자/색상 기록.
		imageBuffer->charInfoArray[index].Char.AsciiChar = image[ix];
		imageBuffer->charInfoArray[index].Attributes = (WORD)color;

		imageBuffer->sortingOrderArray[index] = sortingOrder;
	}
}

void Engine::PresentImmediately() // 언제 사용할지 잘 모르겠다
{
	GetRenderer()->Render(imageBuffer->charInfoArray); // 백 버퍼에 그리기
	Present(); // 버퍼 교환
}

void Engine::CleanUp()
{
	// 레벨 삭제.
	SafeDelete(mainLevel);

	// 문자 버퍼 삭제.
	SafeDelete(imageBuffer);

	// 렌더 타겟 삭제.
	SafeDelete(renderTargets[0]);
	SafeDelete(renderTargets[1]);
}

void Engine::Quit()
{
	isQuit = true;
}

void Engine::AddLevel(Level* newLevel)
{
	if (mainLevel)
	{
		delete mainLevel;
	}

	mainLevel = newLevel;
}

int Engine::Width() const
{
	return settings.width;
}

int Engine::Height() const
{
	return settings.height;
}

ScreenBuffer* Engine::GetRenderer() const
{
	return renderTargets[currentRenderTargetIndex];
}

Engine& Engine::Get()
{
	return *instance;
}

void Engine::BeginPlaye()
{
	// LEVEL
	if (mainLevel)
	{
		mainLevel->BeginPlay();
	}
}

void Engine::Tick(float deltaTime)
{
	// LEVEL
	if (mainLevel)
	{
		mainLevel->Tick(deltaTime);
	}

	/*if (GetKeyDown(VK_ESCAPE))
	{
		Quit();
	}*/
}

void Engine::Render()
{
	//SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7 /*= FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED*/); // 출력 색상 정리.
	Utils::SetConsoleTextColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);

	Clear(); // 화면 지우기

	// LEVEL
	if (mainLevel)
	{
		mainLevel->Render();
	}

	GetRenderer()->Render(imageBuffer->charInfoArray); // 백버퍼에 데이터 쓰기

	Present(); // 버퍼 교환
}

void Engine::LoadEngineSettings()
{
	// 파일 열기
	FILE* file = nullptr;

	fopen_s(&file, "../Settings/EngineSettings.txt", "rt");

	if (file == nullptr)
	{
		std::cout << "Failed to load engine settigs. \n";
		__debugbreak();
		return;
	}

	/// 로드 
	fseek(file, 0, SEEK_END); // FP(file position) 포인터를 끝위치로 옮기기
	size_t fileSize = ftell(file); // 위치 구하기
	rewind(file); // FP 포인터를 처음으로 

	char* buffer = new char[fileSize + 1];

	size_t readSize = fread(buffer, sizeof(char), fileSize, file); // 내용 읽기

	/// 파싱(구문 해석)
	char* context = nullptr;
	char* token = nullptr;

	token = strtok_s(buffer, "\n", &context);

	// 구문 분석
	while (token != nullptr)
	{
		// 키 = 값 분리
		char header[10] = {};

		// 아래 구문이 제대로 동작하려면 키와 값 사이의 빈칸이 있어야 함
		sscanf_s(token, "%s", header, 10);

		// 헤더 문자열 비교
		if (strcmp(header, "framerate") == 0)
		{
			sscanf_s(token, "framerate = %f", &settings.framerate);
		}
		else if (strcmp(header, "width") == 0)
		{
			sscanf_s(token, "width = %d", &settings.width);
		}
		else if (strcmp(header, "height") == 0)
		{
			sscanf_s(token, "height = %d", &settings.height);
		}

		// 그 다음줄 분리
		token = strtok_s(nullptr, "\n", &context);
	}

	SafeDeleteArray(buffer); // 버퍼 해제

	// 파일 닫기
	fclose(file);
}

void Engine::Clear()
{
	ClearImageBuffer();
	GetRenderer()->Clear();
}

void Engine::Present()
{
	// 버퍼 교환.
	SetConsoleActiveScreenBuffer(GetRenderer()->buffer);

	// 인덱스 뒤집기. 1->0, 0->1.
	currentRenderTargetIndex = 1 - currentRenderTargetIndex;
}

void Engine::ClearImageBuffer()
{
	// 글자 버퍼 덮어쓰기.
	for (int y = 0; y < settings.height; ++y)
	{
		for (int x = 0; x < settings.width; ++x)
		{
			int index = (y * (settings.width)) + x;
			CHAR_INFO& buffer = imageBuffer->charInfoArray[index];
			buffer.Char.AsciiChar = ' ';
			buffer.Attributes = 0;
			imageBuffer->sortingOrderArray[index] = -1;
		}

		// 각 줄 끝에 개행 문자 추가.
		int index = (y * (settings.width)) + settings.width;
		CHAR_INFO& buffer = imageBuffer->charInfoArray[index];
		buffer.Char.AsciiChar = '\n';
		buffer.Attributes = 0;
		imageBuffer->sortingOrderArray[index] = -1;
	}

	// 마지막에 널 문자 추가.
	int index = (settings.width) * settings.height + 1;
	CHAR_INFO& buffer = imageBuffer->charInfoArray[index];
	buffer.Char.AsciiChar = '\0';
	buffer.Attributes = 0;
	imageBuffer->sortingOrderArray[index] = -1;
}
