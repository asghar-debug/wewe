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
/*
	KeyMap.c
	Alex McLean
	Pumpkin Studios, EIDOS Interactive.
	Internal Use Only
	-----------------

	Handles the assignment of functions to keys.
*/

#include <string.h>

#include "lib/framework/frame.h"
#include "lib/framework/input.h"
#include "lib/netplay/netplay.h"
#include "lib/gamelib/gtime.h"
#include "keymap.h"
#include "qtscript.h"
#include "console.h"
#include "keybind.h"
#include "display3d.h"
#include "keymap.h"
#include "keyedit.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

static UDWORD asciiKeyCodeToTable(KEY_CODE code);
static KEY_CODE getQwertyKey();

// ----------------------------------------------------------------------------------
InputContexts InputContext::contexts = InputContexts();

static const unsigned int MAX_ICONTEXT_PRIORITY = std::numeric_limits<unsigned int>::max();
const InputContext InputContext::ALWAYS_ACTIVE = { MAX_ICONTEXT_PRIORITY, InputContext::State::ACTIVE,    N_("Global Hotkeys") };
const InputContext InputContext::BACKGROUND    = { 0,                     InputContext::State::ACTIVE,    N_("Other Hotkeys") };
const InputContext InputContext::GAMEPLAY      = { 1,                     InputContext::State::ACTIVE,    N_("Gameplay") };
const InputContext InputContext::RADAR         = { { 2, 0 },              InputContext::State::ACTIVE,    N_("Radar") };
const InputContext InputContext::__DEBUG       = { MAX_ICONTEXT_PRIORITY, InputContext::State::INACTIVE,  N_("Debug") };

static unsigned int inputCtxIndexCounter = 0;
InputContext::InputContext(const ContextPriority priority, const State initialState, const char* const displayName)
	: priority(priority)
	, index(inputCtxIndexCounter++)
	, displayName(displayName)
	, defaultState(initialState)
{
	contexts.push_back(*this);
}

const InputContexts InputContext::getAllContexts()
{
	return contexts;
}

void InputManager::setContextState(const InputContext& context, const InputContext::State newState)
{
	contextStates[context.index] = newState;
	bMappingsSortOrderDirty = true;
}

bool InputManager::isContextActive(const InputContext& context) const
{
	const auto state = contextStates[context.index];
	return state != InputContext::State::INACTIVE;
}

unsigned int InputManager::getContextPriority(const InputContext& context) const
{
	switch (contextStates[context.index])
	{
	case InputContext::State::PRIORITIZED:
		return context.priority.prioritized;
	case InputContext::State::ACTIVE:
		return context.priority.active;
	case InputContext::State::INACTIVE:
	default:
		return 0;
	}
}

const std::string InputContext::getDisplayName() const
{
	return displayName;
}

bool operator==(const InputContext& lhs, const InputContext& rhs)
{
	return lhs.index == rhs.index;
}

bool operator!=(const InputContext& lhs, const InputContext& rhs)
{
	return !(lhs == rhs);
}

void InputManager::resetContextStates()
{
	const auto contexts = InputContext::getAllContexts();
	contextStates = std::vector<InputContext::State>(contexts.size(), InputContext::State::INACTIVE);
	for (const InputContext& context : contexts)
	{
		contextStates[context.index] = context.defaultState;
	}
	bMappingsSortOrderDirty = true;
}

void InputManager::makeAllContextsInactive()
{
	for (const InputContext& context : InputContext::getAllContexts())
	{
		if (context != InputContext::ALWAYS_ACTIVE)
		{
			setContextState(context, InputContext::State::INACTIVE);
		}
	}
	bMappingsSortOrderDirty = true;
}

KeyMapping* InputManager::addMapping(const KEY_CODE meta, const KeyMappingInput input, const KeyAction action, const MappableFunction function, const KeyMappingSlot slot)
{
	/* Make sure the meta key is the left variant */
	KEY_CODE leftMeta = meta;
	if (meta == KEY_RCTRL)
	{
		leftMeta = KEY_LCTRL;
	}
	else if (meta == KEY_RALT)
	{
		leftMeta = KEY_LALT;
	}
	else if (meta == KEY_RSHIFT)
	{
		leftMeta = KEY_LSHIFT;
	}
	else if (meta == KEY_RMETA)
	{
		leftMeta = KEY_LMETA;
	}

	/* Figure out what we are trying to bind */
	const KeyFunctionInfo* info = keyFunctionInfoByFunction(function);
	ASSERT_OR_RETURN(nullptr, info != nullptr, "Could not find key function info for the function while adding new mapping!");

	/* Create the mapping as the last element in the list */
	keyMappings.push_back({
		info,
		gameTime,
		leftMeta,
		input,
		action,
		slot
	});

	/* Invalidate the sorting order and return the newly created mapping */
	bMappingsSortOrderDirty = true;
	return &keyMappings.back();
}

KeyMapping* InputManager::getMappingFromFunction(const MappableFunction function, const KeyMappingSlot slot)
{
	auto mapping = std::find_if(keyMappings.begin(), keyMappings.end(), [function, slot](KeyMapping const& mapping) {
		return mapping.info->function == function && mapping.slot == slot;
	});
	return mapping != keyMappings.end() ? &*mapping : nullptr;
}

std::vector<KeyMapping*> InputManager::findMappingsForInput(const KEY_CODE meta, const KeyMappingInput input)
{
	std::vector<KeyMapping*> matches;
	for (auto& mapping : keyMappings)
	{
		if (mapping.metaKeyCode == meta && mapping.input == input)
		{
			matches.push_back(&mapping);
		}
	}

	return matches;
}

std::vector<KeyMapping> InputManager::removeConflictingMappings(const KEY_CODE meta, const KeyMappingInput input, const InputContext context)
{
	/* Find any mapping with same keys */
	const auto matches = findMappingsForInput(meta, input);
	std::vector<KeyMapping> conflicts;
	for (KeyMapping* psMapping : matches)
	{
		/* Clear only if the mapping is for an assignable binding. Do not clear if there is no conflict (different context) */
		const bool bConflicts = psMapping->info->context == context;
		if (psMapping->info->type == KeyMappingType::ASSIGNABLE && bConflicts)
		{
			conflicts.push_back(*psMapping);
			removeMapping(psMapping);
		}
	}

	return conflicts;
}

void InputManager::shutdown()
{
	keyMappings.clear();
}

void InputManager::clearAssignableMappings()
{
	keyMappings.remove_if([](const KeyMapping& mapping) {
		return mapping.info->type == KeyMappingType::ASSIGNABLE;
	});
}

const std::list<KeyMapping> InputManager::getAllMappings() const
{
	return keyMappings;
}

// ----------------------------------------------------------------------------------
bool KeyMappingInput::isPressed() const
{
	switch (source) {
	case KeyMappingInputSource::KEY_CODE:
		return keyPressed(value.keyCode);
	case KeyMappingInputSource::MOUSE_KEY_CODE:
		return mousePressed(value.mouseKeyCode);
	default:
		return false;
	}
}

bool KeyMappingInput::isDown() const
{
	switch (source) {
	case KeyMappingInputSource::KEY_CODE:
		return keyDown(value.keyCode);
	case KeyMappingInputSource::MOUSE_KEY_CODE:
		return mouseDown(value.mouseKeyCode);
	default:
		return false;
	}
}

bool KeyMappingInput::isReleased() const
{
	switch (source) {
	case KeyMappingInputSource::KEY_CODE:
		return keyReleased(value.keyCode);
	case KeyMappingInputSource::MOUSE_KEY_CODE:
		return mouseReleased(value.mouseKeyCode);
	default:
		return false;
	}
}

bool KeyMappingInput::isCleared() const
{
	return source == KeyMappingInputSource::KEY_CODE && value.keyCode == KEY_CODE::KEY_MAXSCAN;
}

bool KeyMappingInput::is(const KEY_CODE keyCode) const
{
	return source == KeyMappingInputSource::KEY_CODE && value.keyCode == keyCode;
}

bool KeyMappingInput::is(const MOUSE_KEY_CODE mouseKeyCode) const
{
	return source == KeyMappingInputSource::MOUSE_KEY_CODE && value.mouseKeyCode == mouseKeyCode;
}

nonstd::optional<KEY_CODE> KeyMappingInput::asKeyCode() const
{
	if (source == KeyMappingInputSource::KEY_CODE)
	{
		return value.keyCode;
	}
	else
	{
		return nonstd::nullopt;
	}
}

nonstd::optional<MOUSE_KEY_CODE> KeyMappingInput::asMouseKeyCode() const
{
	if (source == KeyMappingInputSource::MOUSE_KEY_CODE)
	{
		return value.mouseKeyCode;
	}
	else
	{
		return nonstd::nullopt;
	}
}

KeyMappingInput::KeyMappingInput()
	: source(KeyMappingInputSource::KEY_CODE)
	, value(KEY_CODE::KEY_IGNORE)
{}

KeyMappingInput::KeyMappingInput(const KEY_CODE KeyCode)
	: source(KeyMappingInputSource::KEY_CODE)
	, value(KeyMappingInputValue(KeyCode))
{
}

KeyMappingInput::KeyMappingInput(const MOUSE_KEY_CODE mouseKeyCode)
	: source(KeyMappingInputSource::MOUSE_KEY_CODE)
	, value(KeyMappingInputValue(mouseKeyCode))
{
}

KeyMappingInputValue::KeyMappingInputValue(const KEY_CODE keyCode)
	: keyCode(keyCode)
{
}

KeyMappingInputValue::KeyMappingInputValue(const MOUSE_KEY_CODE mouseKeyCode)
	: mouseKeyCode(mouseKeyCode)
{
}

// Key mapping inputs are unions with type enum attached, so we overload the equality
// comparison for convenience.
bool operator==(const KeyMappingInput& lhs, const KeyMappingInput& rhs) {
	if (lhs.source != rhs.source) {
		return false;
	}

	switch (lhs.source) {
	case KeyMappingInputSource::KEY_CODE:
		return lhs.value.keyCode == rhs.value.keyCode;
	case KeyMappingInputSource::MOUSE_KEY_CODE:
		return lhs.value.mouseKeyCode == rhs.value.mouseKeyCode;
	default:
		return false;
	}
}

bool operator!=(const KeyMappingInput& lhs, const KeyMappingInput& rhs) {
	return !(lhs == rhs);
}

// ----------------------------------------------------------------------------------
static bool isCombination(const KeyMapping& mapping)
{
	return mapping.metaKeyCode != KEY_CODE::KEY_IGNORE;
}

static bool isActiveSingleKey(const KeyMapping& mapping)
{
	switch (mapping.action)
	{
	case KeyAction::PRESSED:
		return mapping.input.isPressed();
	case KeyAction::DOWN:
		return mapping.input.isDown();
	case KeyAction::RELEASED:
		return mapping.input.isReleased();
	default:
		debug(LOG_WARNING, "Unknown key action (action code %u) while processing keymap.", (unsigned int)mapping.action);
		return false;
	}
}

static KEY_CODE getAlternativeForMetaKey(const KEY_CODE meta)
{
	auto altMeta = KEY_CODE::KEY_IGNORE;
	if (meta == KEY_CODE::KEY_LCTRL)
	{
		altMeta = KEY_CODE::KEY_RCTRL;
	}
	else if (meta == KEY_CODE::KEY_LALT)
	{
		altMeta = KEY_CODE::KEY_RALT;
	}
	else if (meta == KEY_CODE::KEY_LSHIFT)
	{
		altMeta = KEY_CODE::KEY_RSHIFT;
	}
	else if (meta == KEY_CODE::KEY_LMETA)
	{
		altMeta = KEY_CODE::KEY_RMETA;
	}

	return altMeta;
}

static bool isActiveCombination(const KeyMapping& mapping)
{
	ASSERT(mapping.hasMeta(), "isActiveCombination called for non-meta key mapping!");

	const bool bSubKeyIsPressed = mapping.input.isPressed();
	const bool bMetaIsDown = keyDown(mapping.metaKeyCode);

	const auto altMeta = getAlternativeForMetaKey(mapping.metaKeyCode);
	const bool bHasAlt = altMeta != KEY_IGNORE;
	const bool bAltMetaIsDown = bHasAlt && keyDown(altMeta);

	return bSubKeyIsPressed && (bMetaIsDown || bAltMetaIsDown);
}

bool KeyMapping::isActivated() const
{
	return isCombination(*this)
		? isActiveCombination(*this)
		: isActiveSingleKey(*this);
}

bool KeyMapping::hasMeta() const
{
	return metaKeyCode != KEY_CODE::KEY_IGNORE;
}

bool KeyMapping::toString(char* pOutStr) const
{
	// Figure out if the keycode is for mouse or keyboard and print the name of
	// the respective key/mouse button to `asciiSub`
	char asciiSub[20] = "\0";
	switch (input.source)
	{
	case KeyMappingInputSource::KEY_CODE:
		keyScanToString(input.value.keyCode, (char*)&asciiSub, 20);
		break;
	case KeyMappingInputSource::MOUSE_KEY_CODE:
		mouseKeyCodeToString(input.value.mouseKeyCode, (char*)&asciiSub, 20);
		break;
	default:
		strcpy(asciiSub, "NOT VALID");
		debug(LOG_WZ, "Encountered invalid key mapping source %u while converting mapping to string!", static_cast<unsigned int>(input.source));
		return true;
	}

	if (hasMeta())
	{
		char asciiMeta[20] = "\0";
		keyScanToString(metaKeyCode, (char*)&asciiMeta, 20);

		sprintf(pOutStr, "%s %s", asciiMeta, asciiSub);
	}
	else
	{
		sprintf(pOutStr, "%s", asciiSub);
	}
	return true;
}

// ----------------------------------------------------------------------------------
/* Some stuff allowing the user to add key mappings themselves */

#define	NUM_QWERTY_KEYS	26
struct KEYMAP_MARKER
{
	KeyMapping	*psMapping;
	UDWORD	xPos, yPos;
	SDWORD	spin;
};
static	KEYMAP_MARKER	qwertyKeyMappings[NUM_QWERTY_KEYS];


static bool bDoingDebugMappings = false;
static bool bWantDebugMappings[MAX_PLAYERS] = {false};
// ----------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------
/* Last meta and sub key that were recorded */
static KEY_CODE	lastMetaKey;
static KeyMappingInput lastInput;

// ----------------------------------------------------------------------------------
KeyFunctionInfo::KeyFunctionInfo(
	const InputContext&  context,
	const KeyMappingType type,
	void         (*const function)(),
	const std::string    name,
	const std::string    displayName
)
	: context(context)
	, type(type)
	, function(function)
	, name(name)
	, displayName(displayName)
{
}

class KeyFunctionInfoTable {
public:
	KeyFunctionInfoTable(std::vector<KeyFunctionInfo>& items)
		: ordered_list(std::move(items))
	{
		for (size_t i = 0; i < ordered_list.size(); ++i)
		{
			functionpt_to_index_map[ordered_list[i].function] = i;
			name_to_index_map[ordered_list[i].name] = i;
		}
	}
public:
	KeyFunctionInfo const *keyFunctionInfoByFunction(void (*function)()) const
	{
		auto it = functionpt_to_index_map.find(function);
		if (it != functionpt_to_index_map.end()) {
			return &ordered_list[it->second];
		}
		return nullptr;
	}

	KeyFunctionInfo const *keyFunctionInfoByName(std::string const &name) const
	{
		auto it = name_to_index_map.find(name);
		if (it != name_to_index_map.end()) {
			return &ordered_list[it->second];
		}
		return nullptr;
	}

	const std::vector<std::reference_wrapper<const KeyFunctionInfo>> allKeymapEntries() const
	{
		std::vector<std::reference_wrapper<const KeyFunctionInfo>> entries;
		for (auto const &keyFunctionInfo : ordered_list)
		{
			entries.push_back(keyFunctionInfo);
		}
		return entries;
	}
private:
	std::vector<KeyFunctionInfo> ordered_list;
	std::unordered_map<void (*)(), size_t> functionpt_to_index_map;
	std::unordered_map<std::string, size_t> name_to_index_map;
};

// Definitions/Configuration for all mappable Key Functions
//
// NOTE: The initialization is done as a function with bunch of emplace_backs instead of an initializer list for two reasons:
//        1.) KeyFunctionInfo is marked as non-copy to avoid unnecessarily copying them around. Using an initializer list requires
//            types to be copyable, so we cannot use initializer lists, at all (we use move-semantics with std::move instead)
//        2.) The initializer list itself would require >20kb of stack memory due to sheer size of this thing. Inserting all
//            entries one-by-one requires only one entry on the stack at a time, mitigating the risk of a stack overflow.
static KeyFunctionInfoTable initializeKeyFunctionInfoTable()
{
	std::vector<KeyFunctionInfo> entries;
	entries.emplace_back(KeyFunctionInfo(InputContext::ALWAYS_ACTIVE,   KeyMappingType::FIXED,       kf_ChooseManufacture,               "ChooseManufacture",            N_("Manufacture")));
	entries.emplace_back(KeyFunctionInfo(InputContext::ALWAYS_ACTIVE,   KeyMappingType::FIXED,       kf_ChooseResearch,                  "ChooseResearch",               N_("Research")));
	entries.emplace_back(KeyFunctionInfo(InputContext::ALWAYS_ACTIVE,   KeyMappingType::FIXED,       kf_ChooseBuild,                     "ChooseBuild",                  N_("Build")));
	entries.emplace_back(KeyFunctionInfo(InputContext::ALWAYS_ACTIVE,   KeyMappingType::FIXED,       kf_ChooseDesign,                    "ChooseDesign",                 N_("Design")));
	entries.emplace_back(KeyFunctionInfo(InputContext::ALWAYS_ACTIVE,   KeyMappingType::FIXED,       kf_ChooseIntelligence,              "ChooseIntelligence",           N_("Intelligence Display")));
	entries.emplace_back(KeyFunctionInfo(InputContext::ALWAYS_ACTIVE,   KeyMappingType::FIXED,       kf_ChooseCommand,                   "ChooseCommand",                N_("Commanders")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_QuickSave,                       "QuickSave",                    N_("QuickSave")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ToggleRadar,                     "ToggleRadar",                  N_("Toggle Radar")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_QuickLoad,                       "QuickLoad",                    N_("QuickLoad")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ToggleConsole,                   "ToggleConsole",                N_("Toggle Console Display")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ToggleEnergyBars,                "ToggleEnergyBars",             N_("Toggle Damage Bars On/Off")));
	entries.emplace_back(KeyFunctionInfo(InputContext::BACKGROUND,      KeyMappingType::FIXED,       kf_ScreenDump,                      "ScreenDump",                   N_("Take Screen Shot")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ToggleFormationSpeedLimiting,    "ToggleFormationSpeedLimiting", N_("Toggle Formation Speed Limiting")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_MoveToLastMessagePos,            "MoveToLastMessagePos",         N_("View Location of Previous Message")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ToggleSensorDisplay,             "ToggleSensorDisplay",          N_("Toggle Sensor display")));
	// ASSIGN GROUPS
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AssignGrouping_0,                "AssignGrouping_0",             N_("Assign Group 0")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AssignGrouping_1,                "AssignGrouping_1",             N_("Assign Group 1")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AssignGrouping_2,                "AssignGrouping_2",             N_("Assign Group 2")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AssignGrouping_3,                "AssignGrouping_3",             N_("Assign Group 3")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AssignGrouping_4,                "AssignGrouping_4",             N_("Assign Group 4")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AssignGrouping_5,                "AssignGrouping_5",             N_("Assign Group 5")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AssignGrouping_6,                "AssignGrouping_6",             N_("Assign Group 6")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AssignGrouping_7,                "AssignGrouping_7",             N_("Assign Group 7")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AssignGrouping_8,                "AssignGrouping_8",             N_("Assign Group 8")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AssignGrouping_9,                "AssignGrouping_9",             N_("Assign Group 9")));
	// ADD TO GROUP
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AddGrouping_0,                   "AddGrouping_0",                N_("Add to Group 0")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AddGrouping_1,                   "AddGrouping_1",                N_("Add to Group 1")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AddGrouping_2,                   "AddGrouping_2",                N_("Add to Group 2")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AddGrouping_3,                   "AddGrouping_3",                N_("Add to Group 3")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AddGrouping_4,                   "AddGrouping_4",                N_("Add to Group 4")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AddGrouping_5,                   "AddGrouping_5",                N_("Add to Group 5")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AddGrouping_6,                   "AddGrouping_6",                N_("Add to Group 6")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AddGrouping_7,                   "AddGrouping_7",                N_("Add to Group 7")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AddGrouping_8,                   "AddGrouping_8",                N_("Add to Group 8")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AddGrouping_9,                   "AddGrouping_9",                N_("Add to Group 9")));
	// SELECT GROUPS - Will jump to the group as well as select if group is ALREADY selected
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectGrouping_0,                "SelectGrouping_0",             N_("Select Group 0")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectGrouping_1,                "SelectGrouping_1",             N_("Select Group 1")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectGrouping_2,                "SelectGrouping_2",             N_("Select Group 2")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectGrouping_3,                "SelectGrouping_3",             N_("Select Group 3")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectGrouping_4,                "SelectGrouping_4",             N_("Select Group 4")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectGrouping_5,                "SelectGrouping_5",             N_("Select Group 5")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectGrouping_6,                "SelectGrouping_6",             N_("Select Group 6")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectGrouping_7,                "SelectGrouping_7",             N_("Select Group 7")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectGrouping_8,                "SelectGrouping_8",             N_("Select Group 8")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectGrouping_9,                "SelectGrouping_9",             N_("Select Group 9")));
	// SELECT COMMANDER - Will jump to the group as well as select if group is ALREADY selected
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectCommander_0,               "SelectCommander_0",            N_("Select Commander 0")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectCommander_1,               "SelectCommander_1",            N_("Select Commander 1")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectCommander_2,               "SelectCommander_2",            N_("Select Commander 2")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectCommander_3,               "SelectCommander_3",            N_("Select Commander 3")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectCommander_4,               "SelectCommander_4",            N_("Select Commander 4")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectCommander_5,               "SelectCommander_5",            N_("Select Commander 5")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectCommander_6,               "SelectCommander_6",            N_("Select Commander 6")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectCommander_7,               "SelectCommander_7",            N_("Select Commander 7")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectCommander_8,               "SelectCommander_8",            N_("Select Commander 8")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectCommander_9,               "SelectCommander_9",            N_("Select Commander 9")));
	// MULTIPLAYER
	entries.emplace_back(KeyFunctionInfo(InputContext::BACKGROUND,      KeyMappingType::ASSIGNABLE,  kf_addMultiMenu,                    "addMultiMenu",                 N_("Multiplayer Options / Alliance dialog")));
	// GAME CONTROLS - Moving around, zooming in, rotating etc
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_CameraUp,                        "CameraUp",                     N_("Move Camera Up")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_CameraDown,                      "CameraDown",                   N_("Move Camera Down")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_CameraRight,                     "CameraRight",                  N_("Move Camera Right")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_CameraLeft,                      "CameraLeft",                   N_("Move Camera Left")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SeekNorth,                       "SeekNorth",                    N_("Snap View to North")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ToggleCamera,                    "ToggleCamera",                 N_("Toggle Tracking Camera")));
	entries.emplace_back(KeyFunctionInfo(InputContext::BACKGROUND,      KeyMappingType::FIXED,       kf_addInGameOptions,                "addInGameOptions",             N_("Display In-Game Options")));
	entries.emplace_back(KeyFunctionInfo(InputContext::RADAR,           KeyMappingType::ASSIGNABLE,  kf_RadarZoomOut,                    "RadarZoomOut",                 N_("Zoom Radar Out")));
	entries.emplace_back(KeyFunctionInfo(InputContext::RADAR,           KeyMappingType::ASSIGNABLE,  kf_RadarZoomIn,                     "RadarZoomIn",                  N_("Zoom Radar In")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ZoomIn,                          "ZoomIn",                       N_("Zoom In")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ZoomOut,                         "ZoomOut",                      N_("Zoom Out")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_PitchForward,                    "PitchForward",                 N_("Pitch Forward")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_RotateLeft,                      "RotateLeft",                   N_("Rotate Left")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ResetPitch,                      "ResetPitch",                   N_("Reset Pitch")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_RotateRight,                     "RotateRight",                  N_("Rotate Right")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_PitchBack,                       "PitchBack",                    N_("Pitch Back")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_RightOrderMenu,                  "RightOrderMenu",               N_("Orders Menu")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SlowDown,                        "SlowDown",                     N_("Decrease Game Speed")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SpeedUp,                         "SpeedUp",                      N_("Increase Game Speed")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_NormalSpeed,                     "NormalSpeed",                  N_("Reset Game Speed")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_FaceNorth,                       "FaceNorth",                    N_("View North")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_FaceSouth,                       "FaceSouth",                    N_("View South")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_FaceEast,                        "FaceEast",                     N_("View East")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_FaceWest,                        "FaceWest",                     N_("View West")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_JumpToResourceExtractor,         "JumpToResourceExtractor",      N_("View next Oil Derrick")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_JumpToRepairUnits,               "JumpToRepairUnits",            N_("View next Repair Unit")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_JumpToConstructorUnits,          "JumpToConstructorUnits",       N_("View next Truck")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_JumpToSensorUnits,               "JumpToSensorUnits",            N_("View next Sensor Unit")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_JumpToCommandUnits,              "JumpToCommandUnits",           N_("View next Commander")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ToggleOverlays,                  "ToggleOverlays",               N_("Toggle Overlays")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ToggleConsoleDrop,               "ToggleConsoleDrop",            N_("Toggle Console History ")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ToggleTeamChat,                  "ToggleTeamChat",               N_("Toggle Team Chat History")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_RotateBuildingCW,                "RotateBuildingClockwise",      N_("Rotate Building Clockwise")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_RotateBuildingACW,               "RotateBuildingAnticlockwise",  N_("Rotate Building Anticlockwise")));
	// IN GAME MAPPINGS - Single key presses - ALL __DEBUG keymappings will be removed for master
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_CentreOnBase,                    "CentreOnBase",                 N_("Center View on HQ")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidAttackCease,             "SetDroidAttackCease",          N_("Hold Fire")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_JumpToUnassignedUnits,           "JumpToUnassignedUnits",        N_("View Unassigned Units")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidAttackReturn,            "SetDroidAttackReturn",         N_("Return Fire")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidAttackAtWill,            "SetDroidAttackAtWill",         N_("Fire at Will")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidMoveGuard,               "SetDroidMoveGuard",            N_("Guard Position")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidReturnToBase,            "SetDroidReturnToBase",         N_("Return to HQ")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidOrderHold,               "SetDroidOrderHold",            N_("Hold Position")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidRangeOptimum,            "SetDroidRangeOptimum",         N_("Optimum Range")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidRangeShort,              "SetDroidRangeShort",           N_("Short Range")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidMovePursue,              "SetDroidMovePursue",           N_("Pursue")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidMovePatrol,              "SetDroidMovePatrol",           N_("Patrol")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidGoForRepair,             "SetDroidGoForRepair",          N_("Return For Repair")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidOrderStop,               "SetDroidOrderStop",            N_("Stop Droid")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidGoToTransport,           "SetDroidGoToTransport",        N_("Go to Transport")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidRangeLong,               "SetDroidRangeLong",            N_("Long Range")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SendGlobalMessage,               "SendGlobalMessage",            N_("Send Global Text Message")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SendTeamMessage,                 "SendTeamMessage",              N_("Send Team Text Message")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_AddHelpBlip,                     "AddHelpBlip",                  N_("Drop a beacon")));
	//
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ToggleShadows,                   "ToggleShadows",                N_("Toggles shadows")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_toggleTrapCursor,                "toggleTrapCursor",             N_("Trap cursor")));
	entries.emplace_back(KeyFunctionInfo(InputContext::RADAR,           KeyMappingType::ASSIGNABLE,  kf_ToggleRadarTerrain,              "ToggleRadarTerrain",           N_("Toggle radar terrain")));
	entries.emplace_back(KeyFunctionInfo(InputContext::RADAR,           KeyMappingType::ASSIGNABLE,  kf_ToggleRadarAllyEnemy,            "ToggleRadarAllyEnemy",         N_("Toggle ally-enemy radar view")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_ShowMappings,                    "ShowMappings",                 N_("Show all keyboard mappings")));
	// Some extra non QWERTY mappings but functioning in same way
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidRetreatMedium,           "SetDroidRetreatMedium",        N_("Retreat at Medium Damage")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidRetreatHeavy,            "SetDroidRetreatHeavy",         N_("Retreat at Heavy Damage")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidRetreatNever,            "SetDroidRetreatNever",         N_("Do or Die!")));
	// In game mappings - COMBO (CTRL + LETTER) presses
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllCombatUnits,            "SelectAllCombatUnits",         N_("Select all Combat Units")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllCyborgs,                "SelectAllCyborgs",             N_("Select all Cyborgs")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllDamaged,                "SelectAllDamaged",             N_("Select all Heavily Damaged Units")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllHalfTracked,            "SelectAllHalfTracked",         N_("Select all Half-tracks")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllHovers,                 "SelectAllHovers",              N_("Select all Hovers")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SetDroidRecycle,                 "SetDroidRecycle",              N_("Return for Recycling")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllOnScreenUnits,          "SelectAllOnScreenUnits",       N_("Select all Units on Screen")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllTracked,                "SelectAllTracked",             N_("Select all Tracks")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllUnits,                  "SelectAllUnits",               N_("Select EVERY unit")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllVTOLs,                  "SelectAllVTOLs",               N_("Select all VTOLs")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllArmedVTOLs,             "SelectAllArmedVTOLs",          N_("Select all fully-armed VTOLs")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllWheeled,                "SelectAllWheeled",             N_("Select all Wheels")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_FrameRate,                       "FrameRate",                    N_("Show frame rate")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllSameType,               "SelectAllSameType",            N_("Select all units with the same components")));
	// In game mappings - COMBO (SHIFT + LETTER) presses
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllCombatCyborgs,          "SelectAllCombatCyborgs",       N_("Select all Combat Cyborgs")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllEngineers,              "SelectAllEngineers",           N_("Select all Engineers")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllLandCombatUnits,        "SelectAllLandCombatUnits",     N_("Select all Land Combat Units")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllMechanics,              "SelectAllMechanics",           N_("Select all Mechanics")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllTransporters,           "SelectAllTransporters",        N_("Select all Transporters")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllRepairTanks,            "SelectAllRepairTanks",         N_("Select all Repair Tanks")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllSensorUnits,            "SelectAllSensorUnits",         N_("Select all Sensor Units")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectAllTrucks,                 "SelectAllTrucks",              N_("Select all Trucks")));
	// SELECT PLAYERS - DEBUG ONLY
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectNextFactory,               "SelectNextFactory",            N_("Select next Factory")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectNextResearch,              "SelectNextResearch",           N_("Select next Research Facility")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectNextPowerStation,          "SelectNextPowerStation",       N_("Select next Power Generator")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectNextCyborgFactory,         "SelectNextCyborgFactory",      N_("Select next Cyborg Factory")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_SelectNextVTOLFactory,           "SelectNextVtolFactory",        N_("Select next VTOL Factory")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_JumpNextFactory,                 "JumpNextFactory",              N_("Jump to next Factory")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_JumpNextResearch,                "JumpNextResearch",             N_("Jump to next Research Facility")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_JumpNextPowerStation,            "JumpNextPowerStation",         N_("Jump to next Power Generator")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_JumpNextCyborgFactory,           "JumpNextCyborgFactory",        N_("Jump to next Cyborg Factory")));
	entries.emplace_back(KeyFunctionInfo(InputContext::GAMEPLAY,        KeyMappingType::ASSIGNABLE,  kf_JumpNextVTOLFactory,             "JumpNextVtolFactory",          N_("Jump to next VTOL Factory")));
	// Debug options
	entries.emplace_back(KeyFunctionInfo(InputContext::BACKGROUND,      KeyMappingType::HIDDEN,      kf_ToggleDebugMappings,             "ToggleDebugMappings",          N_("Toggle Debug Mappings")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_ToggleShowPath,                  "ToggleShowPath",               N_("Toggle display of droid path")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_ToggleShowGateways,              "ToggleShowGateways",           N_("Toggle display of gateways")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_ToggleVisibility,                "ToggleVisibility",             N_("Toggle visibility")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_RaiseTile,                       "RaiseTile",                    N_("Raise tile height")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_LowerTile,                       "LowerTile",                    N_("Lower tile height")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_ToggleFog,                       "ToggleFog",                    N_("Toggles All fog")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_ToggleWeather,                   "ToggleWeather",                N_("Trigger some weather")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_TriFlip,                         "TriFlip",                      N_("Flip terrain triangle")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_PerformanceSample,               "PerformanceSample",            N_("Make a performance measurement sample")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_AllAvailable,                    "AllAvailable",                 N_("Make all items available")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_KillSelected,                    "KillSelected",                 N_("Kill Selected Unit(s)")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_ToggleGodMode,                   "ToggleGodMode",                N_("Toggle god Mode Status")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_ChooseOptions,                   "ChooseOptions",                N_("Display Options Screen")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_FinishResearch,                  "FinishResearch",               N_("Complete current research")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_RevealMapAtPos,                  "RevealMapAtPos",               N_("Reveal map at mouse position")));
	entries.emplace_back(KeyFunctionInfo(InputContext::__DEBUG,         KeyMappingType::HIDDEN,      kf_TraceObject,                     "TraceObject",                  N_("Trace a game object")));

	return KeyFunctionInfoTable(entries);
}
static const KeyFunctionInfoTable keyFunctionInfoTable = initializeKeyFunctionInfoTable();

const std::vector<std::reference_wrapper<const KeyFunctionInfo>> allKeymapEntries()
{
	return keyFunctionInfoTable.allKeymapEntries();
}

KeyFunctionInfo const *keyFunctionInfoByFunction(void (*function)())
{
	return keyFunctionInfoTable.keyFunctionInfoByFunction(function);
}

KeyFunctionInfo const *keyFunctionInfoByName(std::string const &name)
{
	return keyFunctionInfoTable.keyFunctionInfoByName(name);
}

KeyMappingInputSource keyMappingSourceByName(std::string const& name)
{
	if (name == "default")
	{
		return KeyMappingInputSource::KEY_CODE;
	}
	else if (name == "mouse_key")
	{
		return KeyMappingInputSource::MOUSE_KEY_CODE;
	}
	else
	{
		debug(LOG_WZ, "Encountered invalid key mapping source name '%s', falling back to using 'default'", name.c_str());
		return KeyMappingInputSource::KEY_CODE;
	}
}

KeyMappingSlot keyMappingSlotByName(std::string const& name)
{
	if (name == "primary")
	{
		return KeyMappingSlot::PRIMARY;
	}
	else if (name == "secondary")
	{
		return KeyMappingSlot::SECONDARY;
	}
	else
	{
		debug(LOG_WZ, "Encountered invalid key mapping slot name '%s', falling back to using 'primary'", name.c_str());
		return KeyMappingSlot::PRIMARY;
	}
}

// ----------------------------------------------------------------------------------
/*
	Here is where we assign functions to keys and to combinations of keys.
	these will be read in from a .cfg file customisable by the player from
	an in-game menu
*/
void InputManager::resetMappings(bool bForceDefaults)
{
	keyMappings.clear();
	bMappingsSortOrderDirty = true;
	for (unsigned n = 0; n < MAX_PLAYERS; ++n)
	{
		processDebugMappings(n, false);
	}

	for (unsigned i = 0; i < NUM_QWERTY_KEYS; i++)
	{
		qwertyKeyMappings[i].psMapping = nullptr;
	}

	// load the mappings.
	if (!bForceDefaults)
	{
		if (loadKeyMap(*this))
		{
			debug(LOG_WZ, "Loaded key map successfully");
		}
		else
		{
			bForceDefaults = true;
		}
	}

	/********************************************************************************************/
	/* The default mappings here are ordered. Similarly the KeyMapInfoTable has the same order. */
	/* Please DO NOT REORDER the mappings.                                                      */
	/********************************************************************************************/

	// Use addDefaultMapping to add the default key mapping if either: (a) bForceDefaults is true, or (b) the loaded key mappings are missing an entry
	bool didAdd = false;

	// FUNCTION KEY MAPPINGS - F1 to F12
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F1,           KeyAction::PRESSED,     kf_ChooseManufacture,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F2,           KeyAction::PRESSED,     kf_ChooseResearch,                  bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F3,           KeyAction::PRESSED,     kf_ChooseBuild,                     bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F4,           KeyAction::PRESSED,     kf_ChooseDesign,                    bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F5,           KeyAction::PRESSED,     kf_ChooseIntelligence,              bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F6,           KeyAction::PRESSED,     kf_ChooseCommand,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F7,           KeyAction::PRESSED,     kf_QuickSave,                       bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_F7,           KeyAction::PRESSED,     kf_ToggleRadar,                     bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F8,           KeyAction::PRESSED,     kf_QuickLoad,                       bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_F8,           KeyAction::PRESSED,     kf_ToggleConsole,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F9,           KeyAction::PRESSED,     kf_ToggleEnergyBars,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F10,          KeyAction::PRESSED,     kf_ScreenDump,                      bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F11,          KeyAction::PRESSED,     kf_ToggleFormationSpeedLimiting,    bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F12,          KeyAction::PRESSED,     kf_MoveToLastMessagePos,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_F12,          KeyAction::PRESSED,     kf_ToggleSensorDisplay,             bForceDefaults);

	//	ASSIGN GROUPS - Will create or replace the existing group
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_0,            KeyAction::PRESSED,     kf_AssignGrouping_0,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_1,            KeyAction::PRESSED,     kf_AssignGrouping_1,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_2,            KeyAction::PRESSED,     kf_AssignGrouping_2,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_3,            KeyAction::PRESSED,     kf_AssignGrouping_3,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_4,            KeyAction::PRESSED,     kf_AssignGrouping_4,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_5,            KeyAction::PRESSED,     kf_AssignGrouping_5,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_6,            KeyAction::PRESSED,     kf_AssignGrouping_6,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_7,            KeyAction::PRESSED,     kf_AssignGrouping_7,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_8,            KeyAction::PRESSED,     kf_AssignGrouping_8,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_9,            KeyAction::PRESSED,     kf_AssignGrouping_9,                bForceDefaults);

	//	ADD TO GROUPS - Will add the selected units to the group
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_0,            KeyAction::PRESSED,     kf_AddGrouping_0,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_1,            KeyAction::PRESSED,     kf_AddGrouping_1,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_2,            KeyAction::PRESSED,     kf_AddGrouping_2,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_3,            KeyAction::PRESSED,     kf_AddGrouping_3,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_4,            KeyAction::PRESSED,     kf_AddGrouping_4,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_5,            KeyAction::PRESSED,     kf_AddGrouping_5,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_6,            KeyAction::PRESSED,     kf_AddGrouping_6,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_7,            KeyAction::PRESSED,     kf_AddGrouping_7,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_8,            KeyAction::PRESSED,     kf_AddGrouping_8,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_9,            KeyAction::PRESSED,     kf_AddGrouping_9,                   bForceDefaults);

	//	SELECT GROUPS - Will jump to the group as well as select if group is ALREADY selected
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_0,            KeyAction::PRESSED,     kf_SelectGrouping_0,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_1,            KeyAction::PRESSED,     kf_SelectGrouping_1,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_2,            KeyAction::PRESSED,     kf_SelectGrouping_2,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_3,            KeyAction::PRESSED,     kf_SelectGrouping_3,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_4,            KeyAction::PRESSED,     kf_SelectGrouping_4,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_5,            KeyAction::PRESSED,     kf_SelectGrouping_5,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_6,            KeyAction::PRESSED,     kf_SelectGrouping_6,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_7,            KeyAction::PRESSED,     kf_SelectGrouping_7,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_8,            KeyAction::PRESSED,     kf_SelectGrouping_8,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_9,            KeyAction::PRESSED,     kf_SelectGrouping_9,                bForceDefaults);

	//	SELECT COMMANDER - Will jump to the group as well as select if group is ALREADY selected
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_0,            KeyAction::PRESSED,     kf_SelectCommander_0,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_1,            KeyAction::PRESSED,     kf_SelectCommander_1,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_2,            KeyAction::PRESSED,     kf_SelectCommander_2,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_3,            KeyAction::PRESSED,     kf_SelectCommander_3,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_4,            KeyAction::PRESSED,     kf_SelectCommander_4,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_5,            KeyAction::PRESSED,     kf_SelectCommander_5,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_6,            KeyAction::PRESSED,     kf_SelectCommander_6,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_7,            KeyAction::PRESSED,     kf_SelectCommander_7,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_8,            KeyAction::PRESSED,     kf_SelectCommander_8,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_9,            KeyAction::PRESSED,     kf_SelectCommander_9,               bForceDefaults);

	//	MULTIPLAYER
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_KPENTER,      KeyAction::PRESSED,     kf_addMultiMenu,                    bForceDefaults);

	//	GAME CONTROLS - Moving around, zooming in, rotating etc
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_UPARROW,      KeyAction::DOWN,        kf_CameraUp,                        bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_DOWNARROW,    KeyAction::DOWN,        kf_CameraDown,                      bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_RIGHTARROW,   KeyAction::DOWN,        kf_CameraRight,                     bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_LEFTARROW,    KeyAction::DOWN,        kf_CameraLeft,                      bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_BACKSPACE,    KeyAction::PRESSED,     kf_SeekNorth,                       bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_SPACE,        KeyAction::PRESSED,     kf_ToggleCamera,                    bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_ESC,          KeyAction::PRESSED,     kf_addInGameOptions,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MINUS,        KeyAction::PRESSED,     kf_RadarZoomOut,                    bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   MOUSE_KEY_CODE::MOUSE_WDN,  KeyAction::PRESSED,     kf_RadarZoomOut,                    bForceDefaults, KeyMappingSlot::SECONDARY);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_EQUALS,       KeyAction::PRESSED,     kf_RadarZoomIn,                     bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   MOUSE_KEY_CODE::MOUSE_WUP,  KeyAction::PRESSED,     kf_RadarZoomIn,                     bForceDefaults, KeyMappingSlot::SECONDARY);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_KP_PLUS,      KeyAction::DOWN,        kf_ZoomIn,                          bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   MOUSE_KEY_CODE::MOUSE_WUP,  KeyAction::PRESSED,     kf_ZoomIn,                          bForceDefaults, KeyMappingSlot::SECONDARY);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_KP_MINUS,     KeyAction::DOWN,        kf_ZoomOut,                         bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   MOUSE_KEY_CODE::MOUSE_WDN,  KeyAction::PRESSED,     kf_ZoomOut,                         bForceDefaults, KeyMappingSlot::SECONDARY);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_KP_2,         KeyAction::DOWN,        kf_PitchForward,                    bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_KP_4,         KeyAction::DOWN,        kf_RotateLeft,                      bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_KP_5,         KeyAction::DOWN,        kf_ResetPitch,                      bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_KP_6,         KeyAction::DOWN,        kf_RotateRight,                     bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_KP_8,         KeyAction::DOWN,        kf_PitchBack,                       bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_KP_0,         KeyAction::PRESSED,     kf_RightOrderMenu,                  bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_MINUS,        KeyAction::PRESSED,     kf_SlowDown,                        bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_EQUALS,       KeyAction::PRESSED,     kf_SpeedUp,                         bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_BACKSPACE,    KeyAction::PRESSED,     kf_NormalSpeed,                     bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_UPARROW,      KeyAction::PRESSED,     kf_FaceNorth,                       bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_DOWNARROW,    KeyAction::PRESSED,     kf_FaceSouth,                       bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_LEFTARROW,    KeyAction::PRESSED,     kf_FaceEast,                        bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_RIGHTARROW,   KeyAction::PRESSED,     kf_FaceWest,                        bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_KP_STAR,      KeyAction::PRESSED,     kf_JumpToResourceExtractor,         bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_JumpToRepairUnits,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_JumpToConstructorUnits,          bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_JumpToSensorUnits,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_JumpToCommandUnits,              bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_TAB,          KeyAction::PRESSED,     kf_ToggleOverlays,                  bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_BACKQUOTE,    KeyAction::PRESSED,     kf_ToggleConsoleDrop,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_BACKQUOTE,    KeyAction::PRESSED,     kf_ToggleTeamChat,                  bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_RotateBuildingCW,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_RotateBuildingACW,               bForceDefaults);
	// IN GAME MAPPINGS - Droid orders etc.
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_B,            KeyAction::PRESSED,     kf_CentreOnBase,                    bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_C,            KeyAction::PRESSED,     kf_SetDroidAttackCease,             bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_D,            KeyAction::PRESSED,     kf_JumpToUnassignedUnits,           bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_E,            KeyAction::PRESSED,     kf_SetDroidAttackReturn,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_F,            KeyAction::PRESSED,     kf_SetDroidAttackAtWill,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_G,            KeyAction::PRESSED,     kf_SetDroidMoveGuard,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_H,            KeyAction::PRESSED,     kf_SetDroidReturnToBase,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_H,            KeyAction::PRESSED,     kf_SetDroidOrderHold,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_I,            KeyAction::PRESSED,     kf_SetDroidRangeOptimum,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_O,            KeyAction::PRESSED,     kf_SetDroidRangeShort,              bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_P,            KeyAction::PRESSED,     kf_SetDroidMovePursue,              bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_Q,            KeyAction::PRESSED,     kf_SetDroidMovePatrol,              bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_R,            KeyAction::PRESSED,     kf_SetDroidGoForRepair,             bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_S,            KeyAction::PRESSED,     kf_SetDroidOrderStop,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_T,            KeyAction::PRESSED,     kf_SetDroidGoToTransport,           bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_U,            KeyAction::PRESSED,     kf_SetDroidRangeLong,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_RETURN,       KeyAction::PRESSED,     kf_SendGlobalMessage,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_RETURN,       KeyAction::PRESSED,     kf_SendTeamMessage,                 bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_H,            KeyAction::PRESSED,     kf_AddHelpBlip,                     bForceDefaults);

	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_S,            KeyAction::PRESSED,     kf_ToggleShadows,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_T,            KeyAction::PRESSED,     kf_toggleTrapCursor,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_TAB,          KeyAction::PRESSED,     kf_ToggleRadarTerrain,              bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_TAB,          KeyAction::PRESSED,     kf_ToggleRadarAllyEnemy,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_M,            KeyAction::PRESSED,     kf_ShowMappings,                    bForceDefaults);

	// Some extra non QWERTY mappings but functioning in same way
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_COMMA,        KeyAction::PRESSED,     kf_SetDroidRetreatMedium,           bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_FULLSTOP,     KeyAction::PRESSED,     kf_SetDroidRetreatHeavy,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_FORWARDSLASH, KeyAction::PRESSED,     kf_SetDroidRetreatNever,            bForceDefaults);

	// IN GAME MAPPINGS - Unit/factory selection
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_A,            KeyAction::PRESSED,     kf_SelectAllCombatUnits,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_C,            KeyAction::PRESSED,     kf_SelectAllCyborgs,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_D,            KeyAction::PRESSED,     kf_SelectAllDamaged,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_F,            KeyAction::PRESSED,     kf_SelectAllHalfTracked,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_H,            KeyAction::PRESSED,     kf_SelectAllHovers,                 bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_R,            KeyAction::PRESSED,     kf_SetDroidRecycle,                 bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_S,            KeyAction::PRESSED,     kf_SelectAllOnScreenUnits,          bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_T,            KeyAction::PRESSED,     kf_SelectAllTracked,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_U,            KeyAction::PRESSED,     kf_SelectAllUnits,                  bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_V,            KeyAction::PRESSED,     kf_SelectAllVTOLs,                  bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_V,            KeyAction::PRESSED,     kf_SelectAllArmedVTOLs,             bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_W,            KeyAction::PRESSED,     kf_SelectAllWheeled,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_Y,            KeyAction::PRESSED,     kf_FrameRate,                       bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_Z,            KeyAction::PRESSED,     kf_SelectAllSameType,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_C,            KeyAction::PRESSED,     kf_SelectAllCombatCyborgs,          bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_E,            KeyAction::PRESSED,     kf_SelectAllEngineers,              bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_G,            KeyAction::PRESSED,     kf_SelectAllLandCombatUnits,        bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_M,            KeyAction::PRESSED,     kf_SelectAllMechanics,              bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_P,            KeyAction::PRESSED,     kf_SelectAllTransporters,           bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_R,            KeyAction::PRESSED,     kf_SelectAllRepairTanks,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_S,            KeyAction::PRESSED,     kf_SelectAllSensorUnits,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_T,            KeyAction::PRESSED,     kf_SelectAllTrucks,                 bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_SelectNextFactory,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_SelectNextResearch,              bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_SelectNextPowerStation,          bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_SelectNextCyborgFactory,         bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_SelectNextVTOLFactory,           bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_JumpNextFactory,                 bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_JumpNextResearch,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_JumpNextPowerStation,            bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_JumpNextCyborgFactory,           bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_MAXSCAN,      KeyAction::PRESSED,     kf_JumpNextVTOLFactory,             bForceDefaults);

	// DEBUG MAPPINGS
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_BACKSPACE,    KeyAction::PRESSED,     kf_ToggleDebugMappings,             bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_M,            KeyAction::PRESSED,     kf_ToggleShowPath,                  bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_E,            KeyAction::PRESSED,     kf_ToggleShowGateways,              bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_V,            KeyAction::PRESSED,     kf_ToggleVisibility,                bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_W,            KeyAction::DOWN,        kf_RaiseTile,                       bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_A,            KeyAction::DOWN,        kf_LowerTile,                       bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_J,            KeyAction::PRESSED,     kf_ToggleFog,                       bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_Q,            KeyAction::PRESSED,     kf_ToggleWeather,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_IGNORE,   KEY_CODE::KEY_K,            KeyAction::PRESSED,     kf_TriFlip,                         bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_K,            KeyAction::PRESSED,     kf_PerformanceSample,               bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_A,            KeyAction::PRESSED,     kf_AllAvailable,                    bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LALT,     KEY_CODE::KEY_K,            KeyAction::PRESSED,     kf_KillSelected,                    bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_G,            KeyAction::PRESSED,     kf_ToggleGodMode,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_O,            KeyAction::PRESSED,     kf_ChooseOptions,                   bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_X,            KeyAction::PRESSED,     kf_FinishResearch,                  bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LSHIFT,   KEY_CODE::KEY_W,            KeyAction::PRESSED,     kf_RevealMapAtPos,                  bForceDefaults);
	didAdd |= addDefaultMapping(KEY_CODE::KEY_LCTRL,    KEY_CODE::KEY_L,            KeyAction::PRESSED,     kf_TraceObject,                     bForceDefaults);

	if (didAdd)
	{
		saveKeyMap(*this);
	}
}

// ----------------------------------------------------------------------------------
bool InputManager::removeMapping(KeyMapping *psToRemove)
{
	auto mapping = std::find_if(keyMappings.begin(), keyMappings.end(), [psToRemove](KeyMapping const &mapping) {
		return &mapping == psToRemove;
	});
	if (mapping != keyMappings.end())
	{
		keyMappings.erase(mapping);
		bMappingsSortOrderDirty = true;
		return true;
	}
	return false;
}

bool InputManager::addDefaultMapping(KEY_CODE metaCode, KeyMappingInput input, KeyAction action, const MappableFunction function, bool bForceDefaults, const KeyMappingSlot slot)
{
	const KeyFunctionInfo* info = keyFunctionInfoTable.keyFunctionInfoByFunction(function);
	ASSERT_OR_RETURN(false, info != nullptr, "Could not determine key function info for mapping being added!");

	KeyMapping* psMapping = getMappingFromFunction(function, slot);
	if (!bForceDefaults && psMapping != nullptr)
	{
		// Not forcing defaults, and there is already a mapping entry for this function with this slot
		return false;
	}

	// Otherwise, force / overwrite with the default
	if (!bForceDefaults)
	{
		debug(LOG_INFO, "Adding missing keymap entry: %s", info->displayName.c_str());
	}
	if (psMapping)
	{
		// Remove any existing mapping for this function
		removeMapping(psMapping);
		psMapping = nullptr;
	}
	if (!bForceDefaults)
	{	
		// Clear the keys from any other mappings
		removeConflictingMappings(metaCode, input, info->context);
	}

	// Set default key mapping
	addMapping(metaCode, input, action, function, slot);
	return true;
}

// ----------------------------------------------------------------------------------
/* Allows _new_ mappings to be made at runtime */
static bool checkQwertyKeys(InputManager& inputManager)
{
	KEY_CODE qKey;
	UDWORD tableEntry;
	bool aquired = false;

	/* Are we trying to make a new map marker? */
	if (keyDown(KEY_LALT))
	{
		/* Did we press a key */
		qKey = getQwertyKey();
		if (qKey)
		{
			tableEntry = asciiKeyCodeToTable(qKey);
			/* We're assigning something to the key */
			debug(LOG_NEVER, "Assigning keymapping to tableEntry: %i", tableEntry);
			if (qwertyKeyMappings[tableEntry].psMapping)
			{
				/* Get rid of the old mapping on this key if there was one */
				inputManager.removeMapping(qwertyKeyMappings[tableEntry].psMapping);
			}
			/* Now add the new one for this location */
			qwertyKeyMappings[tableEntry].psMapping =
				inputManager.addMapping(KEY_LSHIFT, KeyMappingInput((KEY_CODE)qKey), KeyAction::PRESSED, kf_JumpToMapMarker, KeyMappingSlot::PRIMARY);
			aquired = true;

			/* Store away the position and view angle */
			qwertyKeyMappings[tableEntry].xPos = playerPos.p.x;
			qwertyKeyMappings[tableEntry].yPos = playerPos.p.z;
			qwertyKeyMappings[tableEntry].spin = playerPos.r.y;
		}
	}
	return aquired;
}

// ----------------------------------------------------------------------------------
/* allows checking if mapping should currently be ignored in processMappings */
static bool isIgnoredMapping(const InputManager& inputManager, const bool bAllowMouseWheelEvents, const KeyMapping& mapping)
{
	if (!inputManager.isContextActive(mapping.info->context))
	{
		return true;
	}

	if (mapping.input.is(KEY_CODE::KEY_MAXSCAN))
	{
		return true;
	}

	if (!bAllowMouseWheelEvents && (mapping.input.is(MOUSE_KEY_CODE::MOUSE_WUP) || mapping.input.is(MOUSE_KEY_CODE::MOUSE_WDN)))
	{
		return true;
	}

	if (mapping.info->function == nullptr)
	{
		return true;
	}

	const bool bIsDebugMapping = mapping.info->context == InputContext::__DEBUG;
	if (bIsDebugMapping && !getDebugMappingStatus()) {
		return true;
	}

	return false;
}

// ----------------------------------------------------------------------------------
/* Manages update of all the active function mappings */
void InputManager::processMappings(const bool bAllowMouseWheelEvents)
{
	/* Bomb out if there are none */
	if (keyMappings.empty())
	{
		return;
	}

	/* Check if player has made new camera markers */
	checkQwertyKeys(*this);

	/* If mappings have been updated or context priorities have changed, sort the mappings by priority and whether or not they have meta keys */
	if (bMappingsSortOrderDirty)
	{
		keyMappings.sort([this](const KeyMapping& a, const KeyMapping& b) {
			// Primary sort by priority
			const unsigned int priorityA = getContextPriority(a.info->context);
			const unsigned int priorityB = getContextPriority(b.info->context);
			if (priorityA != priorityB)
			{
				return priorityA > priorityB;
			}

			// Sort by meta. This causes all mappings with meta to be checked before non-meta mappings,
			// avoiding having to check for meta-conflicts in the processing loop. (e.g. if we should execute
			// a mapping with right arrow key, depending on if another binding on shift+right-arrow is executed
			// or not). In other words, if any mapping with meta is executed, it will consume the respective input,
			// preventing any non-meta mappings with the same input from being executed.
			return a.hasMeta() && !b.hasMeta();
		});
		bMappingsSortOrderDirty = false;
	}

	std::unordered_set<KeyMappingInput, KeyMappingInput::Hash> consumedInputs;

	/* Run through all sorted mappings */
	for (const KeyMapping& keyToProcess : keyMappings)
	{
		/* Skip inappropriate ones when necessary */
		if (isIgnoredMapping(*this, bAllowMouseWheelEvents, keyToProcess))
		{
			continue;
		}

		/* Skip if the input is already consumed. Handles skips for meta-conflicts */
		const auto bIsAlreadyConsumed = consumedInputs.find(keyToProcess.input) != consumedInputs.end();
		if (bIsAlreadyConsumed)
		{
			continue;
		}

		/* Execute the action if mapping was hit */
		if (keyToProcess.isActivated())
		{
			if (keyToProcess.hasMeta())
			{
				lastMetaKey = keyToProcess.metaKeyCode;
			}

			lastInput = keyToProcess.input;
			keyToProcess.info->function();
			consumedInputs.insert(keyToProcess.input);
		}
	}

	/* Script callback - find out what meta key was pressed */
	int pressedMetaKey = KEY_IGNORE;

	/* getLastMetaKey() can't be used here, have to do manually */
	if (keyDown(KEY_LCTRL))
	{
		pressedMetaKey = KEY_LCTRL;
	}
	else if (keyDown(KEY_RCTRL))
	{
		pressedMetaKey = KEY_RCTRL;
	}
	else if (keyDown(KEY_LALT))
	{
		pressedMetaKey = KEY_LALT;
	}
	else if (keyDown(KEY_RALT))
	{
		pressedMetaKey = KEY_RALT;
	}
	else if (keyDown(KEY_LSHIFT))
	{
		pressedMetaKey = KEY_LSHIFT;
	}
	else if (keyDown(KEY_RSHIFT))
	{
		pressedMetaKey = KEY_RSHIFT;
	}
	else if (keyDown(KEY_LMETA))
	{
		pressedMetaKey = KEY_LMETA;
	}
	else if (keyDown(KEY_RMETA))
	{
		pressedMetaKey = KEY_RMETA;
	}

	/* Find out what keys were pressed */
	for (int i = 0; i < KEY_MAXSCAN; i++)
	{
		/* Skip meta keys */
		switch (i)
		{
		case KEY_LCTRL:
		case KEY_RCTRL:
		case KEY_LALT:
		case KEY_RALT:
		case KEY_LSHIFT:
		case KEY_RSHIFT:
		case KEY_LMETA:
		case KEY_RMETA:
			continue;
			break;
		}

		/* Let scripts process this key if it's pressed */
		if (keyPressed((KEY_CODE)i))
		{
			triggerEventKeyPressed(pressedMetaKey, i);
		}
	}
}

// ----------------------------------------------------------------------------------
/* Returns the key code of the last sub key pressed - allows called functions to have a simple stack */
KeyMappingInput getLastInput()
{
	return lastInput;
}

// ----------------------------------------------------------------------------------
/* Returns the key code of the last meta key pressed - allows called functions to have a simple stack */
KEY_CODE getLastMetaKey()
{
	return lastMetaKey;
}


static const KEY_CODE qwertyCodes[26] =
{
	//  +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+
	    KEY_Q,  KEY_W,  KEY_E,  KEY_R,  KEY_T,  KEY_Y,  KEY_U,  KEY_I,  KEY_O,  KEY_P,
	//  +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+
	//    +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+
	      KEY_A,  KEY_S,  KEY_D,  KEY_F,  KEY_G,  KEY_H,  KEY_J,  KEY_K,  KEY_L,
	//    +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+
	//        +---+   +---+   +---+   +---+   +---+   +---+   +---+
	          KEY_Z,  KEY_X,  KEY_C,  KEY_V,  KEY_B,  KEY_N,  KEY_M
	//        +---+   +---+   +---+   +---+   +---+   +---+   +---+
};

/* Returns the key code of the first ascii key that its finds has been PRESSED */
static KEY_CODE getQwertyKey()
{
	for (KEY_CODE code : qwertyCodes)
	{
		if (keyPressed(code))
		{
			return code;  // Top-, middle- or bottom-row key pressed.
		}
	}

	return (KEY_CODE)0;                     // no ascii key pressed
}

// ----------------------------------------------------------------------------------
/*	Returns the number (0 to 26) of a key on the keyboard
	from it's keycode. Q is zero, through to M being 25
*/
UDWORD asciiKeyCodeToTable(KEY_CODE code)
{
	unsigned i;
	for (i = 0; i < ARRAY_SIZE(qwertyCodes); ++i)
	{
		if (code == qwertyCodes[i])
		{
			return i;
		}
	}

	ASSERT(false, "only pass nonzero key codes from getQwertyKey to this function");
	return 0;
}

// ----------------------------------------------------------------------------------
/* Returns the map X position associated with the passed in keycode */
UDWORD	getMarkerX(KEY_CODE code)
{
	UDWORD	entry;
	entry = asciiKeyCodeToTable(code);
	return (qwertyKeyMappings[entry].xPos);
}

// ----------------------------------------------------------------------------------
/* Returns the map Y position associated with the passed in keycode */
UDWORD	getMarkerY(KEY_CODE code)
{
	UDWORD	entry;
	entry = asciiKeyCodeToTable(code);
	return (qwertyKeyMappings[entry].yPos);
}

// ----------------------------------------------------------------------------------
/* Returns the map Y rotation associated with the passed in keycode */
SDWORD	getMarkerSpin(KEY_CODE code)
{
	UDWORD	entry;
	entry = asciiKeyCodeToTable(code);
	return (qwertyKeyMappings[entry].spin);
}


// ----------------------------------------------------------------------------------
/* Defines whether we process debug key mapping stuff */
void processDebugMappings(unsigned player, bool val)
{
	bWantDebugMappings[player] = val;
	bDoingDebugMappings = true;
	for (unsigned n = 0; n < MAX_PLAYERS; ++n)
	{
		bDoingDebugMappings = bDoingDebugMappings && (bWantDebugMappings[n] || !NetPlay.players[n].allocated);
	}
}

// ----------------------------------------------------------------------------------
/* Returns present status of debug mapping processing */
bool getDebugMappingStatus()
{
	return bDoingDebugMappings;
}
bool getWantedDebugMappingStatus(unsigned player)
{
	return bWantDebugMappings[player];
}
std::string getWantedDebugMappingStatuses(bool val)
{
	char ret[MAX_PLAYERS + 1];
	char *p = ret;
	for (unsigned n = 0; n < MAX_PLAYERS; ++n)
	{
		if (NetPlay.players[n].allocated && bWantDebugMappings[n] == val)
		{
			*p++ = '0' + NetPlay.players[n].position;
		}
	}
	std::sort(ret, p);
	*p++ = '\0';
	return ret;
}
