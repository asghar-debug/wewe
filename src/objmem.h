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
 *  Routines for managing object's memory
 */

#ifndef __INCLUDED_SRC_OBJMEM_H__
#define __INCLUDED_SRC_OBJMEM_H__

#include <iterator>
#include <vector>

#include "objectdef.h"

/* The lists of objects allocated */
extern DROID			*apsDroidLists[MAX_PLAYERS];
extern STRUCTURE		*apsStructLists[MAX_PLAYERS];
extern FEATURE			*apsFeatureLists[MAX_PLAYERS];
extern FLAG_POSITION	*apsFlagPosLists[MAX_PLAYERS];
extern STRUCTURE		*apsExtractorLists[MAX_PLAYERS];
extern BASE_OBJECT		*apsSensorList[1];
extern FEATURE			*apsOilList[1];

/* The list of destroyed objects */
extern BASE_OBJECT	*psDestroyedObj;


/* Abstract base class for iterators over object lists. */
template<typename T>
class ObjectIterator : public std::iterator<std::input_iterator_tag, T, T, T*, T&>
{
public:
	explicit ObjectIterator(
		const bool bSelectedOnly,
		T* firstObject
	);

private:
	T* currentObject;
	bool bSelectedOnly;

	// Iterator implementation
public:
	ObjectIterator<T>& operator++();
	ObjectIterator<T> operator++(int);
	bool operator==(ObjectIterator<T> other) const;
	bool operator!=(ObjectIterator<T> other) const;
	typename ObjectIterator<T>::reference operator*() const;

	template<typename U>
	friend class PlayerObjectIterator;
};

template <typename T>
class PlayerObjectIterator : public std::iterator<std::input_iterator_tag, T, T, T*, T&>
{
public:
	explicit PlayerObjectIterator(
		const unsigned int playerCursor,
		const std::vector<unsigned int> playerIndices,
		const bool bSelectedOnly,
		T** objectList
	);

private:
	unsigned int playerCursor;
	ObjectIterator<T> objIter;

	const std::vector<unsigned int> playerIndices;
	const bool bSelectedOnly;
	T** objectList;

	// Iterator implementation
public:
	PlayerObjectIterator<T>& operator++();
	PlayerObjectIterator<T> operator++(int);
	bool operator==(PlayerObjectIterator<T> other) const;
	bool operator!=(PlayerObjectIterator<T> other) const;
	typename PlayerObjectIterator<T>::reference operator*() const;
};

/* Convenient iterator over the droid lists. */
class Droids
{
public:
	/* Obtains a collection of droids available for the given player. Each player has their
	   own droids that are always included in their droid collection. Other players' droids
	   are available for a player only if the other player has shared their unit controls
	   with the player in question (and if bIncludeShared is true).                         */
	static Droids forPlayer(const unsigned int playerIndex, const bool bIncludeShared, const bool bSelectedOnly);

public:
	PlayerObjectIterator<DROID> begin() const;

	PlayerObjectIterator<DROID> end() const;

	unsigned int count() const;

private:
	explicit Droids(const std::vector<unsigned int> playerIndices, const bool bSelectedOnly);

	const std::vector<unsigned int> playerIndices;
	const bool bSelectedOnly;
};


/* Initialise the object heaps */
bool objmemInitialise();

/* Release the object heaps */
void objmemShutdown();

/* General housekeeping for the object system */
void objmemUpdate();

/// Generates a new, (hopefully) unique object id.
uint32_t generateNewObjectId();
/// Generates a new, (hopefully) unique object id, which all clients agree on.
uint32_t generateSynchronisedObjectId();

/* add the droid to the Droid Lists */
void addDroid(DROID *psDroidToAdd, DROID *pList[MAX_PLAYERS]);

/*destroy a droid */
void killDroid(DROID *psDel);

/* Remove all droids */
void freeAllDroids();

/*Remove a single Droid from its list*/
void removeDroid(DROID *psDroidToRemove, DROID *pList[MAX_PLAYERS]);

/*Removes all droids that may be stored in the mission lists*/
void freeAllMissionDroids();

/*Removes all droids that may be stored in the limbo lists*/
void freeAllLimboDroids();

/* add the structure to the Structure Lists */
void addStructure(STRUCTURE *psStructToAdd);

/* Destroy a structure */
void killStruct(STRUCTURE *psDel);

/* Remove all structures */
void freeAllStructs();

/*Remove a single Structure from a list*/
void removeStructureFromList(STRUCTURE *psStructToRemove, STRUCTURE *pList[MAX_PLAYERS]);

/* add the feature to the Feature Lists */
void addFeature(FEATURE *psFeatureToAdd);

/* Destroy a feature */
void killFeature(FEATURE *psDel);

/* Remove all features */
void freeAllFeatures();

/* Create a new Flag Position */
bool createFlagPosition(FLAG_POSITION **ppsNew, UDWORD player);
/* add the Flag Position to the Flag Position Lists */
void addFlagPosition(FLAG_POSITION *psFlagPosToAdd);
/* Remove a Flag Position from the Lists */
void removeFlagPosition(FLAG_POSITION *psDel);
// free all flag positions
void freeAllFlagPositions();

// Find a base object from it's id
BASE_OBJECT *getBaseObjFromData(unsigned id, unsigned player, OBJECT_TYPE type);
BASE_OBJECT *getBaseObjFromId(UDWORD id);

UDWORD getRepairIdFromFlag(FLAG_POSITION *psFlag);

void objCount(int *droids, int *structures, int *features);

#ifdef DEBUG
void checkFactoryFlags();
#endif

#endif // __INCLUDED_SRC_OBJMEM_H__
