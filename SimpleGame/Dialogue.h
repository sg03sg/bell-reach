#pragma once

//
// 대사.
//
// 굳어 있다 깨어난 사람이 처음 건네는 말이다. 구역마다 누가 굳어 있는지는
// 좌표로 정해지고(Regions::Region::personSeed), 그 사람의 대사를 여기서 고른다.
//
// 대사 속 {종탑}은 그 구역 종탑이 있는 방향("동쪽", "북서쪽" …)으로 바뀐다.
// 기획서의 "깨운 사람이 종탑 위치를 알려준다"가 이 한 줄에서 나온다.
//
// 지금은 코드에 적혀 있지만, 내용과 규칙을 나눠 두었으므로 v0.2에서
// 파일로 옮길 때 Game은 바뀌지 않는다.
//
struct DialogueLine
{
	bool fromPlayer;      // true면 "나"의 대사
	const char* text;     // 한 페이지. '\n'으로 두 줄까지
};

struct DialogueScript
{
	const char* speaker;  // 화면에 나오는 이름
	const DialogueLine* lines;
	int lineCount;
};

int WakeScriptCount();
const DialogueScript& WakeScript(int index);
