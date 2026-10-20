#include "stdafx.h"
#include "Monitor.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#pragma comment(lib, "ws2_32.lib")
	typedef int socklen_t;
	#define CloseSocket closesocket
#else
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <unistd.h>
	#define CloseSocket close
	#define INVALID_SOCKET (-1)
#endif

namespace
{
	void SendAll(int client, const char* data, size_t length)
	{
		size_t sent = 0;
		while (sent < length)
		{
			const int wrote = static_cast<int>(
				send(client, data + sent, static_cast<int>(length - sent), 0));
			if (wrote <= 0)
			{
				return;   // 상대가 끊었다. 조용히 그만둔다
			}
			sent += static_cast<size_t>(wrote);
		}
	}

	void SendResponse(int client, const char* status, const char* type, const std::string& body)
	{
		char header[256];
		const int length = std::snprintf(header, sizeof(header),
			"HTTP/1.1 %s\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %zu\r\n"
			"Cache-Control: no-store\r\n"       // 대시보드가 옛 값을 붙들지 않게
			"Connection: close\r\n"
			"\r\n",
			status, type, body.size());

		SendAll(client, header, static_cast<size_t>(length));
		SendAll(client, body.data(), body.size());
	}
}

Monitor::~Monitor()
{
	Stop();
}

bool Monitor::Start(int port, const std::string& webRoot)
{
	m_WebRoot = webRoot;

#if defined(_WIN32)
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		return false;
	}
#endif

	m_Listen = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
	if (m_Listen == INVALID_SOCKET)
	{
		return false;
	}

	// 게임을 껐다 바로 켤 때 "주소가 이미 쓰이는 중"으로 막히지 않게 한다.
	int reuse = 1;
	setsockopt(m_Listen, SOL_SOCKET, SO_REUSEADDR,
	           reinterpret_cast<const char*>(&reuse), sizeof(reuse));

	sockaddr_in address;
	std::memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(static_cast<unsigned short>(port));

	// 이 컴퓨터에서만 접속할 수 있게 한다. 같은 망의 다른 사람에게는 열지 않는다.
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(m_Listen, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
	    || listen(m_Listen, 8) != 0)
	{
		CloseSocket(m_Listen);
		m_Listen = -1;
		return false;
	}

	m_Port = port;
	m_Running = true;
	m_Thread = std::thread(&Monitor::Serve, this);
	return true;
}

void Monitor::Stop()
{
	if (!m_Running)
	{
		return;
	}

	m_Running = false;

	// 듣는 소켓을 닫으면 accept가 풀려서 스레드가 빠져나온다.
	if (m_Listen != -1)
	{
		CloseSocket(m_Listen);
		m_Listen = -1;
	}
	if (m_Thread.joinable())
	{
		m_Thread.join();
	}

#if defined(_WIN32)
	WSACleanup();
#endif
}

void Monitor::Publish(const std::string& json)
{
	std::lock_guard<std::mutex> guard(m_Lock);
	m_Json = json;
}

std::string Monitor::Snapshot() const
{
	std::lock_guard<std::mutex> guard(m_Lock);
	return m_Json;
}

void Monitor::Serve()
{
	while (m_Running)
	{
		sockaddr_in from;
		socklen_t fromLength = sizeof(from);
		const int client = static_cast<int>(
			accept(m_Listen, reinterpret_cast<sockaddr*>(&from), &fromLength));

		if (client == INVALID_SOCKET)
		{
			break;   // Stop()이 소켓을 닫았거나 더는 받을 수 없다
		}

		HandleOne(client);
		CloseSocket(client);
	}
}

void Monitor::HandleOne(int client)
{
	// 요청의 첫 줄만 보면 충분하다. 헤더와 본문은 읽지 않는다.
	char buffer[1024];
	const int received = static_cast<int>(recv(client, buffer, sizeof(buffer) - 1, 0));
	if (received <= 0)
	{
		return;
	}
	buffer[received] = '\0';
	++m_Requests;

	// "GET /경로 HTTP/1.1"
	std::string path = "/";
	{
		const char* start = std::strchr(buffer, ' ');
		if (start != NULL)
		{
			++start;
			const char* end = std::strchr(start, ' ');
			if (end != NULL)
			{
				path.assign(start, end);
			}
		}
	}

	if (path == "/status.json")
	{
		SendResponse(client, "200 OK", "application/json; charset=utf-8", Snapshot());
		return;
	}

	if (path == "/" || path == "/index.html")
	{
		const std::string page = ReadWebFile("index.html");
		if (page.empty())
		{
			SendResponse(client, "500 Internal Server Error", "text/plain; charset=utf-8",
			             "Monitor/index.html 을 찾을 수 없습니다.");
			return;
		}
		SendResponse(client, "200 OK", "text/html; charset=utf-8", page);
		return;
	}

	SendResponse(client, "404 Not Found", "text/plain; charset=utf-8", "없는 주소입니다.");
}

std::string Monitor::ReadWebFile(const std::string& name) const
{
	// 경로에 장난을 못 치게 이름만 받는다. '/'나 '..'이 들어오면 거절한다.
	if (name.find('/') != std::string::npos || name.find("..") != std::string::npos)
	{
		return std::string();
	}

	std::ifstream file((m_WebRoot + "/" + name).c_str(), std::ios::binary);
	if (!file)
	{
		return std::string();
	}

	std::ostringstream text;
	text << file.rdbuf();
	return text.str();
}

// ═══ JSON ═════════════════════════════════════════════════════════
namespace Json
{
	std::string Quote(const std::string& text)
	{
		std::string out;
		out.reserve(text.size() + 2);
		out += '"';

		for (size_t i = 0; i < text.size(); ++i)
		{
			const unsigned char c = static_cast<unsigned char>(text[i]);
			switch (c)
			{
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			default:
				if (c < 0x20)
				{
					char escape[8];
					std::snprintf(escape, sizeof(escape), "\\u%04x", c);
					out += escape;
				}
				else
				{
					// 한글을 비롯한 UTF-8은 그대로 둔다. JSON이 허용한다.
					out += static_cast<char>(c);
				}
				break;
			}
		}

		out += '"';
		return out;
	}

	std::string Number(float value)
	{
		char text[32];
		std::snprintf(text, sizeof(text), "%.2f", value);
		return std::string(text);
	}

	std::string Number(int value)
	{
		char text[32];
		std::snprintf(text, sizeof(text), "%d", value);
		return std::string(text);
	}

	std::string Bool(bool value)
	{
		return value ? "true" : "false";
	}
}
