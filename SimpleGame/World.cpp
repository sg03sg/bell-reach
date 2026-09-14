#include "stdafx.h"
#include "World.h"

#include <algorithm>
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

	ChunkRandom random(HashCoords(chunkX, chunkY, m_Seed));

	auto tile = [&chunk, n](int x, int y) -> unsigned char&
	{
		return chunk.tiles[y * n + x];
	};
	auto inside = [n](int x, int y)
	{
		return x >= 0 && y >= 0 && x < n && y < n;
	};
	auto onRoad = [mid](int x, int y)
	{
		return x == mid - 1 || x == mid || y == mid - 1 || y == mid;
	};

	// 마을 안에서 이 청크가 맡은 자리
	const int roleX = PositiveMod(chunkX, kVillageChunks);
	const int roleY = PositiveMod(chunkY, kVillageChunks);
	const bool isPlaza = (roleX == 1 && roleY == 1);
	const bool isWoods = (roleX != 1 && roleY != 1);

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
				const unsigned char t = tile(x, y);
				if (footprint ? (t != kGrass) : (t == kHouse || t == kTree))
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
			if (inside(propX, propY) && tile(propX, propY) != kHouse && tile(propX, propY) != kTree)
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
			if (tile(x, y) != kGrass || nearRoad(x, y) || nearHouse(x, y))
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
		return t != kHouse && t != kTree;
	};

	for (int pass = 0; pass < n; ++pass)
	{
		std::vector<unsigned char> reached(static_cast<size_t>(n) * n, 0);
		std::vector<int> frontier;

		for (int y = 0; y < n; ++y)
		{
			for (int x = 0; x < n; ++x)
			{
				if (onRoad(x, y))
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
		if (inside(x, y) && !onRoad(x, y) && tile(x, y) != kHouse && tile(x, y) != kTree)
		{
			prop.tileX = chunkX * n + x;
			prop.tileY = chunkY * n + y;
			prop.kind = random.Chance(60) ? kPropRubble : kPropBarrel;
			chunk.props.push_back(prop);
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
	return tile == kHouse || tile == kTree;
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
