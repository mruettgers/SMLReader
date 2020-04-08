/**
 * DLMS Units as specified in ISO EN 62056-62 and used by SML
 *
 * @package libsml
 * @copyright Copyright (c) 2011, The volkszaehler.org project
 * @license http://www.gnu.org/licenses/gpl.txt GNU Public License
 * @author Steffen Vogel <info@steffenvogel.de>
 */
/*
 * This file is part of volkzaehler.org
 *
 * volkzaehler.org is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * volkzaehler.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with volkszaehler.org. If not, see <http://www.gnu.org/licenses/>.
 */

#include "types.h"

typedef struct {
	unsigned char code;
	const char *unit;
} dlms_unit_t;

/**
 * Static lookup table
 */
dlms_unit_t dlms_units[] = {
// code, unit		// Quantity			Unit name		SI definition (comment)
//=====================================================================================================
{1, "a"},		// time				year			52*7*24*60*60 s
{2, "mo"},		// time				month			31*24*60*60 s
{3, "wk"},		// time				week			7*24*60*60 s
{4, "d"},		// time				day			24*60*60 s
{5, "h"},		// time				hour			60*60 s
{6, "min."},		// time				min			60 s
{7, "s"},		// time (t)			second			s
{8, "°"},		// (phase) angle		degree			rad*180/π
{9, "°C"},		// temperature (T)		degree celsius		K-273.15
{10, "currency"},	// (local) currency
{11, "m"},		// length (l)			metre			m
{12, "m/s"},		// speed (v)			metre per second	m/s
{13, "m³"},		// volume (V)			cubic metre		m³
{14, "m³"},		// corrected volume		cubic metre		m³
{15, "m³/h"},		// volume flux			cubic metre per hour 	m³/(60*60s)
{16, "m³/h"},		// corrected volume flux	cubic metre per hour 	m³/(60*60s)
{17, "m³/d"},		// volume flux						m³/(24*60*60s)
{18, "m³/d"},		// corrected volume flux				m³/(24*60*60s)
{19, "l"},		// volume			litre			10-3 m³
{20, "kg"},		// mass (m)			kilogram
{21, "N"},		// force (F)			newton
{22, "Nm"},		// energy			newtonmeter		J = Nm = Ws
{23, "Pa"},		// pressure (p)			pascal			N/m²
{24, "bar"},		// pressure (p)			bar			10⁵ N/m²
{25, "J"},		// energy			joule			J = Nm = Ws
{26, "J/h"},		// thermal power		joule per hour		J/(60*60s)
{27, "W"},		// active power (P)		watt			W = J/s
{28, "VA"},		// apparent power (S)		volt-ampere
{29, "var"},		// reactive power (Q)		var
{30, "Wh"},		// active energy		watt-hour		W*(60*60s)
{31, "VAh"},		// apparent energy		volt-ampere-hour	VA*(60*60s)
{32, "varh"},		// reactive energy		var-hour		var*(60*60s)
{33, "A"},		// current (I)			ampere			A
{34, "C"},		// electrical charge (Q)	coulomb			C = As
{35, "V"},		// voltage (U)			volt			V
{36, "V/m"},		// electr. field strength (E)	volt per metre
{37, "F"},		// capacitance (C)		farad			C/V = As/V
{38, "Ω"},		// resistance (R)		ohm			Ω = V/A
{39, "Ωm²/m"},		// resistivity (ρ)		Ωm
{40, "Wb"},		// magnetic flux (Φ)		weber			Wb = Vs
{41, "T"},		// magnetic flux density (B)	tesla			Wb/m2
{42, "A/m"},		// magnetic field strength (H)	ampere per metre	A/m
{43, "H"},		// inductance (L)		henry			H = Wb/A
{44, "Hz"},		// frequency (f, ω)		hertz			1/s
{45, "1/(Wh)"},		// R_W							(Active energy meter constant or pulse value)
{46, "1/(varh)"},	// R_B							(reactive energy meter constant or pulse value)
{47, "1/(VAh)"},	// R_S							(apparent energy meter constant or pulse value)
{48, "V²h"},		// volt-squared hour		volt-squaredhours	V²(60*60s)
{49, "A²h"},		// ampere-squared hour		ampere-squaredhours	A²(60*60s)
{50, "kg/s"},		// mass flux			kilogram per second	kg/s
{51, "S, mho"},		// conductance siemens					1/Ω
{52, "K"},		// temperature (T)		kelvin
{53, "1/(V²h)"},	// R_U²h						(Volt-squared hour meter constant or pulse value)
{54, "1/(A²h)"},	// R_I²h						(Ampere-squared hour meter constant or pulse value)
{55, "1/m³"},		// R_V, meter constant or pulse value (volume)
{56, "%"},		// percentage			%
{57, "Ah"},		// ampere-hours			ampere-hour
{60, "Wh/m³"},		// energy per volume					3,6*103 J/m³
{61, "J/m³"},		// calorific value, wobbe
{62, "Mol %"},		// molar fraction of		mole percent		(Basic gas composition unit)
			// gas composition  
{63, "g/m³"},		// mass density, quantity of material			(Gas analysis, accompanying elements)
{64, "Pa s"},		// dynamic viscosity pascal second			(Characteristic of gas stream)
{253, "(reserved)"},	// reserved
{254, "(other)"},	// other unit
{255, "(unitless)"},	// no unit, unitless, count
{0, ""}		// stop condition for iterator
};
	
const char * dlms_get_unit(unsigned char code) {
	dlms_unit_t *it = dlms_units;
	do { // linear search
		if (it->code == code) {
			return it->unit;
		}
	} while ((++it)->code);
	
	return NULL; // not found
}