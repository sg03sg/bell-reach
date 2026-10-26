#include "stdafx.h"
#include "World.h"

#include "Regions.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace
{
	// 좌표와 씨앗을 섞어 청크마다 고정된 난수 씨앗을 만든다.
	unsigned int HashCoords(int x, int y, unsigned int seed)
	{
		unsigned int h = seed;
		h ^= static_cast<unsigned int>(x) * 0x9E3779B9u;
		h = (h << 13) | (h >> 19);
		h *= 0x85EBCA6Bu;
		h ^= static_cast<unsigned int>(y) * 0xC2B2AE35u;
		h = (h << 17) | (h >> 15);
		h *= 0x27D4EB2Fu;
		h ^= h >> 16;
		return h ? h : 0x1234567u;   // xorshift는 0에서 벗어나지 못한다
	}

	long long PackKey(int chunkX, int chunkY)
	{
		return (static_cast<long long>(chunkY) << 32)
		     | static_cast<long long>(static_cast<unsigned int>(chunkX));
	}

	// 청크 안에서 쓰는 작은 난수기. 같은 청크는 늘 같은 순서로 같은 값을 낸다.
	class ChunkRandom
	{
	public:
		explicit ChunkRandom(unsigned int seed) : m_State(seed) {}

		unsigned int Next()
		{
			m_State ^= m_State << 13;
			m_State ^= m_State >> 17;
			m_State ^= m_State << 5;
			return m_State;
		}

		int Range(int low, int high)     // low 이상 high 이하
		{
			return low + static_cast<int>(Next() % static_cast<unsigned int>(high - low + 1));
		}

		bool Chance(int percent)
		{
			return static_cast<int>(Next() % 100u) < percent;
		}

	private:
		unsigned int m_State;
	};
}

World::World(unsigned int seed)
	: m_Seed(seed)
{
}

int World::FloorDiv(int value, int divisor)
{
	const int quotient = value / divisor;
	return (value % divisor != 0 && (value < 0) != (divisor < 0)) ? quotient - 1 : quotient;
}

int World::PositiveMod(int value, int divisor)
{
	const int remainder = value % divisor;
	return remainder < 0 ? remainder + divisor : remainder;
}

void World::Generate(Chunk& chunk, int chunkX, int chunkY) const
{
	const int n = kChunkTiles;
	const int mid = n / 2;

	// 클래스 안에서 값만 정한 상수를 참조로 넘기면 링크 에러가 날 수 있어 복사해 넘긴다.
	const unsigned char grass = kGrass;
	chunk.tiles.assign(n * n, grass);
	chunk.props.clear();
	chunk.houses.clear();
	chunk.trees.clear();
	chunk.walls.clear();

	ChunkRandom random(HashCoords(chunkX, chunkY, m_Seed));

	auto tile = [&chunk](int x, int y) -> unsigned char&
	{
		return chunk.tiles[y * n + x];
	};
	auto inside = [](int x, int y)
	{
		return x >= 0 && y >= 0 && x < n && y < n;
	};
	auto onRoad = [](int x, int y)
	{
		return x == mid - 1 || x == mid || y == mid - 1 || y == mid;
	};

	// ── 0. 마을인가, 숲인가 ────────────────────────────────────
	// 길 위의 구역만 마을이다. 나머지는 들어갈 수 없는 숲이라 수풀로 채우고 끝낸다.
	// 숲은 나무 액터를 하나도 만들지 않는다 — 땅이 수풀로 그린다.
	const int regionX = FloorDiv(chunkX, kVillageChunks);
	const int regionY = FloorDiv(chunkY, kVillageChunks);

	int villageIndex = 0;
	unsigned char gates = Regions::kGateNorth | Regions::kGateEast
	                    | Regions::kGateSouth | Regions::kGateWest;   // 길을 모르면 사방으로
	if (m_Route != nullptr)
	{
		villageIndex = m_Route->VillageIndex(regionX, regionY);
		gates = m_Route->GatesOf(regionX, regionY);
	}

	if (villageIndex < 0)
	{
		const unsigned char woods = kWoods;
		chunk.tiles.assign(n * n, woods);
		return;
	}

	// 첫 마을은 돌 성벽, 나머지는 나무 울타리
	const bool stoneWall = (villageIndex == 0);

	// 마을(구역) 안에서의 칸 좌표. 클래스 상수는 복사해 쓴다(참조로 넘기면 링크 에러).
	const int span = kVillageSpan;
	const int wallLow = kWallLow;
	const int wallHigh = kWallHigh;
	const int laneLow = kLaneLow;
	const int laneHigh = kLaneHigh;
	const int roadLow = kRoadLow;
	const int roadHigh = kRoadHigh;

	auto localX = [&](int x) { return PositiveMod(chunkX * n + x, span); };
	auto localY = [&](int y) { return PositiveMod(chunkY * n + y, span); };
	auto outsideWall = [&](int lx, int ly)
	{
		return lx < wallLow || lx > wallHigh || ly < wallLow || ly > wallHigh;
	};
	auto onWall = [&](int lx, int ly)
	{
		return !outsideWall(lx, ly)
		    && (lx == wallLow || lx == wallHigh || ly == wallLow || ly == wallHigh);
	};

	// 마을 안에서 이 청크가 맡은 자리
	const int roleX = PositiveMod(chunkX, kVillageChunks);
	const int roleY = PositiveMod(chunkY, kVillageChunks);
	// 광장에서 몇 청크 떨어졌는가. 0은 광장, 1~2는 집이 늘어선 성읍,
	// 3은 울타리에 걸친 가장자리(뒤뜰과 텃밭, 빈집), 그 밖은 울타리 밖 숲이다.
	const int centerRole = kVillageChunks / 2;
	const int ring = std::max(std::abs(roleX - centerRole), std::abs(roleY - centerRole));
	const bool isPlaza = (ring == 0);
	const bool isWoods = (ring >= 3);

	// ── 1. 십자 흙길 ───────────────────────────────────────────
	// 네 변 정중앙에서 이웃 청크의 길과 만난다. 무한 맵에서 길이 끊기지 않는다.
	for (int i = 0; i < n; ++i)
	{
		tile(i, mid - 1) = kPath;
		tile(i, mid) = kPath;
		tile(mid - 1, i) = kPath;
		tile(mid, i) = kPath;
	}

	// ── 2. 종탑 광장 ───────────────────────────────────────────
	if (isPlaza)
	{
		for (int y = 4; y <= 11; ++y)
		{
			for (int x = 4; x <= 11; ++x)
			{
				tile(x, y) = kPlaza;
			}
		}
	}

	// ── 2.5 울타리, 안길, 숲, 문 ───────────────────────────────
	// 울타리 밖은 숲으로 덮고, 울타리 줄은 벽으로, 그 바로 안쪽 한 줄은 안길로.
	// 청크마다 깔아 둔 십자길 중 울타리 밖으로 나간 부분은 여기서 숲에 묻힌다.
	for (int y = 0; y < n; ++y)
	{
		for (int x = 0; x < n; ++x)
		{
			const int lx = localX(x);
			const int ly = localY(y);
			if (outsideWall(lx, ly))
			{
				tile(x, y) = kWoods;
			}
			else if (onWall(lx, ly))
			{
				tile(x, y) = kWall;
			}
			else if (lx == laneLow || lx == laneHigh || ly == laneLow || ly == laneHigh)
			{
				tile(x, y) = kPath;
			}
		}
	}

	// 앞 마을과 다음 마을 쪽으로만 문을 내고, 문에서 구역 끝까지 숲길을 튼다.
	// 문은 길섶까지 네 칸이 열리고, 문을 나서면 숲길이 점점 넓어져 열두 칸(384px,
	// 화면 높이의 절반 남짓)의 숲속 빈터가 된다. 가운데 네 줄이 흙길, 나머지는
	// 풀밭이고 드문드문 나무가 선다 — 길을 따라가되 어디로 비켜설지는 고를 수 있다.
	// 이웃 구역과 맞닿는 끝에서는 양쪽 모두 가장 넓으므로 폭이 어긋나지 않는다.
	//
	// 숲길은 사인 곡선 한 굽이만큼 좌우로 휜다. 문 앞(t=0)과 구역 경계(t=1)에서는
	// 휘지 않으므로 이웃 구역의 길과 늘 같은 줄에서 만난다. 한 칸 갈 때 한 칸보다
	// 덜 휘어서 길이 끊기지 않는다. 휘는 쪽은 구역과 방향마다 다르다.
	const int bendAmount = kRoadBend;
	auto bend = [&](int along, int length, int side)
	{
		if (length <= 0)
		{
			return 0;
		}
		const float t = static_cast<float>(along) / static_cast<float>(length);
		const float sign = ((HashCoords(regionX * 4 + side, regionY, m_Seed) >> 7) & 1u) ? 1.0f : -1.0f;
		return static_cast<int>(std::lround(sign * bendAmount * std::sin(6.2831853f * t)));
	};

	// 문에서 몇 칸 나왔는가로 숲길의 반폭을 정한다. 문 앞 세 칸은 문 폭 그대로,
	// 그다음 두 칸마다 한 칸씩 넓어져 kRoadSpread에서 멈춘다.
	const int spread = kRoadSpread;
	auto widen = [&](int along)
	{
		return std::min(spread, 1 + std::max(0, along - 2) / 2);
	};

	for (int y = 0; y < n; ++y)
	{
		for (int x = 0; x < n; ++x)
		{
			const int lx = localX(x);
			const int ly = localY(y);

			bool road = false;
			bool outside = false;   // 울타리 줄이 아니라 그 바깥 숲길인가
			int across = 0;         // 가운데 두 줄에서 몇 칸 떨어졌나
			int wide = 1;           // 이 자리의 반폭

			// 동서로 난 길: 휜 만큼 줄을 옮기고, 가운데 두 줄(roadLow · roadHigh)에서 wide칸씩 넓힌다
			auto eastWest = [&](int offset, int width, bool beyond)
			{
				const int center0 = roadLow + offset;
				const int center1 = roadHigh + offset;
				if (ly >= center0 - width && ly <= center1 + width)
				{
					road = true;
					outside = beyond;
					wide = width;
					across = std::max(0, std::max(center0 - ly, ly - center1));
				}
			};
			auto northSouth = [&](int offset, int width, bool beyond)
			{
				const int center0 = roadLow + offset;
				const int center1 = roadHigh + offset;
				if (lx >= center0 - width && lx <= center1 + width)
				{
					road = true;
					outside = beyond;
					wide = width;
					across = std::max(0, std::max(center0 - lx, lx - center1));
				}
			};

			if ((gates & Regions::kGateEast) && lx >= wallHigh)
			{
				const int along = lx - wallHigh;
				eastWest(bend(along, span - 1 - wallHigh, 0), widen(along), lx > wallHigh);
			}
			if ((gates & Regions::kGateWest) && lx <= wallLow)
			{
				const int along = wallLow - lx;
				eastWest(bend(along, wallLow, 1), widen(along), lx < wallLow);
			}
			if ((gates & Regions::kGateSouth) && ly >= wallHigh)
			{
				const int along = ly - wallHigh;
				northSouth(bend(along, span - 1 - wallHigh, 2), widen(along), ly > wallHigh);
			}
			if ((gates & Regions::kGateNorth) && ly <= wallLow)
			{
				const int along = wallLow - ly;
				northSouth(bend(along, wallLow, 3), widen(along), ly < wallLow);
			}

			if (!road)
			{
				continue;
			}

			if (!outside)
			{
				tile(x, y) = kPath;                 // 울타리에 난 문
			}
			else if (across >= wide)
			{
				tile(x, y) = kGrass;                // 숲과 맞닿는 길섶
			}
			else if (across <= 1)
			{
				tile(x, y) = kPath;                 // 가운데 흙길
			}
			else if (across >= 3 && random.Chance(4))
			{
				// 빈터에 드문드문 선 나무. 흙길에서 두 칸은 떨어져 있어 길을 막지 않는다.
				tile(x, y) = kTree;
				Tree tree;
				tree.tileX = chunkX * n + x;
				tree.tileY = chunkY * n + y;
				const int roll = random.Range(0, 99);
				tree.kind = static_cast<unsigned char>(roll < 35 ? 0 : (roll < 90 ? 1 : 2));
				chunk.trees.push_back(tree);
			}
			else
			{
				// 풀밭. 오래 밟힌 자리는 흙이 드러났다
				tile(x, y) = random.Chance(12) ? kPath : kGrass;
			}
		}
	}

	// ── 3. 집 ──────────────────────────────────────────────────
	// 집이 설 자리는 전부 풀밭이어야 하고, 둘레 한 칸에는 다른 집이나 나무가 없어야 한다.
	// 그래야 집과 집 사이로 늘 사람이 지나갈 수 있다.
	auto canBuild = [&](int x0, int y0, int width, int depth)
	{
		for (int y = y0 - 1; y <= y0 + depth; ++y)
		{
			for (int x = x0 - 1; x <= x0 + width; ++x)
			{
				if (!inside(x, y))
				{
					return false;
				}
				const bool footprint = (x >= x0 && x < x0 + width && y >= y0 && y < y0 + depth);
				if (footprint && outsideWall(localX(x), localY(y)))
				{
					return false;   // 숲길 빈터에는 집을 짓지 않는다
				}
				const unsigned char t = tile(x, y);
				if (footprint ? (t != kGrass) : (t == kHouse || t == kTree || t == kWall || t == kWoods))
				{
					return false;
				}
			}
		}
		return true;
	};

	auto build = [&](int x0, int y0, int width, int depth, int ruinedPercent)
	{
		House house;
		house.tileX = chunkX * n + x0;
		house.tileY = chunkY * n + y0;
		house.width = width;
		house.depth = depth;
		house.style = static_cast<unsigned char>(random.Next() % 3u);
		house.ruined = random.Chance(ruinedPercent);
		house.door = random.Range(-1, 1);
		chunk.houses.push_back(house);

		for (int y = y0; y < y0 + depth; ++y)
		{
			for (int x = x0; x < x0 + width; ++x)
			{
				tile(x, y) = kHouse;
			}
		}

		// 앞마당에 살림살이 흔적을 남긴다. 폐가 앞에는 무너진 잔해가 많다.
		if (random.Chance(65))
		{
			const int propX = x0 + (house.door < 0 ? width - 1 : 0);
			const int propY = y0 + depth;
			if (inside(propX, propY) && (tile(propX, propY) == kGrass || tile(propX, propY) == kPath))
			{
				Prop prop;
				prop.tileX = chunkX * n + propX;
				prop.tileY = chunkY * n + propY;
				if (house.ruined)
				{
					prop.kind = random.Chance(65) ? kPropRubble : kPropBarrel;
				}
				else
				{
					prop.kind = random.Chance(50) ? kPropSupply : kPropBarrel;
				}
				chunk.props.push_back(prop);
			}
		}
	};

	// 십자길이 청크를 넷으로 나눈다. 칸마다 집을 한 채까지 앉힌다.
	//   왼쪽 x 1~6, 오른쪽 x 9~14, 위 y 1~6, 아래 y 9~14
	const int quadrantLeft[2] = { 1, mid + 1 };
	const int quadrantTop[2] = { 1, mid + 1 };
	const int quadrantSpan = mid - 2;

	if (isPlaza)
	{
		// 광장 둘레, 위아래 띠에 네 채. 광장을 바라보고 늘어선 마을의 중심가다.
		for (int qy = 0; qy < 2; ++qy)
		{
			for (int qx = 0; qx < 2; ++qx)
			{
				const int width = random.Range(3, 4);
				const int depth = 2;
				const int y0 = (qy == 0) ? 1 : n - 1 - depth;
				for (int attempt = 0; attempt < 4; ++attempt)
				{
					const int x0 = quadrantLeft[qx] + random.Range(0, quadrantSpan - width);
					if (canBuild(x0, y0, width, depth))
					{
						build(x0, y0, width, depth, 20);
						break;
					}
				}
			}
		}
	}
	else
	{
		const int housePercent = isWoods ? 30 : 85;
		const int ruinedPercent = isWoods ? 75 : 35;

		for (int qy = 0; qy < 2; ++qy)
		{
			for (int qx = 0; qx < 2; ++qx)
			{
				if (!random.Chance(housePercent))
				{
					continue;
				}

				const int width = random.Range(3, 4);
				const int depth = random.Range(2, 3);

				// 위쪽 칸의 집은 앞마당이 길에 닿게 길 쪽으로 붙인다.
				const int y0 = (qy == 0) ? (mid - 2 - depth) : quadrantTop[1] + 1;

				for (int attempt = 0; attempt < 4; ++attempt)
				{
					const int x0 = quadrantLeft[qx] + random.Range(0, quadrantSpan - width);
					if (canBuild(x0, y0, width, depth))
					{
						build(x0, y0, width, depth, ruinedPercent);
						break;
					}
				}
			}
		}
	}

	// ── 4. 나무 ────────────────────────────────────────────────
	// 길 바로 옆과 집 둘레는 비워 둔다. 숲에서도 길은 늘 트여 있다.
	auto nearRoad = [&](int x, int y)
	{
		return onRoad(x - 1, y) || onRoad(x + 1, y) || onRoad(x, y - 1) || onRoad(x, y + 1);
	};
	auto nearHouse = [&](int x, int y)
	{
		for (int dy = -1; dy <= 1; ++dy)
		{
			for (int dx = -1; dx <= 1; ++dx)
			{
				if (inside(x + dx, y + dy) && tile(x + dx, y + dy) == kHouse)
				{
					return true;
				}
			}
		}
		return false;
	};

	const int treePercent = isWoods ? 30 : (isPlaza ? 10 : 7);

	for (int y = 0; y < n; ++y)
	{
		for (int x = 0; x < n; ++x)
		{
			// 울타리 밖 숲길의 나무는 길을 낼 때 이미 심었다(2.5)
			if (tile(x, y) != kGrass || nearRoad(x, y) || nearHouse(x, y) || outsideWall(localX(x), localY(y)))
			{
				continue;
			}
			if (!random.Chance(treePercent))
			{
				continue;
			}

			Tree tree;
			tree.tileX = chunkX * n + x;
			tree.tileY = chunkY * n + y;

			// 숲에는 침엽수가 섞이고, 마을의 나무는 역병 뒤로 절반쯤 말라 죽었다.
			const int roll = random.Range(0, 99);
			if (isWoods)
			{
				tree.kind = static_cast<unsigned char>(roll < 50 ? 0 : (roll < 82 ? 1 : 2));
			}
			else
			{
				tree.kind = static_cast<unsigned char>(roll < 58 ? 0 : 2);
			}

			chunk.trees.push_back(tree);
			tile(x, y) = kTree;
		}
	}

	// ── 4.5 숲 가장자리 ────────────────────────────────────────
	// 숲(kWoods)은 땅이 수풀로 그리지만, 길가에서 보면 나무가 서 있어야
	// 숲처럼 보인다. 걸을 수 있는 칸에 닿은 수풀만 진짜 나무로 세운다.
	// 숲 속 깊은 곳은 아무도 들어가지 않으므로 액터를 만들지 않는다.
	{
		auto walkableAt = [&](int x, int y)
		{
			if (!inside(x, y))
			{
				return false;
			}
			const unsigned char t = tile(x, y);
			return t == kGrass || t == kPath || t == kPlaza;
		};

		for (int y = 0; y < n; ++y)
		{
			for (int x = 0; x < n; ++x)
			{
				if (tile(x, y) != kWoods)
				{
					continue;
				}
				if (!walkableAt(x - 1, y) && !walkableAt(x + 1, y)
				 && !walkableAt(x, y - 1) && !walkableAt(x, y + 1))
				{
					continue;
				}

				Tree tree;
				tree.tileX = chunkX * n + x;
				tree.tileY = chunkY * n + y;

				// 숲은 침엽수가 우세하다. 마을 안의 마른 나무와 달리 살아 있다.
				const int roll = random.Range(0, 99);
				tree.kind = static_cast<unsigned char>(roll < 30 ? 0 : (roll < 92 ? 1 : 2));

				chunk.trees.push_back(tree);
				tile(x, y) = kTree;
			}
		}
	}

	// ── 5. 들쭉날쭉한 길가 ─────────────────────────────────────
	// 곧은 십자길은 격자처럼 보인다. 길가의 풀밭을 드문드문 흙으로 바꿔
	// 오래 밟혀 넓어진 시골길처럼 만든다.
	for (int y = 0; y < n; ++y)
	{
		for (int x = 0; x < n; ++x)
		{
			if (tile(x, y) == kGrass && !onRoad(x, y) && nearRoad(x, y) && random.Chance(30))
			{
				tile(x, y) = kPath;
			}
		}
	}

	// ── 6. 갇힌 곳 없애기 ──────────────────────────────────────
	// 길에서 출발해 걸을 수 있는 칸을 채워 나간다(상하좌우). 끝까지 닿지 않는 칸이
	// 남으면, 닿은 칸과 닿지 않은 칸 사이에 선 나무를 벤다. 막힌 곳이 없어질 때까지.
	auto walkable = [&](int x, int y)
	{
		const unsigned char t = tile(x, y);
		return t != kHouse && t != kTree && t != kWall && t != kWoods;
	};

	for (int pass = 0; pass < n; ++pass)
	{
		std::vector<unsigned char> reached(static_cast<size_t>(n) * n, 0);
		std::vector<int> frontier;

		for (int y = 0; y < n; ++y)
		{
			for (int x = 0; x < n; ++x)
			{
				// 실제로 길인 칸에서 출발한다. 십자길 자리라도 숲에 묻혔으면 길이 아니다.
				// 안길과 광장도 출발점이다 — 모서리 청크는 십자길이 모두 울타리 밖이다.
				const unsigned char t = tile(x, y);
				if (t == kPath || t == kPlaza)
				{
					reached[y * n + x] = 1;
					frontier.push_back(y * n + x);
				}
			}
		}

		const int stepX[4] = { 1, -1, 0, 0 };
		const int stepY[4] = { 0, 0, 1, -1 };

		while (!frontier.empty())
		{
			const int cell = frontier.back();
			frontier.pop_back();
			for (int k = 0; k < 4; ++k)
			{
				const int nx = cell % n + stepX[k];
				const int ny = cell / n + stepY[k];
				if (inside(nx, ny) && !reached[ny * n + nx] && walkable(nx, ny))
				{
					reached[ny * n + nx] = 1;
					frontier.push_back(ny * n + nx);
				}
			}
		}

		bool cleared = false;
		for (int y = 0; y < n; ++y)
		{
			for (int x = 0; x < n; ++x)
			{
				if (tile(x, y) != kTree)
				{
					continue;
				}

				bool touchesReached = false;
				bool touchesSealed = false;
				for (int k = 0; k < 4; ++k)
				{
					const int nx = x + stepX[k];
					const int ny = y + stepY[k];
					if (!inside(nx, ny) || !walkable(nx, ny))
					{
						continue;
					}
					if (reached[ny * n + nx]) { touchesReached = true; }
					else                      { touchesSealed = true; }
				}

				if (touchesReached && touchesSealed)
				{
					tile(x, y) = kGrass;
					const int globalX = chunkX * n + x;
					const int globalY = chunkY * n + y;
					for (size_t i = 0; i < chunk.trees.size(); ++i)
					{
						if (chunk.trees[i].tileX == globalX && chunk.trees[i].tileY == globalY)
						{
							chunk.trees.erase(chunk.trees.begin() + static_cast<long>(i));
							break;
						}
					}
					cleared = true;
				}
			}
		}

		if (!cleared)
		{
			break;
		}
	}

	// ── 7. 길가에 버려진 것들 ─────────────────────────────────
	if (!isPlaza && random.Chance(isWoods ? 25 : 45))
	{
		Prop prop;
		const int along = random.Range(1, n - 2);
		const bool horizontal = random.Chance(50);
		const int x = horizontal ? along : mid + 1 + random.Range(0, 1);
		const int y = horizontal ? mid + 1 + random.Range(0, 1) : along;
		if (inside(x, y) && !onRoad(x, y) && (tile(x, y) == kGrass || tile(x, y) == kPath))
		{
			prop.tileX = chunkX * n + x;
			prop.tileY = chunkY * n + y;
			prop.kind = random.Chance(60) ? kPropRubble : kPropBarrel;
			chunk.props.push_back(prop);
		}
	}

	// ── 8. 울타리 ──────────────────────────────────────────────
	// 그리기 위해 울타리 칸마다 어느 쪽 벽인지, 모서리인지, 문 기둥인지 적어 둔다.
	for (int y = 0; y < n; ++y)
	{
		for (int x = 0; x < n; ++x)
		{
			if (tile(x, y) != kWall)
			{
				continue;
			}
			const int lx = localX(x);
			const int ly = localY(y);

			Wall wall;
			wall.tileX = chunkX * n + x;
			wall.tileY = chunkY * n + y;
			wall.stone = stoneWall;
			wall.corner = (lx == wallLow || lx == wallHigh) && (ly == wallLow || ly == wallHigh);

			if (ly == wallLow)       { wall.side = kSideNorth; }
			else if (ly == wallHigh) { wall.side = kSideSouth; }
			else if (lx == wallLow)  { wall.side = kSideWest; }
			else                     { wall.side = kSideEast; }

			// 문 바로 옆 칸이 문 기둥이다
			const bool beside = (wall.side == kSideNorth || wall.side == kSideSouth)
			                  ? (lx == roadLow - 2 || lx == roadHigh + 2)
			                  : (ly == roadLow - 2 || ly == roadHigh + 2);
			unsigned char gateBit = 0;
			switch (wall.side)
			{
			case kSideNorth: gateBit = Regions::kGateNorth; break;
			case kSideSouth: gateBit = Regions::kGateSouth; break;
			case kSideWest:  gateBit = Regions::kGateWest;  break;
			default:         gateBit = Regions::kGateEast;  break;
			}
			wall.post = !wall.corner && beside && (gates & gateBit) != 0;

			chunk.walls.push_back(wall);
		}
	}
}

const World::Chunk& World::ChunkAt(int chunkX, int chunkY) const
{
	const long long key = PackKey(chunkX, chunkY);

	auto found = m_Chunks.find(key);
	if (found != m_Chunks.end())
	{
		return found->second;
	}

	Chunk chunk;
	Generate(chunk, chunkX, chunkY);
	return m_Chunks.emplace(key, std::move(chunk)).first->second;
}

unsigned char World::TileAt(int tileX, int tileY) const
{
	const int chunkX = FloorDiv(tileX, kChunkTiles);
	const int chunkY = FloorDiv(tileY, kChunkTiles);
	const int localX = PositiveMod(tileX, kChunkTiles);
	const int localY = PositiveMod(tileY, kChunkTiles);

	return ChunkAt(chunkX, chunkY).tiles[localY * kChunkTiles + localX];
}

bool World::IsBlocked(int tileX, int tileY) const
{
	const unsigned char tile = TileAt(tileX, tileY);
	return tile == kHouse || tile == kTree || tile == kWall || tile == kWoods;
}

void World::CollectWalls(int tileMinX, int tileMinY,
                         int tileMaxX, int tileMaxY,
                         std::vector<Wall>& out) const
{
	const int firstChunkX = FloorDiv(tileMinX, kChunkTiles);
	const int lastChunkX  = FloorDiv(tileMaxX, kChunkTiles);
	const int firstChunkY = FloorDiv(tileMinY, kChunkTiles);
	const int lastChunkY  = FloorDiv(tileMaxY, kChunkTiles);

	for (int chunkY = firstChunkY; chunkY <= lastChunkY; ++chunkY)
	{
		for (int chunkX = firstChunkX; chunkX <= lastChunkX; ++chunkX)
		{
			for (const Wall& wall : ChunkAt(chunkX, chunkY).walls)
			{
				if (wall.tileX >= tileMinX && wall.tileX <= tileMaxX
				 && wall.tileY >= tileMinY && wall.tileY <= tileMaxY)
				{
					out.push_back(wall);
				}
			}
		}
	}
}

void World::CollectProps(int tileMinX, int tileMinY,
                         int tileMaxX, int tileMaxY,
                         std::vector<Prop>& out) const
{
	const int firstChunkX = FloorDiv(tileMinX, kChunkTiles);
	const int lastChunkX  = FloorDiv(tileMaxX, kChunkTiles);
	const int firstChunkY = FloorDiv(tileMinY, kChunkTiles);
	const int lastChunkY  = FloorDiv(tileMaxY, kChunkTiles);

	for (int chunkY = firstChunkY; chunkY <= lastChunkY; ++chunkY)
	{
		for (int chunkX = firstChunkX; chunkX <= lastChunkX; ++chunkX)
		{
			for (const Prop& prop : ChunkAt(chunkX, chunkY).props)
			{
				if (prop.tileX >= tileMinX && prop.tileX <= tileMaxX
				 && prop.tileY >= tileMinY && prop.tileY <= tileMaxY)
				{
					out.push_back(prop);
				}
			}
		}
	}
}

void World::CollectStructures(int tileMinX, int tileMinY,
                              int tileMaxX, int tileMaxY,
                              std::vector<House>& houses,
                              std::vector<Tree>& trees) const
{
	const int firstChunkX = FloorDiv(tileMinX, kChunkTiles);
	const int lastChunkX  = FloorDiv(tileMaxX, kChunkTiles);
	const int firstChunkY = FloorDiv(tileMinY, kChunkTiles);
	const int lastChunkY  = FloorDiv(tileMaxY, kChunkTiles);

	for (int chunkY = firstChunkY; chunkY <= lastChunkY; ++chunkY)
	{
		for (int chunkX = firstChunkX; chunkX <= lastChunkX; ++chunkX)
		{
			const Chunk& chunk = ChunkAt(chunkX, chunkY);

			for (const House& house : chunk.houses)
			{
				// 집은 여러 칸에 걸치므로 범위와 겹치기만 하면 모은다.
				if (house.tileX <= tileMaxX && house.tileX + house.width - 1 >= tileMinX
				 && house.tileY <= tileMaxY && house.tileY + house.depth - 1 >= tileMinY)
				{
					houses.push_back(house);
				}
			}

			for (const Tree& tree : chunk.trees)
			{
				if (tree.tileX >= tileMinX && tree.tileX <= tileMaxX
				 && tree.tileY >= tileMinY && tree.tileY <= tileMaxY)
				{
					trees.push_back(tree);
				}
			}
		}
	}
}
