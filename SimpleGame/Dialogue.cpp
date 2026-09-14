#include "stdafx.h"
#include "Dialogue.h"

namespace
{
	// 한 줄은 서른 자 안쪽으로. 대화창 한 줄에 들어가는 길이다.

	const DialogueLine kBlacksmith[] =
	{
		{ false, "……불이다. 진짜 불이야." },
		{ false, "망치를 쥔 채로 굳어 있었군.\n얼마나 지났지? 아니, 말하지 마." },
		{ true,  "종탑의 종을 다시 울리려 합니다." },
		{ false, "{종탑} 종탑 말인가. 금이 갔을 텐데.\n그래도 소리는 날 거다. 둘이 당긴다면." },
		{ false, "앞장서게. 그 등불에서 멀어지면\n또 저것들한테 먹힐 테니." },
	};

	const DialogueLine kMiner[] =
	{
		{ false, "갱도가 무너졌을 때도 이렇게 어둡진 않았어." },
		{ false, "기름 냄새… 당신 등불이구먼.\n그 불, 아껴 써. 여긴 기름이 귀해." },
		{ true,  "종을 울리면 이 일대가 밝아집니다." },
		{ false, "{종탑}으로 가면 종탑이 있지.\n곡괭이는 잃었어도 밧줄 당길 팔은 있네." },
		{ false, "가세. 뒤처지면 소리라도 질러 주게." },
	};

	const DialogueLine kMonk[] =
	{
		{ false, "기도가… 닿았나 봅니다." },
		{ false, "어둠 속에서 내내 종소리를 기다렸습니다.\n울리지 않더군요. 울릴 사람이 없었으니." },
		{ true,  "함께 울려 주시겠습니까." },
		{ false, "큰 종은 혼자서는 울지 않는 법이지요.\n{종탑}에 있는 종루로 가십시다." },
		{ false, "그 불빛 곁에 있겠습니다.\n제가 다시 굳거든, 부디 돌아와 주십시오." },
	};

	const DialogueLine kHerbalist[] =
	{
		{ false, "……손끝이 따뜻해. 이게 얼마 만이야." },
		{ false, "역병이 돌 때 약을 달이다 불이 꺼졌어.\n그다음은 기억이 안 나." },
		{ true,  "종탑으로 갑니다. 같이 가요." },
		{ false, "{종탑} 종탑이면 길이 험할 거야.\n그래도 여기 혼자 남는 것보단 낫지." },
		{ false, "등불 가까이 붙어 갈게. 놓치지 마." },
	};

	const DialogueLine kHunter[] =
	{
		{ false, "쉿. …아직 저것들이 근처에 있어." },
		{ false, "까마귀가 울 때 도망쳤어야 했는데.\n발이 먼저 굳더군." },
		{ true,  "종이 울리면 저것들은 물러납니다." },
		{ false, "종탑은 {종탑}에 있어. 지름길은 없고.\n네 발자국이 꺼지기 전에 따라가지." },
		{ false, "앞은 네가 봐. 뒤는 내가 보마." },
	};

	const DialogueLine kScribe[] =
	{
		{ false, "잉크가… 아니, 제 손이 검었던 거군요." },
		{ false, "이 섬의 지도를 옮겨 적고 있었습니다.\n어둠이 종이까지 먹어 버렸지만." },
		{ true,  "종탑이 어디 있는지 아십니까." },
		{ false, "{종탑}입니다. 지도는 머릿속에 남아 있어요.\n길은 당신이 비춰 주셔야 하지만." },
		{ false, "가시지요. 기록할 일이 생기겠군요." },
	};

	const DialogueLine kBellRinger[] =
	{
		{ false, "……그 줄은 내가 당겨야 하는데." },
		{ false, "사십 년을 저 종을 쳤지.\n마지막엔 혼자서는 꿈쩍도 안 하더군." },
		{ true,  "이번엔 둘입니다." },
		{ false, "허, 그래. 둘이면 울리지.\n{종탑} 종탑이다. 내 발이 기억해." },
		{ false, "천천히 가게. 늙은이가 따라갈 만큼만." },
	};

	const DialogueLine kSoldier[] =
	{
		{ false, "…칼은 어디 갔지. 아니, 필요 없나." },
		{ false, "영주의 성은 진즉 무너졌소.\n지킬 것도 없는데 여기서 굳어 버렸군." },
		{ true,  "지킬 것이 생길 겁니다. 종을 울리면." },
		{ false, "{종탑} 종탑이라. 좋소, 따라가지.\n당신 불빛에 붙어 가는 쪽이겠지만." },
		{ false, "앞장서시오." },
	};

	#define SCRIPT(speaker, lines) { speaker, lines, static_cast<int>(sizeof(lines) / sizeof(lines[0])) }

	const DialogueScript kWakeScripts[] =
	{
		SCRIPT("대장장이", kBlacksmith),
		SCRIPT("광부", kMiner),
		SCRIPT("수도승", kMonk),
		SCRIPT("약초꾼", kHerbalist),
		SCRIPT("사냥꾼", kHunter),
		SCRIPT("필경사", kScribe),
		SCRIPT("종지기 노인", kBellRinger),
		SCRIPT("떠돌이 병사", kSoldier),
	};

	#undef SCRIPT
}

int WakeScriptCount()
{
	return static_cast<int>(sizeof(kWakeScripts) / sizeof(kWakeScripts[0]));
}

const DialogueScript& WakeScript(int index)
{
	const int count = WakeScriptCount();
	int wrapped = index % count;
	if (wrapped < 0)
	{
		wrapped += count;
	}
	return kWakeScripts[wrapped];
}
