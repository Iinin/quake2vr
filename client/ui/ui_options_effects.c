/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// ui_options_effects.c -- the effects options menu

#include <ctype.h>
#ifdef _WIN32
#include <io.h>
#endif
#include "../client.h"
#include "include/ui_local.h"

/*
=======================================================================

EFFECTS MENU

=======================================================================
*/

static menuframework_s	s_options_effects_menu;
static menuseparator_s	s_options_effects_header;
static menulist_s		s_options_effects_blood_box;
static menulist_s		s_options_effects_plasmaexplosound_box;
static menulist_s		s_options_effects_itembob_box;
static menuslider_s		s_options_effects_decal_slider;
static menuslider_s		s_options_effects_particle_comp_slider;
static menulist_s		s_options_effects_railtrail_box;
static menulist_s		s_options_effects_railcore_color;
static menulist_s		s_options_effects_railspiral_color;
static menulist_s		s_options_effects_footstep_box;
static menuaction_s		s_options_effects_defaults_action;
static menuaction_s		s_options_effects_back_action;


static void BloodFunc( void *unused )
{
	Cvar_SetValue( "cl_blood", s_options_effects_blood_box.curvalue );
}

static void PlasmaExploSoundFunc( void *unused )
{
	Cvar_SetValue( "cl_plasma_explo_sound", s_options_effects_plasmaexplosound_box.curvalue );
}

static void ItemBobFunc( void *unused )
{
	Cvar_SetValue( "cl_item_bobbing", s_options_effects_itembob_box.curvalue );
}

static void ParticleCompFunc( void *unused )
{
	Cvar_SetValue( "cl_particle_scale", (s_options_effects_particle_comp_slider.curvalue-3)*-1+3);
}

static void DecalCallback( void *unused )
{
	Cvar_SetValue( "r_decals", s_options_effects_decal_slider.curvalue * 50);
}

// Psychospaz's changeable rail trail
static void RailTrailFunc( void *unused )
{
	Cvar_SetValue( "cl_railtype", s_options_effects_railtrail_box.curvalue );
}

static void RailColorFunc( void *unused )
{
	Cvar_SetInteger( "cl_railcore_color",s_options_effects_railcore_color.curvalue + 1);
	Cvar_SetInteger( "cl_railspiral_color",s_options_effects_railspiral_color.curvalue + 1);
}

// foostep override option
static void FootStepFunc( void *unused )
{
	Cvar_SetValue( "cl_footstep_override", s_options_effects_footstep_box.curvalue );
}


//=======================================================================

static void EffectsSetMenuItemValues( void )
{
	Cvar_SetValue( "cl_blood", ClampCvar( 0, 4, Cvar_VariableValue("cl_blood") ) );
	s_options_effects_blood_box.curvalue			= Cvar_VariableValue("cl_blood");

	Cvar_SetValue( "cl_plasma_explo_sound", ClampCvar( 0, 1, Cvar_VariableValue("cl_plasma_explo_sound") ) );
	s_options_effects_plasmaexplosound_box.curvalue = Cvar_VariableValue("cl_plasma_explo_sound");

	Cvar_SetValue( "cl_item_bobbing", ClampCvar( 0, 1, Cvar_VariableValue("cl_item_bobbing") ) );
	s_options_effects_itembob_box.curvalue = Cvar_VariableValue("cl_item_bobbing");

	Cvar_SetValue( "r_decals", ClampCvar (0, 1000, Cvar_VariableValue("r_decals")) );
	s_options_effects_decal_slider.curvalue = Cvar_VariableValue("r_decals") / 50;

	Cvar_SetValue( "cl_particle_scale", ClampCvar( 0, 5, Cvar_VariableValue("cl_particle_scale") ) );
	s_options_effects_particle_comp_slider.curvalue	= (Cvar_VariableValue("cl_particle_scale") -3)*-1+3;

	Cvar_SetValue( "cl_railtype", ClampCvar( 0, 4, Cvar_VariableValue("cl_railtype") ) );
	s_options_effects_railtrail_box.curvalue		= Cvar_VariableValue("cl_railtype");

	Cvar_SetValue( "cl_railcore_color", ClampCvar( 1, 7, Cvar_VariableValue("cl_railcore_color") ) );
	s_options_effects_railcore_color.curvalue		= Cvar_VariableValue("cl_railcore_color") - 1;

	Cvar_SetValue( "cl_railspiral_color", ClampCvar( 1, 7, Cvar_VariableValue("cl_railspiral_color") ) );
	s_options_effects_railspiral_color.curvalue		= Cvar_VariableValue("cl_railspiral_color") - 1;


	Cvar_SetValue( "cl_footstep_override", ClampCvar( 0, 1, Cvar_VariableValue("cl_footstep_override") ) );
	s_options_effects_footstep_box.curvalue			= Cvar_VariableValue("cl_footstep_override");
}

static void EffectsResetDefaultsFunc ( void *unused )
{
	Cvar_SetToDefault ("cl_blood");
	Cvar_SetToDefault ("cl_plasma_explo_sound");
	Cvar_SetToDefault ("cl_item_bobbing");
	Cvar_SetToDefault ("r_decals");
	Cvar_SetToDefault ("cl_particle_scale");
	Cvar_SetToDefault ("cl_railtype");
	Cvar_SetToDefault ("cl_railcore_color");
	Cvar_SetToDefault ("cl_railspiral_color");
	Cvar_SetToDefault ("cl_footstep_override");

	EffectsSetMenuItemValues();
}

void Options_Effects_MenuInit ( void )
{
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char *blood_names[] =
	{
		"none",
		"puff",
		"splat",
		"bleed",
		"gore",
		0
	};

	static const char *railtrail_names[] =
	{
		"standard",
		"core + particles",
		"core + spiral",
		"core", //laser
		"devrail",
		0
	};

	static const char *ColorLookup_names[] =
	{
		"red",
		"green",
		"yellow",
		"blue",
		"cyan",
		"magenta",
		"white",
		0
	};

	int32_t y = 3*MENU_LINE_SIZE;

	s_options_effects_menu.x = SCREEN_WIDTH*0.5;
	s_options_effects_menu.y = SCREEN_HEIGHT*0.5 - 80;
	s_options_effects_menu.nitems = 0;

	s_options_effects_header.generic.type			= MTYPE_SEPARATOR;
	s_options_effects_header.generic.name			= "Effects";
	s_options_effects_header.generic.x				= MENU_FONT_SIZE/2 * strlen(s_options_effects_header.generic.name);
	s_options_effects_header.generic.y				= 0;
	
	s_options_effects_blood_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_effects_blood_box.generic.x			= 0;
	s_options_effects_blood_box.generic.y			= y;
	s_options_effects_blood_box.generic.name		= "blood type";
	s_options_effects_blood_box.generic.callback	= BloodFunc;
	s_options_effects_blood_box.itemnames			= blood_names;
	s_options_effects_blood_box.generic.statusbar	= "changes blood effect type";

	s_options_effects_plasmaexplosound_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_effects_plasmaexplosound_box.generic.x			= 0;
	s_options_effects_plasmaexplosound_box.generic.y			= y += MENU_LINE_SIZE;
	s_options_effects_plasmaexplosound_box.generic.name			= "unique plasma explode sound";
	s_options_effects_plasmaexplosound_box.generic.callback		= PlasmaExploSoundFunc;
	s_options_effects_plasmaexplosound_box.itemnames			= yesno_names;
	s_options_effects_plasmaexplosound_box.generic.statusbar	= "gives Phalanx Cannon plasma explosions a unique sound";

	s_options_effects_itembob_box.generic.type					= MTYPE_SPINCONTROL;
	s_options_effects_itembob_box.generic.x						= 0;
	s_options_effects_itembob_box.generic.y						= y += MENU_LINE_SIZE;
	s_options_effects_itembob_box.generic.name					= "item bobbing";
	s_options_effects_itembob_box.generic.callback				= ItemBobFunc;
	s_options_effects_itembob_box.itemnames						= yesno_names;
	s_options_effects_itembob_box.generic.statusbar				= "adds bobbing effect to rotating items";

	s_options_effects_decal_slider.generic.type					= MTYPE_SLIDER;
	s_options_effects_decal_slider.generic.x					= 0;
	s_options_effects_decal_slider.generic.y					= y += 2*MENU_LINE_SIZE;
	s_options_effects_decal_slider.generic.name					= "decal quantity";
	s_options_effects_decal_slider.generic.callback				= DecalCallback;
	s_options_effects_decal_slider.minvalue						= 0;
	s_options_effects_decal_slider.maxvalue						= 20;
	s_options_effects_decal_slider.generic.statusbar			= "how many decals to display at once (max = 1000)";

	s_options_effects_particle_comp_slider.generic.type			= MTYPE_SLIDER;
	s_options_effects_particle_comp_slider.generic.x			= 0;
	s_options_effects_particle_comp_slider.generic.y			= y += MENU_LINE_SIZE;
	s_options_effects_particle_comp_slider.generic.name			= "particle effect complexity";
	s_options_effects_particle_comp_slider.generic.callback		= ParticleCompFunc;
	s_options_effects_particle_comp_slider.minvalue				= 1;
	s_options_effects_particle_comp_slider.maxvalue				= 5;
	s_options_effects_particle_comp_slider.generic.statusbar	= "lower = faster performance";

	// Psychospaz's changeable rail trail
	s_options_effects_railtrail_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_effects_railtrail_box.generic.x				= 0;
	s_options_effects_railtrail_box.generic.y				= y += 2*MENU_LINE_SIZE;
	s_options_effects_railtrail_box.generic.name			= "railtrail type";
	s_options_effects_railtrail_box.generic.callback		= RailTrailFunc;
	s_options_effects_railtrail_box.itemnames				= railtrail_names;
	s_options_effects_railtrail_box.generic.statusbar		= "changes railgun particle effect";

	s_options_effects_railcore_color.generic.type			= MTYPE_SPINCONTROL;
	s_options_effects_railcore_color.generic.x				= 0;
	s_options_effects_railcore_color.generic.y				= y += MENU_LINE_SIZE;
	s_options_effects_railcore_color.generic.name			= "railgun core color";
	s_options_effects_railcore_color.generic.callback		= RailColorFunc;
	s_options_effects_railcore_color.itemnames				= ColorLookup_names;
	s_options_effects_railcore_color.generic.statusbar		= "changes railgun core color";

	s_options_effects_railspiral_color.generic.type			= MTYPE_SPINCONTROL;
	s_options_effects_railspiral_color.generic.x			= 0;
	s_options_effects_railspiral_color.generic.y			= y += MENU_LINE_SIZE;
	s_options_effects_railspiral_color.generic.name			= "railgun spiral color";
	s_options_effects_railspiral_color.generic.callback		= RailColorFunc;
	s_options_effects_railspiral_color.itemnames			= ColorLookup_names;
	s_options_effects_railspiral_color.generic.statusbar	= "changes railgun spiral color";


	// foostep override option
	s_options_effects_footstep_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_effects_footstep_box.generic.x			= 0;
	s_options_effects_footstep_box.generic.y			= y += 2*MENU_LINE_SIZE;
	s_options_effects_footstep_box.generic.name			= "override footstep sounds";
	s_options_effects_footstep_box.generic.callback		= FootStepFunc;
	s_options_effects_footstep_box.itemnames			= yesno_names;
	s_options_effects_footstep_box.generic.statusbar	= "sets footstep sounds with definitions in texsurfs.txt";

	s_options_effects_defaults_action.generic.type		= MTYPE_ACTION;
	s_options_effects_defaults_action.generic.x			= MENU_FONT_SIZE;
	s_options_effects_defaults_action.generic.y			= y += 2*MENU_LINE_SIZE;
	s_options_effects_defaults_action.generic.name		= "reset defaults";
	s_options_effects_defaults_action.generic.callback	= EffectsResetDefaultsFunc;
	s_options_effects_defaults_action.generic.statusbar	= "resets all effects settings to internal defaults";

	s_options_effects_back_action.generic.type			= MTYPE_ACTION;
	s_options_effects_back_action.generic.x				= MENU_FONT_SIZE;
	s_options_effects_back_action.generic.y				= y += 2*MENU_LINE_SIZE;
	s_options_effects_back_action.generic.name			= "back to options";
	s_options_effects_back_action.generic.callback		= UI_BackMenu;

	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_header );
	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_blood_box );
	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_plasmaexplosound_box );
	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_itembob_box );
	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_decal_slider );
	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_particle_comp_slider );
	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_railtrail_box );
	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_railcore_color );
	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_railspiral_color );
	
	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_footstep_box );
	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_defaults_action );
	Menu_AddItem( &s_options_effects_menu, ( void * ) &s_options_effects_back_action );

	EffectsSetMenuItemValues();
}

void Options_Effects_MenuDraw (void)
{
	Menu_DrawBanner( "m_banner_options" );

	Menu_AdjustCursor( &s_options_effects_menu, 1 );
	Menu_Draw( &s_options_effects_menu );
}

const char *Options_Effects_MenuKey( int32_t key )
{
	return Default_MenuKey( &s_options_effects_menu, key );
}

void M_Menu_Options_Effects_f (void)
{
	Options_Effects_MenuInit();
	UI_PushMenu ( Options_Effects_MenuDraw, Options_Effects_MenuKey );
}
