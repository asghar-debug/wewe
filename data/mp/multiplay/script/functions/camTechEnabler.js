function completeResearchOnTime(time, playnum)
{
	for (const tech in allRes)
	{
		if (allRes[tech] <= time)
		{
			completeResearch(tech, playnum);
		}
	}
}

var allRes = {
	"R-Vehicle-Prop-Wheels": 0,
	"R-Sys-Spade1Mk1": 0,
	"R-Vehicle-Body01": 0,
	"R-Wpn-MG1Mk1": 5,
	"R-Defense-Tower01": 48,
	"R-Wpn-MG-Damage01": 48,
	"R-Sys-Sensor-Turret01": 65,
	"R-Sys-Engineering01": 86,
	"R-Defense-TankTrap01": 108,
	"R-Defense-HardcreteWall": 129,
	"R-Sys-MobileRepairTurret01": 129,
	"R-Wpn-Flamer01Mk1": 129,
	"R-Sys-Sensor-Tower01": 129,
	"R-Wpn-MG-Damage02": 134,
	"R-Vehicle-Engine01": 172,
	"R-Vehicle-Prop-Halftracks": 172,
	"R-Wpn-Flamer-Damage01": 172,
	"R-Wpn-Cannon1Mk1": 176,
	"R-Defense-Pillbox01": 177,
	"R-Defense-Pillbox05": 187,
	"R-Defense-HardcreteGate": 198,
	"R-Struc-Factory-Cyborg": 215,
	"R-Defense-WallUpgrade01": 215,
	"R-Struc-CommandRelay": 215,
	"R-Sys-Sensor-Tower02": 215,
	"R-Wpn-MG2Mk1": 219,
	"R-Defense-Pillbox04": 230,
	"R-Defense-WallTower02": 230,
	"R-Struc-PowerModuleMk1": 258,
	"R-Wpn-Rocket05-MiniPod": 258,
	"R-Wpn-Flamer-Damage02": 258,
	"R-Comp-CommandTurret01": 258,
	"R-Wpn-Cannon-Damage01": 262,
	"R-Struc-Research-Module": 301,
	"R-Wpn-Flamer-ROF01": 315,
	"R-Vehicle-Engine02": 329,
	"R-Wpn-Rocket-Damage01": 329,
	"R-Defense-Tower06": 332,
	"R-Struc-Materials01": 358,
	"R-Struc-Research-Upgrade01": 358,
	"R-Defense-WallUpgrade02": 360,
	"R-Wpn-MG-Damage03": 360,
	"R-Struc-Factory-Module": 384,
	"R-Wpn-Mortar01Lt": 384,
	"R-Wpn-Flamer-Damage03": 385,
	"R-Wpn-Cannon-Damage02": 385,
	"R-Wpn-MG3Mk1": 407,
	"R-Defense-MortarPit": 407,
	"R-Wpn-Rocket-Damage02": 427,
	"R-Vehicle-Body05": 430,
	"R-Defense-WallTower01": 430,
	"R-Cyborg-Metals01": 431,
	"R-Struc-Research-Upgrade02": 451,
	"R-Wpn-Mortar-Damage01": 453,
	"R-Vehicle-Metals01": 453,
	"R-Struc-Factory-Upgrade01": 472,
	"R-Struc-RepairFacility": 472,
	"R-Sys-MobileRepairTurretHvy": 472,
	"R-Vehicle-Body04": 492,
	"R-Wpn-Rocket02-MRL": 509,
	"R-Vehicle-Engine03": 511,
	"R-Vehicle-Prop-Hover": 511,
	"R-Wpn-Cannon2Mk1": 511,
	"R-Defense-WallUpgrade03": 512,
	"R-Cyborg-Metals02": 512,
	"R-Wpn-Cannon-Accuracy01": 512,
	"R-Wpn-Cannon-Damage03": 512,
	"R-Defense-MRL": 528,
	"R-Defense-WallTower03": 540,
	"R-Wpn-Rocket-Accuracy01": 547,
	"R-Wpn-Rocket-Damage03": 547,
	"R-Struc-RprFac-Upgrade01": 550,
	"R-Wpn-Rocket-ROF01": 550,
	"R-Struc-Research-Upgrade03": 567,
	"R-Wpn-MG-Damage04": 569,
	"R-Vehicle-Metals02": 569,
	"R-Wpn-Mortar-Acc01": 585,
	"R-Wpn-Mortar-Damage02": 585,
	"R-Wpn-MG-ROF01": 602,
	"R-Wpn-Flamer-Damage04": 620,
	"R-Cyborg-Metals03": 620,
	"R-Vehicle-Body08": 636,
	"R-Vehicle-Body11": 636,
	"R-Vehicle-Prop-Tracks": 636,
	"R-Wpn-Rocket01-LtAT": 650,
	"R-Wpn-Cannon4AMk1": 667,
	"R-Defense-Pillbox06": 671,
	"R-Defense-WallTower06": 671,
	"R-Wpn-Rocket-ROF02": 686,
	"R-Struc-VTOLFactory": 686,
	"R-Wpn-Cannon-Damage04": 687,
	"R-Struc-Research-Upgrade04": 701,
	"R-Defense-Emplacement-HPVcannon": 711,
	"R-Vehicle-Metals03": 717,
	"R-Cyborg-Armor-Heat01": 718,
	"R-Defense-WallTower-HPVcannon": 730,
	"R-Wpn-MG-ROF02": 732,
	"R-Wpn-Mortar-Acc02": 732,
	"R-Vehicle-Body12": 732,
	"R-Sys-Sensor-Upgrade01": 745,
	"R-Wpn-Rocket-Damage04": 774,
	"R-Wpn-Rocket03-HvAT": 774,
	"R-Wpn-Mortar-Damage03": 775,
	"R-Wpn-Mortar02Hvy": 775,
	"R-Wpn-Mortar3": 775,
	"R-Vehicle-Body02": 776,
	"R-Cyborg-Transport": 776,
	"R-Wpn-Cannon-ROF01": 786,
	"R-Wpn-MG-Damage05": 790,
	"R-Sys-RadarDetector01": 794,
	"R-Wpn-Sunburst": 803,
	"R-Defense-RotMor": 805,
	"R-Defense-HvyMor": 805,
	"R-Wpn-Flame2": 805,
	"R-Struc-Factory-Upgrade04": 808,
	"R-Wpn-AAGun01": 818,
	"R-Sys-Engineering02": 818,
	"R-Wpn-Cannon-Accuracy02": 818,
	"R-Wpn-Mortar-ROF01": 819,
	"R-Wpn-AAGun03": 833,
	"R-Vehicle-Armor-Heat01": 834,
	"R-Vehicle-Prop-VTOL": 835,
	"R-Cyborg-Armor-Heat02": 835,
	"R-Cyborg-Metals04": 838,
	"R-Struc-Power-Upgrade01": 847,
	"R-Struc-Research-Upgrade05": 847,
	"R-Wpn-Flamer-Damage05": 853,
	"R-Defense-HvyFlamer": 854,
	"R-Defense-Sunburst": 860,
	"R-Sys-CBSensor-Turret01": 861,
	"R-Wpn-Cannon-Damage05": 863,
	"R-Vehicle-Engine04": 869,
	"R-SuperTransport": 871,
	"R-Defense-AASite-QuadBof": 875,
	"R-Wpn-Mortar-Acc03": 875,
	"R-Wpn-MG4": 875,
	"R-Wpn-Rocket-Accuracy02": 886,
	"R-Defense-AASite-QuadMg1": 887,
	"R-Vehicle-Metals04": 896,
	"R-Wpn-Cannon5": 898,
	"R-Wpn-Rocket-Damage05": 912,
	"R-Wpn-Rocket-ROF03": 914,
	"R-Defense-RotMG": 914,
	"R-Wpn-Mortar-Incendiary": 914,
	"R-Struc-VTOLPad": 915,
	"R-Defense-WallTower-DoubleAAgun": 926,
	"R-Cyborg-Hvywpn-Mcannon": 926,
	"R-Sys-Sensor-Upgrade02": 926,
	"R-Sys-CBSensor-Tower01": 939,
	"R-Wpn-Flamer-ROF02": 940,
	"R-Defense-MortarPit-Incendiary": 941,
	"R-Struc-RprFac-Upgrade04": 943,
	"R-Wpn-Cannon-ROF02": 943,
	"R-Defense-Wall-VulcanCan": 950,
	"R-Defense-WallUpgrade04": 952,
	"R-Wpn-Mortar-ROF02": 953,
	"R-Wpn-Bomb01": 956,
	"R-Wpn-MG-Damage06": 966,
	"R-Cyborg-Armor-Heat03": 967,
	"R-Defense-Wall-RotMg": 978,
	"R-Wpn-Mortar-Damage04": 984,
	"R-Struc-Power-Upgrade01b": 984,
	"R-Vehicle-Armor-Heat02": 988,
	"R-Vehicle-Body06": 1001,
	"R-Cyborg-Metals05": 1004,
	"R-Wpn-MG-ROF03": 1006,
	"R-Cyborg-Hvywpn-Acannon": 1013,
	"R-Cyborg-Hvywpn-HPV": 1013,
	"R-Wpn-RocketSlow-Accuracy01": 1017,
	"R-Wpn-Rocket06-IDF": 1017,
	"R-Struc-VTOLPad-Upgrade01": 1019,
	"R-Struc-Research-Upgrade06": 1022,
	"R-Wpn-Rocket02-MRLHvy": 1041,
	"R-Wpn-Cannon-Damage06": 1043,
	"R-Wpn-Cannon3Mk1": 1051,
	"R-Vehicle-Engine05": 1061,
	"R-Wpn-Rocket-Damage06": 1065,
	"R-Defense-MRLHvy": 1065,
	"R-Sys-VTOLStrike-Turret01": 1067,
	"R-Defense-WallTower04": 1075,
	"R-Struc-Materials02": 1077,
	"R-Wpn-Mortar-ROF03": 1084,
	"R-Wpn-Flamer-Damage06": 1090,
	"R-Vehicle-Metals05": 1097,
	"R-Wpn-Bomb-Damage01": 1104,
	"R-Wpn-Bomb03": 1104,
	"R-Wpn-Flamer-ROF03": 1105,
	"R-Struc-Power-Upgrade01c": 1106,
	"R-Wpn-Cannon-ROF03": 1108,
	"R-Defense-IDFRocket": 1112,
	"R-Defense-WallUpgrade05": 1116,
	"R-Wpn-Rocket07-Tank-Killer": 1135,
	"R-Struc-VTOLPad-Upgrade02": 1137,
	"R-Wpn-MG-Damage07": 1144,
	"R-Wpn-RocketSlow-Accuracy02": 1159,
	"R-Defense-HvyA-Trocket": 1159,
	"R-Defense-WallTower-HvyA-Trocket": 1159,
	"R-Wpn-Cannon6TwinAslt": 1160,
	"R-Wpn-MG5": 1163,
	"R-Vehicle-Armor-Heat03": 1168,
	"R-Wpn-Mortar-Damage05": 1170,
	"R-Cyborg-Metals06": 1194,
	"R-Sys-VTOLStrike-Tower01": 1208,
	"R-Defense-WallTower-TwinAGun": 1210,
	"R-Wpn-HowitzerMk1": 1211,
	"R-Cyborg-Hvywpn-TK": 1214,
	"R-Struc-Research-Upgrade07": 1218,
	"R-Defense-Cannon6": 1223,
	"R-Wpn-Rocket-Damage07": 1228,
	"R-Sys-VTOLCBS-Turret01": 1230,
	"R-Wpn-Cannon-Damage07": 1230,
	"R-Wpn-AAGun04": 1232,
	"R-Wpn-Mortar-ROF04": 1236,
	"R-Wpn-Bomb02": 1243,
	"R-Wpn-PlasmaCannon": 1254,
	"R-Comp-CommandTurret02": 1263,
	"R-Vehicle-Engine06": 1271,
	"R-Defense-Super-Cannon": 1275,
	"R-Wpn-Bomb04": 1278,
	"R-Wpn-Bomb-Damage02": 1280,
	"R-Struc-VTOLPad-Upgrade03": 1288,
	"R-Defense-Howitzer": 1298,
	"R-Vehicle-Body09": 1301,
	"R-Defense-WallUpgrade06": 1304,
	"R-Vehicle-Metals06": 1315,
	"R-Defense-AASite-QuadRotMg": 1318,
	"R-Wpn-Howitzer-Damage01": 1319,
	"R-Cyborg-Armor-Heat04": 1340,
	"R-Wpn-Howitzer-Accuracy01": 1340,
	"R-Wpn-Mortar-Damage06": 1342,
	"R-Wpn-MG-Damage08": 1344,
	"R-Sys-Sensor-Upgrade03": 1347,
	"R-Wpn-Plasmite-Flamer": 1347,
	"R-Sys-VTOLCBS-Tower01": 1361,
	"R-Struc-Power-Upgrade02": 1379,
	"R-Defense-PlasmaCannon": 1382,
	"R-Defense-PlasmiteFlamer": 1383,
	"R-Wpn-Howitzer-Incendiary": 1390,
	"R-Wpn-Howitzer03-Rot": 1390,
	"R-Wpn-AAGun02": 1397,
	"R-Wpn-Rocket-Damage08": 1400,
	"R-Cyborg-Metals07": 1404,
	"R-Wpn-Cannon-ROF04": 1409,
	"R-Wpn-Cannon-Damage08": 1423,
	"R-Struc-Research-Upgrade08": 1432,
	"R-Defense-WallTower-QuadRotAA": 1446,
	"R-Wpn-Howitzer-Damage02": 1460,
	"R-Struc-VTOLPad-Upgrade04": 1464,
	"R-Wpn-Laser01": 1472,
	"R-Defense-RotHow": 1476,
	"R-Wpn-Bomb-Damage03": 1476,
	"R-Defense-AASite-QuadBof02": 1479,
	"R-Defense-Howitzer-Incendiary": 1479,
	"R-Wpn-RailGun01": 1483,
	"R-Sys-Sensor-WS": 1485,
	"R-Wpn-Howitzer-Accuracy02": 1499,
	"R-Vehicle-Armor-Heat04": 1505,
	"R-Wpn-Missile2A-T": 1508,
	"R-Cyborg-Armor-Heat05": 1511,
	"R-Defense-WallTower-DoubleAAgun02": 1518,
	"R-Defense-GuardTower-Rail1": 1542,
	"R-Vehicle-Metals07": 1548,
	"R-Sys-SpyTurret": 1564,
	"R-Defense-GuardTower-ATMiss": 1567,
	"R-Defense-WallTower-A-Tmiss": 1567,
	"R-Wpn-Flamer-Damage07": 1570,
	"R-Wpn-Rocket-Damage09": 1580,
	"R-Struc-Power-Upgrade03": 1580,
	"R-Sys-Engineering03": 1590,
	"R-Cyborg-Hvywpn-A-T": 1590,
	"R-Defense-PrisLas": 1590,
	"R-Defense-Super-Rocket": 1600,
	"R-Wpn-Bomb06": 1608,
	"R-Wpn-Cannon-ROF05": 1608,
	"R-Wpn-Cannon-Damage09": 1621,
	"R-Sys-SpyTower": 1623,
	"R-Wpn-Howitzer-Damage03": 1624,
	"R-Cyborg-Metals08": 1629,
	"R-Struc-VTOLPad-Upgrade05": 1661,
	"R-Struc-Research-Upgrade09": 1662,
	"R-Vehicle-Body03": 1666,
	"R-Sys-Autorepair-General": 1668,
	"R-Wpn-EMPCannon": 1668,
	"R-Wpn-Bomb05": 1680,
	"R-Wpn-Howitzer-Accuracy03": 1681,
	"R-Wpn-HvyHowitzer": 1693,
	"R-Wpn-Energy-Accuracy01": 1705,
	"R-Cyborg-Armor-Heat06": 1705,
	"R-Vehicle-Armor-Heat05": 1715,
	"R-Wpn-Rail-Damage01": 1715,
	"R-Defense-EMPCannon": 1723,
	"R-Wpn-Missile-LtSAM": 1738,
	"R-Wpn-Missile-ROF01": 1738,
	"R-Sys-Sensor-WSTower": 1747,
	"R-Struc-RprFac-Upgrade06": 1747,
	"R-Vehicle-Engine07": 1754,
	"R-Struc-Power-Upgrade03a": 1768,
	"R-Defense-WallUpgrade07": 1778,
	"R-Defense-HvyHowitzer": 1785,
	"R-Vehicle-Metals08": 1793,
	"R-Wpn-Flamer-Damage08": 1808,
	"R-Wpn-Cannon-ROF06": 1824,
	"R-Struc-Factory-Upgrade07": 1826,
	"R-Defense-SamSite1": 1856,
	"R-Cyborg-Metals09": 1868,
	"R-Struc-VTOLPad-Upgrade06": 1873,
	"R-Wpn-MortarEMP": 1881,
	"R-Wpn-Howitzer-Damage04": 1912,
	"R-Wpn-Howitzer-ROF01": 1912,
	"R-Cyborg-Armor-Heat07": 1917,
	"R-Wpn-Laser02": 1923,
	"R-Wpn-Energy-Damage01": 1923,
	"R-Wpn-Rail-Accuracy01": 1934,
	"R-Sys-Resistance-Circuits": 1935,
	"R-Defense-EMPMortar": 1935,
	"R-Vehicle-Armor-Heat06": 1942,
	"R-Wpn-MdArtMissile": 1956,
	"R-Wpn-Missile-Damage01": 1956,
	"R-Struc-Materials03": 1960,
	"R-Defense-WallUpgrade08": 1990,
	"R-Cyborg-Hvywpn-PulseLsr": 2009,
	"R-Vehicle-Body07": 2012,
	"R-Defense-WallTower-SamSite": 2020,
	"R-Sys-Sensor-UpLink": 2032,
	"R-Defense-WallTower-PulseLas": 2032,
	"R-Defense-PulseLas": 2043,
	"R-Vehicle-Metals09": 2060,
	"R-Wpn-Flamer-Damage09": 2069,
	"R-Defense-MdArtMissile": 2069,
	"R-Struc-Factory-Upgrade09": 2117,
	"R-Wpn-Energy-ROF01": 2141,
	"R-Wpn-Rail-Damage02": 2152,
	"R-Wpn-Rail-ROF01": 2152,
	"R-Cyborg-Armor-Heat08": 2160,
	"R-Wpn-Missile-ROF02": 2175,
	"R-Wpn-Missile-Accuracy01": 2175,
	"R-Vehicle-Armor-Heat07": 2200,
	"R-Defense-WallUpgrade09": 2233,
	"R-Vehicle-Engine08": 2239,
	"R-Wpn-HeavyPlasmaLauncher": 2244,
	"R-Wpn-Howitzer-ROF02": 2348,
	"R-Wpn-Howitzer-Damage05": 2354,
	"R-Wpn-Energy-Damage02": 2360,
	"R-Wpn-Missile-Damage02": 2393,
	"R-Cyborg-Armor-Heat09": 2433,
	"R-Wpn-HvyLaser": 2469,
	"R-Vehicle-Armor-Heat08": 2488,
	"R-Vehicle-Body10": 2497,
	"R-Defense-WallUpgrade10": 2506,
	"R-Wpn-AALaser": 2578,
	"R-Wpn-Energy-ROF02": 2578,
	"R-Wpn-Rail-ROF02": 2589,
	"R-Wpn-RailGun02": 2596,
	"R-Wpn-Missile-Accuracy02": 2611,
	"R-Defense-HeavyLas": 2624,
	"R-Defense-Rail2": 2672,
	"R-Cyborg-Hvywpn-RailGunner": 2680,
	"R-Defense-HeavyPlasmaLauncher": 2681,
	"R-Defense-WallTower-Rail2": 2706,
	"R-Vehicle-Engine09": 2755,
	"R-Vehicle-Armor-Heat09": 2806,
	"R-Defense-AA-Laser": 2807,
	"R-Wpn-Missile-ROF03": 2829,
	"R-Wpn-HvArtMissile": 2830,
	"R-Defense-WallUpgrade11": 2839,
	"R-Wpn-LasSat": 2890,
	"R-Wpn-Howitzer-ROF03": 3003,
	"R-Wpn-Howitzer-Damage06": 3009,
	"R-Wpn-Energy-Damage03": 3014,
	"R-Wpn-Missile-Damage03": 3048,
	"R-Vehicle-Body13": 3191,
	"R-Defense-WallUpgrade12": 3203,
	"R-Wpn-Energy-ROF03": 3233,
	"R-Wpn-Rail-ROF03": 3243,
	"R-Wpn-Rail-Damage03": 3251,
	"R-Wpn-RailGun03": 3251,
	"R-Defense-HvyArtMissile": 3266,
	"R-Defense-Rail3": 3327,
	"R-Defense-WallTower-Rail3": 3360,
	"R-Wpn-Missile-HvSAM": 3484,
	"R-Defense-Super-Missile": 3597,
	"R-Defense-SamSite2": 3703,
	"R-Defense-WallTower-SamHvy": 3757,
	"R-Vehicle-Body14": 3846,
	"R-Wpn-Howitzer-ROF04": 3861,
	"R-Defense-MassDriver": 4009
};
