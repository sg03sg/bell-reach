#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

//
// 모니터링 — 게임이 돌아가는 동안 밖에서 들여다보는 창.
//
// 게임 안에 아주 작은 HTTP 서버를 띄운다. 브라우저로 http://localhost:8787 에
// 들어가면 대시보드가 뜨고, 그 페이지가 1초마다 /status.json 을 받아 간다.
//
//   브라우저  ──  GET /             →  Monitor/index.html (대시보드)
//             ──  GET /status.json  →  지금 이 순간의 상태
//
// 별도 프로그램을 띄우지 않아도 되고, 페이지와 데이터가 같은 출처라
// 브라우저가 막지 않는다(file:// 로 열면 CORS에 막힌다).
//
// 게임 루프를 멈추지 않도록 서버는 따로 돌린다. 주고받는 것은
// 문자열 하나뿐이라, 자물쇠 하나로 충분하다.
//
//   게임 스레드 : 몇 분의 1초마다 Publish(json) 으로 최신 상태를 놓고 간다
//   서버 스레드 : 요청이 오면 놓여 있는 것을 그대로 보낸다
//
// 둘은 서로를 기다리지 않는다. 브라우저가 늦게 받아 가도 게임은 느려지지 않고,
// 게임이 바빠도 서버는 직전 상태를 보여 준다.
//
class Monitor
{
public:
	~Monitor();

	// 듣기 시작한다. 실패해도 게임은 그대로 돌아간다(모니터만 꺼진다).
	// webRoot는 대시보드 파일이 있는 폴더다.
	bool Start(int port, const std::string& webRoot);
	void Stop();

	bool IsRunning() const { return m_Running; }
	int Port() const { return m_Port; }

	// 최신 상태를 놓아 둔다. 게임 스레드에서 부른다.
	void Publish(const std::string& json);

	// 지금까지 받은 요청 수. 보고 있는 사람이 있는지 알 수 있다.
	int RequestCount() const { return m_Requests; }

private:
	void Serve();                                  // 서버 스레드가 도는 곳
	void HandleOne(int client);
	std::string Snapshot() const;
	std::string ReadWebFile(const std::string& name) const;

	int m_Listen = -1;
	int m_Port = 0;
	std::string m_WebRoot;

	std::thread m_Thread;
	std::atomic<bool> m_Running{ false };
	std::atomic<int> m_Requests{ 0 };

	mutable std::mutex m_Lock;
	std::string m_Json = "{}";
};

//
// JSON 조각을 만드는 도구. 외부 라이브러리를 쓰지 않으므로 직접 쌓는다.
//
namespace Json
{
	// 따옴표와 역슬래시, 제어문자를 막는다. 한글은 UTF-8 그대로 둬도 된다.
	std::string Quote(const std::string& text);

	std::string Number(float value);
	std::string Number(int value);
	std::string Bool(bool value);
}
