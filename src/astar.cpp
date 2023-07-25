/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2020  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/** @file
 *  A* based path finding
 *  See http://en.wikipedia.org/wiki/A*_search_algorithm for more information.
 *  How this works:
 *  * First time (in a given tick)  that some droid  wants to pathfind  to a particular
 *    destination,  the A*  algorithm from source to  destination is used.  The desired
 *    destination,  and the nearest  reachable point  to the  destination is saved in a
 *    Context.
 *  * Second time (in a given tick)  that some droid wants to  pathfind to a particular
 *    destination,  the appropriate  Context is found,  and the A* algorithm is used to
 *    find a path from the nearest reachable point to the destination  (which was saved
 *    earlier), to the source.
 *  * Subsequent times  (in a given tick) that some droid wants to pathfind to a parti-
 *    cular destination,  the path is looked up in appropriate Context.  If the path is
 *    not already known,  the A* weights are adjusted, and the previous A*  pathfinding
 *    is continued until the new source is reached.  If the new source is  not reached,
 *    the droid is  on a  different island than the previous droid,  and pathfinding is
 *    restarted from the first step.
 *  Up to 30 pathfinding maps from A* are cached, in a LRU list. The PathNode heap con-
 *  tains the  priority-heap-sorted  nodes which are to be explored.  The path back  is
 *  stored in the PathExploredTile 2D array of tiles.
 */

#include <algorithm>

#ifndef WZ_TESTING
#include "lib/framework/frame.h"

#include "fpath.h"
#include "astar.h"
#include "map.h"
#endif


// Convert a direction into an offset
// dir 0 => x = 0, y = -1
static constexpr Vector2i aDirOffset[] =
{
	Vector2i(0, 1),
	Vector2i(-1, 1),
	Vector2i(-1, 0),
	Vector2i(-1, -1),
	Vector2i(0, -1),
	Vector2i(1, -1),
	Vector2i(1, 0),
	Vector2i(1, 1),
};

static constexpr cost_t MaxPathCost = ~cost_t(0);

bool isTileBlocked(const PathfindContext& context, int x, int y) {
	if (context.dstIgnore.isNonblocking(x, y))
	{
		return false;  // The path is actually blocked here by a structure, but ignore it since it's where we want to go (or where we came from).
	}
	// Not sure whether the out-of-bounds check is needed, can only happen if pathfinding is started on a blocking tile (or off the map).
	return x < 0 || y < 0 || x >= context.width || y >= context.height || context.blockingMap->map[x + y * context.width];
}

bool PathBlockingMap::operator ==(PathBlockingType const &z) const
{
	return type.gameTime == z.gameTime &&
		   fpathIsEquivalentBlocking(type.propulsion, type.owner, static_cast<FPATH_MOVETYPE>(type.moveType),
									 z.propulsion,    z.owner,    static_cast<FPATH_MOVETYPE>(z.moveType));
}

PathCoord PathBlockingMap::worldToMap(int x, int y) const{
	return PathCoord(x >> tileShift, y >> tileShift);
}

PathCoord PathBlockingMap::mapToWorld(int x, int y) const {
	return PathCoord(x << tileShift, y << tileShift);
}

bool PathfindContext::matches(std::shared_ptr<PathBlockingMap> &blockingMap_, PathCoord tileS_, PathNonblockingArea dstIgnore_, bool reverse_) const
{
	// Must check myGameTime == blockingMap_->type.gameTime, otherwise blockingMap could be a deleted pointer which coincidentally compares equal to the valid pointer blockingMap_.
	return myGameTime == blockingMap_->type.gameTime && blockingMap == blockingMap_ && tileS == tileS_ && dstIgnore == dstIgnore_ && reverse == reverse_;
}

void PathfindContext::assign(std::shared_ptr<PathBlockingMap> &blockingMap_, PathCoord tileS_, PathNonblockingArea dstIgnore_, bool reverse_)
{
	ASSERT_OR_RETURN(, blockingMap_->width && blockingMap_->height, "Incorrect size of blocking map");
	blockingMap = blockingMap_;
	tileS = tileS_;
	dstIgnore = dstIgnore_;
	myGameTime = blockingMap->type.gameTime;
	reverse = reverse_;
	nodes.clear();

	// Make the iteration not match any value of iteration in map.
	if (++iteration == 0xFFFF)
	{
		map.clear();  // There are no values of iteration guaranteed not to exist in map, so clear the map.
		iteration = 0;
	}
	width = blockingMap_->width;
	height = blockingMap_->height;
	map.resize(static_cast<size_t>(width) * static_cast<size_t>(height));  // Allocate space for map, if needed.
}

/** Get the nearest entry in the open list
 */
/// Takes the current best node, and removes from the node heap.
static inline PathNode fpathTakeNode(std::vector<PathNode> &nodes)
{
	// find the node with the lowest distance
	// if equal totals, give preference to node closer to target
	PathNode ret = nodes.front();

	// remove the node from the list
	std::pop_heap(nodes.begin(), nodes.end());  // Move the best node from the front of nodes to the back of nodes, preserving the heap properties, setting the front to the next best node.
	nodes.pop_back();                           // Pop the best node (which we will be returning).

	return ret;
}

/** Estimate the distance to the target point
 */
static inline unsigned WZ_DECL_PURE fpathEstimate(PathCoord s, PathCoord f)
{
	// Cost of moving horizontal/vertical = 70*2, cost of moving diagonal = 99*2, 99/70 = 1.41428571... ≈ √2 = 1.41421356...
	unsigned xDelta = abs(s.x - f.x), yDelta = abs(s.y - f.y);
	return std::min(xDelta, yDelta) * (198 - 140) + std::max(xDelta, yDelta) * 140;
}
static inline unsigned WZ_DECL_PURE fpathGoodEstimate(PathCoord s, PathCoord f)
{
	// Cost of moving horizontal/vertical = 70*2, cost of moving diagonal = 99*2, 99/70 = 1.41428571... ≈ √2 = 1.41421356...
	return iHypot((s.x - f.x) * 140, (s.y - f.y) * 140);
}

/// Helper structure to extract blocking and cost information for PF wave propagation.
/// It must extract and cache data for direct access.
struct CostLayer {
	CostLayer(const PathfindContext& pfc) : pfc(pfc)
	{
		assert(pfc.blockingMap);
		pBlockingMap = pfc.blockingMap.get();
	}

	cost_t cost(int x, int y) const {
		return isDangerous(x, y) ? 5 : 1;
	}

	bool isBlocked(int x, int y) const
	{
		if (pfc.dstIgnore.isNonblocking(x, y))
		{
			return false;  // The path is actually blocked here by a structure, but ignore it since it's where we want to go (or where we came from).
		}
		// Not sure whether the out-of-bounds check is needed, can only happen if pathfinding is started on a blocking tile (or off the map).
		return x < 0 || y < 0 || x >= mapWidth || y >= mapHeight || pBlockingMap->isBlocked(x, y);
	}

	bool isNonblocking(int x, int y) const {
		return pfc.dstIgnore.isNonblocking(x, y);
	}

	bool isDangerous(int x, int y) const
	{
		return !pBlockingMap->dangerMap.empty() && pBlockingMap->isDangerous(x, y);
	}

	const PathfindContext& pfc;
	/// Direct pointer to blocking map.
	PathBlockingMap* pBlockingMap = nullptr;
};

/** Generate a new node
 */
template <class Predicate, class CostLayer>
bool fpathNewNode(PathfindContext &context, Predicate& predicate,
	const CostLayer& costLayer,
	PathCoord pos, cost_t prevDist, PathCoord prevPos)
{
	ASSERT_OR_RETURN(false, (unsigned)pos.x < (unsigned)context.width && (unsigned)pos.y < (unsigned)context.height, "X (%d) or Y (%d) coordinate for path finding node is out of range!", pos.x, pos.y);

	unsigned estimateCost = predicate.estimateCost(pos);
	// Create the node.
	PathNode node;
	cost_t costFactor = costLayer.cost(pos.x, pos.y);
	node.p = pos;
	node.dist = prevDist + fpathEstimate(prevPos, pos) * costFactor;
	node.est = node.dist + estimateCost;

	Vector2i delta = Vector2i(pos.x - prevPos.x, pos.y - prevPos.y) * 64;
	bool isDiagonal = delta.x && delta.y;

	PathExploredTile &expl = context.map[pos.x + pos.y * context.width];
	if (expl.iteration == context.iteration)
	{
		if (expl.visited)
		{
			return false;  // Already visited this tile. Do nothing.
		}
		Vector2i deltaA = delta;
		Vector2i deltaB = Vector2i(expl.dx, expl.dy);
		Vector2i deltaDelta = deltaA - deltaB;  // Vector pointing from current considered source tile leading to pos, to the previously considered source tile leading to pos.
		if (abs(deltaDelta.x) + abs(deltaDelta.y) == 64)
		{
			// prevPos is tile A or B, and pos is tile P. We were previously called with prevPos being tile B or A, and pos tile P.
			// We want to find the distance to tile P, taking into account that the actual shortest path involves coming from somewhere between tile A and tile B.
			// +---+---+
			// |   | P |
			// +---+---+
			// | A | B |
			// +---+---+
			cost_t distA = node.dist - (isDiagonal ? 198 : 140) * costFactor; // If isDiagonal, node is A and expl is B.
			cost_t distB = expl.dist - (isDiagonal ? 140 : 198) * costFactor;
			if (!isDiagonal)
			{
				std::swap(distA, distB);
				std::swap(deltaA, deltaB);
			}
			int gradientX = int(distB - distA) / costFactor;
			if (gradientX > 0 && gradientX <= 98)  // 98 = floor(140/√2), so gradientX <= 98 is needed so that gradientX < gradientY.
			{
				// The distance gradient is now known to be somewhere between the direction from A to P and the direction from B to P.
				static constexpr uint8_t gradYLookup[99] = {140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 139, 139, 139, 139, 139, 139, 139, 139, 139, 138, 138, 138, 138, 138, 138, 137, 137, 137, 137, 137, 136, 136, 136, 136, 135, 135, 135, 134, 134, 134, 134, 133, 133, 133, 132, 132, 132, 131, 131, 130, 130, 130, 129, 129, 128, 128, 127, 127, 126, 126, 126, 125, 125, 124, 123, 123, 122, 122, 121, 121, 120, 119, 119, 118, 118, 117, 116, 116, 115, 114, 113, 113, 112, 111, 110, 110, 109, 108, 107, 106, 106, 105, 104, 103, 102, 101, 100};
				int gradientY = gradYLookup[gradientX];  // = sqrt(140² -  gradientX²), rounded to nearest integer
				cost_t distP = gradientY * costFactor + distB;
				node.est -= node.dist - distP;
				node.dist = distP;
				delta = (deltaA * gradientX + deltaB * (gradientY - gradientX)) / gradientY;
			}
		}
		if (expl.dist <= node.dist)
		{
			return false;  // A different path to this tile is shorter.
		}
	}

	// Remember where we have been, and remember the way back.
	expl.iteration = context.iteration;
	expl.dx = delta.x;
	expl.dy = delta.y;
	expl.dist = node.dist;
	expl.visited = false;

	// Add the node to the node heap.
	context.nodes.push_back(node);                               // Add the new node to nodes.
	std::push_heap(context.nodes.begin(), context.nodes.end());  // Move the new node to the right place in the heap.
	return true;
}

/// Recalculates estimates to new tileF tile.
static void fpathAStarReestimate(PathfindContext &context, PathCoord tileF)
{
	for (auto &node : context.nodes)
	{
		node.est = node.dist + fpathGoodEstimate(node.p, tileF);
	}

	// Changing the estimates breaks the heap ordering. Fix the heap ordering.
	std::make_heap(context.nodes.begin(), context.nodes.end());
}


/// A predicate for searching path to a single point.
struct NearestSearchPredicate {
	/// Target tile.
	PathCoord goal;
	/// Nearest coordinates of the wave to the target tile.
	PathCoord nearestCoord {0, 0};

	/// Nearest distance to the target.
	cost_t nearestDist = MaxPathCost;

	NearestSearchPredicate(const PathCoord& goal) : goal(goal) { }

	bool isGoal(const PathNode& node) {
		if (node.p == goal)
		{
			// reached the target
			nearestCoord = node.p;
			nearestDist = 0;
			return true;
		} else if (node.est - node.dist < nearestDist)
		{
			nearestCoord = node.p;
			nearestDist = node.est - node.dist;
		}
		return false;
	}

	unsigned estimateCost(const PathCoord& pos) const {
		return fpathGoodEstimate(pos, goal);
	}

	void clear() {
		nearestCoord = {0, 0};
		nearestDist = MaxPathCost;
	}
};

struct ExplorationReport {
	bool success = false;
	size_t tilesExplored = 0;
	cost_t cost = 0;

	operator bool() const {
		return success;
	}
};

/// Runs A* wave propagation for 8 neighbors pattern.
/// Target is checked using predicate object.
/// @returns true if search wave has reached the goal or false if
/// 	the wave has collapsed before reaching the goal.
template <class Predicate, class CostLayer>
static ExplorationReport fpathAStarExplore(PathfindContext &context, Predicate& predicate, CostLayer& costLayer)
{
	ExplorationReport report;
	constexpr int adjacency = 8;
	while (!context.nodes.empty())
	{
		PathNode node = fpathTakeNode(context.nodes);
		report.tilesExplored++;
		report.cost = node.dist;

		PathExploredTile& tile = context.tile(node.p);
		if (context.isTileVisited(tile))
			continue;
		tile.visited = true;

		if (predicate.isGoal(node)) {
			report.success = true;
			break;
		}

		/*
		   5  6  7
		     \|/
		   4 -I- 0
		     /|\
		   3  2  1
		   odd:orthogonal-adjacent tiles even:non-orthogonal-adjacent tiles
		*/

		// Cache adjacent states from blocking map. Saves some cycles for diagonal checks for corners.
		bool blocking[adjacency];
		bool ignoreBlocking[adjacency];
		for (unsigned dir = 0; dir < adjacency; ++dir) {
			int x = node.p.x + aDirOffset[dir].x;
			int y = node.p.y + aDirOffset[dir].y;
			blocking[dir] = costLayer.isBlocked(x, y);
			ignoreBlocking[dir] = costLayer.isNonblocking(x, y);
		}

		bool ignoreCenter = costLayer.isNonblocking(node.p.x, node.p.y);

		// loop through possible moves in 8 directions to find a valid move
		for (unsigned dir = 0; dir < adjacency; ++dir)
		{
			// See if the node is a blocking tile
			if (blocking[dir])
				continue;
			if (dir % 2 != 0 && !ignoreCenter && !ignoreBlocking[dir])
			{
				// Turn CCW.
				if (blocking[(dir + 1) % 8])
					continue;
				// Turn CW.
				if (blocking[(dir + 7) % 8])
					continue;
			}

			// Try a new location
			int x = node.p.x + aDirOffset[dir].x;
			int y = node.p.y + aDirOffset[dir].y;

			PathCoord newPos(x, y);

			// Now insert the point into the appropriate list, if not already visited.
			fpathNewNode(context, predicate, costLayer, newPos, node.dist, node.p);
		}
	}

	return report;
}

/// Traces path from search tree.
/// @param src - starting point of a search
/// @param dst - final point, when tracing stops.
static ASR_RETVAL fpathTracePath(const PathfindContext& context, PathCoord src, PathCoord dst, std::vector<Vector2i>& path) {
	ASR_RETVAL retval = ASR_OK;
	path.clear();
	Vector2i newP(0, 0);
	for (Vector2i p(world_coord(src.x) + TILE_UNITS / 2, world_coord(src.y) + TILE_UNITS / 2); true; p = newP)
	{
		ASSERT_OR_RETURN(ASR_FAILED, worldOnMap(p.x, p.y), "Assigned XY coordinates (%d, %d) not on map!", (int)p.x, (int)p.y);
		ASSERT_OR_RETURN(ASR_FAILED, path.size() < (static_cast<size_t>(mapWidth) * static_cast<size_t>(mapHeight)), "Pathfinding got in a loop.");

		path.push_back(p);

		const PathExploredTile &tile = context.map[map_coord(p.x) + map_coord(p.y) * mapWidth];
		newP = p - Vector2i(tile.dx, tile.dy) * (TILE_UNITS / 64);
		Vector2i mapP = map_coord(newP);
		int xSide = newP.x - world_coord(mapP.x) > TILE_UNITS / 2 ? 1 : -1; // 1 if newP is on right-hand side of the tile, or -1 if newP is on the left-hand side of the tile.
		int ySide = newP.y - world_coord(mapP.y) > TILE_UNITS / 2 ? 1 : -1; // 1 if newP is on bottom side of the tile, or -1 if newP is on the top side of the tile.
		if (isTileBlocked(context, mapP.x + xSide, mapP.y))
		{
			newP.x = world_coord(mapP.x) + TILE_UNITS / 2; // Point too close to a blocking tile on left or right side, so move the point to the middle.
		}
		if (isTileBlocked(context, mapP.x, mapP.y + ySide))
		{
			newP.y = world_coord(mapP.y) + TILE_UNITS / 2; // Point too close to a blocking tile on rop or bottom side, so move the point to the middle.
		}
		if (map_coord(p) == Vector2i(dst.x, dst.y) || p == newP)
		{
			break;  // We stopped moving, because we reached the destination or the closest reachable tile to dst. Give up now.
		}
	}
	return retval;
}

ASR_RETVAL fpathAStarRoute(std::list<PathfindContext>& fpathContexts,
	MOVE_CONTROL *psMove, PATHJOB *psJob)
{
	ASSERT_OR_RETURN(ASR_FAILED, psMove, "Null psMove");
	ASSERT_OR_RETURN(ASR_FAILED, psJob, "Null psMove");

	ASR_RETVAL retval = ASR_OK;

	bool mustReverse = false;

	const PathCoord tileOrig = psJob->blockingMap->worldToMap(psJob->origX, psJob->origY);
	const PathCoord tileDest = psJob->blockingMap->worldToMap(psJob->destX, psJob->destY);

	if (psJob->blockingMap->isBlocked(tileOrig.x, tileOrig.y)) {
		debug(LOG_NEVER, "Initial tile blocked (%d;%d)", tileOrig.x, tileOrig.y);
	}
	if (psJob->blockingMap->isBlocked(tileDest.x, tileDest.y)) {
		debug(LOG_NEVER, "Destination tile blocked (%d;%d)", tileOrig.x, tileOrig.y);
	}
	const PathNonblockingArea dstIgnore(psJob->dstStructure);

	NearestSearchPredicate pred(tileOrig);

	PathCoord endCoord;

	// Caching reverse searches.
	std::list<PathfindContext>::iterator contextIterator = fpathContexts.begin();
	for (; contextIterator != fpathContexts.end(); ++contextIterator)
	{
		PathfindContext& pfContext = *contextIterator;
		if (!pfContext.matches(psJob->blockingMap, tileDest, dstIgnore, /*reverse*/true))
		{
			// This context is not for the same droid type and same destination.
			continue;
		}

		const PathExploredTile& pt = pfContext.tile(tileOrig);
		// We have tried going to tileDest before.
		if (pfContext.isTileVisited(pt))
		{
			// Already know the path from orig to dest.
			endCoord = tileOrig;
		}
		else if (pfContext.nodes.empty()) {
			// Wave has already collapsed. Consequent attempt to search will exit immediately.
			// We can be here only if there is literally no path existing.
			continue;
		}
		else
		{
			CostLayer costLayer(pfContext);
			// Need to find the path from orig to dest, continue previous exploration.
			fpathAStarReestimate(pfContext, pred.goal);
			pred.clear();
			ExplorationReport report = fpathAStarExplore(pfContext, pred, costLayer);
			if (report) {
				endCoord = pred.nearestCoord;
				// Found the path! Don't search more contexts.
				break;
			}
		}
	}

	if (contextIterator == fpathContexts.end())
	{
		// We did not find an appropriate context. Make one.
		if (fpathContexts.size() < 30)
		{
			fpathContexts.emplace_back();
		}
		contextIterator--;
		PathfindContext& pfContext = fpathContexts.back();

		// Init a new context, overwriting the oldest one if we are caching too many.
		// We will be searching from orig to dest, since we don't know where the nearest reachable tile to dest is.
		pfContext.assign(psJob->blockingMap, tileDest, dstIgnore, true);
		pred.clear();

		CostLayer costLayer(pfContext);
		// Add the start point to the open list
		bool started = fpathNewNode(pfContext, pred, costLayer, tileDest, 0, tileDest);
		ASSERT(started, "fpathNewNode failed to add node.");

		ExplorationReport report = fpathAStarExplore(pfContext, pred, costLayer);
		if (!report) {
			debug(LOG_NEVER, "Failed to find path (%d;%d)-(%d;%d)", tileOrig.x, tileOrig.y, tileDest.x, tileDest.y);
		}
		endCoord = pred.nearestCoord;
	}

	PathfindContext &context = *contextIterator;

	// return the nearest route if no actual route was found
	if (endCoord != pred.goal)
	{
		retval = ASR_NEAREST;
	}


	static std::vector<Vector2i> path;  // Declared static to save allocations.
	ASR_RETVAL traceRet = fpathTracePath(context, endCoord, tileDest, path);
	if (traceRet != ASR_OK)
		return traceRet;

	if (retval == ASR_OK)
	{
		// Found exact path, so use exact coordinates for last point, no reason to lose precision
		Vector2i v(psJob->destX, psJob->destY);
		if (mustReverse)
		{
			path.front() = v;
		}
		else
		{
			path.back() = v;
		}
	}

	// Allocate memory
	psMove->asPath.resize(path.size());

	// get the route in the correct order
	// If as I suspect this is to reverse the list, then it's my suspicion that
	// we could route from destination to source as opposed to source to
	// destination. We could then save the reversal. to risky to try now...Alex M
	//
	// The idea is impractical, because you can't guarentee that the target is
	// reachable. As I see it, this is the reason why psNearest got introduced.
	// -- Dennis L.
	//
	// If many droids are heading towards the same destination, then destination
	// to source would be faster if reusing the information in nodeArray. --Cyp
	if (mustReverse)
	{
		// Copy the list, in reverse.
		std::copy(path.rbegin(), path.rend(), psMove->asPath.data());
	}
	else
	{
		// Copy the list.
		std::copy(path.begin(), path.end(), psMove->asPath.data());
	}

	psMove->destination = psMove->asPath[path.size() - 1];

	// Move context to beginning of last recently used list.
	if (contextIterator != fpathContexts.begin())  // Not sure whether or not the splice is a safe noop, if equal.
	{
		fpathContexts.splice(fpathContexts.begin(), fpathContexts, contextIterator);
	}

	return retval;
}

void PathMapCache::clear()
{
	fpathBlockingMaps.clear();
}

struct I32Checksum {
	uint32_t factor = 0;
	uint32_t checksum = 0;

	I32Checksum& operator += (bool value) {
		checksum ^= value * (factor = 3 * factor + 1);
		return *this;
	}

	operator uint32_t() const {
		return checksum;
	}
};

uint32_t bitmapChecksum(const std::vector<bool>& map) {
	I32Checksum checksum;
	for (auto v: map)
		checksum += v;
	return checksum;
}

void fillBlockingMap(PathBlockingMap& blockMap, PathBlockingType type) {
	// blockMap now points to an empty map with no data. Fill the map.
	blockMap.type = type;
	std::vector<bool> &map = blockMap.map;
	map.resize(static_cast<size_t>(mapWidth) * static_cast<size_t>(mapHeight));
	auto moveType = static_cast<FPATH_MOVETYPE>(type.moveType);
	for (int y = 0; y < mapHeight; ++y) {
		for (int x = 0; x < mapWidth; ++x)
			map[x + y * mapWidth] = fpathBaseBlockingTile(x, y, type.propulsion, type.owner, moveType);
	}
	std::vector<bool> &dangerMap = blockMap.dangerMap;
	dangerMap.resize(static_cast<size_t>(mapWidth) * static_cast<size_t>(mapHeight));
	if (!isHumanPlayer(type.owner) && type.moveType == FMT_MOVE)
	{
		for (int y = 0; y < mapHeight; ++y) {
			for (int x = 0; x < mapWidth; ++x)
				dangerMap[x + y * mapWidth] = auxTile(x, y, type.owner) & AUXBITS_THREAT;
		}
	}
	blockMap.width = mapWidth;
	blockMap.height = mapHeight;
	blockMap.tileShift = TILE_SHIFT;
}

void PathMapCache::assignBlockingMap(PATHJOB& psJob)
{
	if (fpathCurrentGameTime != gameTime)
	{
		// New tick, remove maps which are no longer needed.
		fpathCurrentGameTime = gameTime;
		clear();
	}

	// Figure out which map we are looking for.
	PathBlockingType type;
	type.gameTime = gameTime;
	type.propulsion = psJob.propulsion;
	type.owner = psJob.owner;
	type.moveType = psJob.moveType;

	// Find the map.
	auto i = std::find_if(fpathBlockingMaps.begin(), fpathBlockingMaps.end(), [&](std::shared_ptr<PathBlockingMap> const &ptr) {
		return *ptr == type;
	});
	if (i == fpathBlockingMaps.end())
	{
		// Didn't find the map, so i does not point to a map.
		auto blockMap = std::make_shared<PathBlockingMap>();
		fpathBlockingMaps.push_back(blockMap);
		fillBlockingMap(*blockMap, type);
		debug(LOG_NEVER, "blockingMap(%d,%d,%d,%d) = %08X %08X", gameTime, psJob.propulsion, psJob.owner, psJob.moveType,
			bitmapChecksum(blockMap->map), bitmapChecksum(blockMap->dangerMap));
		psJob.blockingMap = blockMap;
	}
	else
	{
		debug(LOG_NEVER, "blockingMap(%d,%d,%d,%d) = cached", gameTime, psJob.propulsion, psJob.owner, psJob.moveType);
		ASSERT_OR_RETURN(, *i != nullptr, "Found null map pointer in cache");
		psJob.blockingMap = *i;
	}
}
