#include "faction_camp.h" // IWYU pragma: associated

#include <algorithm>
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "activity_handlers.h"
#include "activity_type.h"
#include "avatar.h"
#include "basecamp.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character.h"
#include "clzones.h"
#include "colony.h"
#include "color.h"
#include "coordinate_conversions.h"
#include "coordinates.h"
#include "cursesdef.h"
#include "debug.h"
#include "enums.h"
#include "faction.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "iexamine.h"
#include "input.h"
#include "inventory.h"
#include "item.h"
#include "item_group.h"
#include "item_pocket.h"
#include "item_stack.h"
#include "itype.h"
#include "kill_tracker.h"
#include "line.h"
#include "localized_comparator.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "mapgen_functions.h"
#include "memory_fast.h"
#include "messages.h"
#include "mission.h"
#include "mission_companion.h"
#include "npc.h"
#include "npctalk.h"
#include "omdata.h"
#include "optional.h"
#include "output.h"
#include "overmap.h"
#include "overmap_ui.h"
#include "overmapbuffer.h"
#include "pimpl.h"
#include "player_activity.h"
#include "point.h"
#include "recipe.h"
#include "recipe_groups.h"
#include "requirements.h"
#include "rng.h"
#include "skill.h"
#include "stomach.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"
#include "ui_manager.h"
#include "units.h"
#include "value_ptr.h"
#include "visitable.h"
#include "weather.h"
#include "weighted_list.h"

class character_id;

static const activity_id ACT_MOVE_LOOT( "ACT_MOVE_LOOT" );

static const item_group_id
Item_spawn_data_foraging_faction_camp_autumn( "foraging_faction_camp_autumn" );
static const item_group_id
Item_spawn_data_foraging_faction_camp_spring( "foraging_faction_camp_spring" );
static const item_group_id
Item_spawn_data_foraging_faction_camp_summer( "foraging_faction_camp_summer" );
static const item_group_id
Item_spawn_data_foraging_faction_camp_winter( "foraging_faction_camp_winter" );
static const item_group_id Item_spawn_data_forest( "forest" );
static const item_group_id
Item_spawn_data_gathering_faction_base_camp_firewood( "gathering_faction_base_camp_firewood" );

static const itype_id itype_fungal_seeds( "fungal_seeds" );
static const itype_id itype_log( "log" );
static const itype_id itype_makeshift_sling( "makeshift_sling" );
static const itype_id itype_marloss_seed( "marloss_seed" );

static const mtype_id mon_bear( "mon_bear" );
static const mtype_id mon_beaver( "mon_beaver" );
static const mtype_id mon_black_rat( "mon_black_rat" );
static const mtype_id mon_chicken( "mon_chicken" );
static const mtype_id mon_chipmunk( "mon_chipmunk" );
static const mtype_id mon_cockatrice( "mon_cockatrice" );
static const mtype_id mon_cougar( "mon_cougar" );
static const mtype_id mon_cow( "mon_cow" );
static const mtype_id mon_coyote( "mon_coyote" );
static const mtype_id mon_deer( "mon_deer" );
static const mtype_id mon_duck( "mon_duck" );
static const mtype_id mon_fox_gray( "mon_fox_gray" );
static const mtype_id mon_fox_red( "mon_fox_red" );
static const mtype_id mon_groundhog( "mon_groundhog" );
static const mtype_id mon_grouse( "mon_grouse" );
static const mtype_id mon_hare( "mon_hare" );
static const mtype_id mon_lemming( "mon_lemming" );
static const mtype_id mon_mink( "mon_mink" );
static const mtype_id mon_moose( "mon_moose" );
static const mtype_id mon_muskrat( "mon_muskrat" );
static const mtype_id mon_opossum( "mon_opossum" );
static const mtype_id mon_otter( "mon_otter" );
static const mtype_id mon_pheasant( "mon_pheasant" );
static const mtype_id mon_pig( "mon_pig" );
static const mtype_id mon_rabbit( "mon_rabbit" );
static const mtype_id mon_squirrel( "mon_squirrel" );
static const mtype_id mon_turkey( "mon_turkey" );
static const mtype_id mon_weasel( "mon_weasel" );
static const mtype_id mon_wolf( "mon_wolf" );

static const oter_str_id oter_forest_wet( "forest_wet" );

static const oter_type_str_id oter_type_forest_trail( "forest_trail" );

static const skill_id skill_bashing( "bashing" );
static const skill_id skill_combat( "combat" );
static const skill_id skill_construction( "construction" );
static const skill_id skill_cutting( "cutting" );
static const skill_id skill_dodge( "dodge" );
static const skill_id skill_fabrication( "fabrication" );
static const skill_id skill_gun( "gun" );
static const skill_id skill_melee( "melee" );
static const skill_id skill_menial( "menial" );
static const skill_id skill_recruiting( "recruiting" );
static const skill_id skill_speech( "speech" );
static const skill_id skill_stabbing( "stabbing" );
static const skill_id skill_survival( "survival" );
static const skill_id skill_swimming( "swimming" );
static const skill_id skill_traps( "traps" );
static const skill_id skill_unarmed( "unarmed" );

static const trait_id trait_DEBUG_HS( "DEBUG_HS" );

static const update_mapgen_id update_mapgen_faction_wall_level_E_1( "faction_wall_level_E_1" );
//static const update_mapgen_id update_mapgen_faction_wall_level_N_1(
//    faction_wall_level_n_1_string.c_str() );
static const update_mapgen_id update_mapgen_faction_wall_level_S_1( "faction_wall_level_S_1" );
static const update_mapgen_id update_mapgen_faction_wall_level_W_1( "faction_wall_level_W_1" );

static const zone_type_id zone_type_CAMP_FOOD( "CAMP_FOOD" );
static const zone_type_id zone_type_CAMP_STORAGE( "CAMP_STORAGE" );

//  Moved the constant compound "string" declaration into a jumble here rather than where they belong
//  because placing them together with their context is rejected by 'cata-static-string_id-constants, -warnings-as-errors'
static const std::string faction_wall_level_n_0_string = "faction_wall_level_N_0";
static const std::string faction_wall_level_n_1_string = "faction_wall_level_N_1";
static const std::string faction_hide_site_0_string = "faction_hide_site_0";
static const oter_str_id oter_faction_hide_site_0( faction_hide_site_0_string );
static const update_mapgen_id update_mapgen_faction_wall_level_N_1(
    faction_wall_level_n_1_string.c_str() );

static const std::string camp_om_fortifications_trench_parameter = faction_wall_level_n_0_string;
static const std::string camp_om_fortifications_spiked_trench_parameter =
    faction_wall_level_n_1_string;

//  These strings are matched against recipe group 'building_type'. Definite candidates for JSON definitions of
//  the various UI strings corresponding to these groups.
static const std::string base_recipe_group_string = "BASE";
static const std::string cook_recipe_group_string = "COOK";
static const std::string farm_recipe_group_string = "FARM";
static const std::string smith_recipe_group_string = "SMITH";

struct mass_volume {
    units::mass wgt = 0_gram;
    units::volume vol = 0_ml;
    int count = 0;
};

namespace base_camps
{
static const std::string recover_ally_string = "Recover Ally, ";
static const std::string expansion_string = " Expansion";


recipe_id select_camp_option( const std::map<recipe_id, translation> &pos_options,
                              const std::string &option );
} // namespace base_camps

/**** Forward declaration of utility functions */
/**
 * Counts or destroys and drops the bash items of all terrain that matches @ref t in the map tile
 * @param comp NPC companion
 * @param omt_tgt the targeted OM tile
 * @param t terrain you are looking for
 * @param chance chance of destruction, 0 to 100
 * @param estimate if true, non-destructive count of the furniture
 * @param bring_back force the destruction of the furniture and bring back the drop items
 */
static int om_harvest_ter( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t,
                           int chance = 100,
                           bool estimate = false, bool bring_back = true );
// om_harvest_ter helper function that counts the furniture instances
static int om_harvest_ter_est( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t,
                               int chance = 100 );
static int om_harvest_ter_break( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t,
                                 int chance = 100 );
/// Collects all items in @ref omt_tgt with a @ref chance between 0 - 1.0, returns total
/// mass and volume
/// @ref take, whether you take the item or count it
static mass_volume om_harvest_itm( const npc_ptr &comp, const tripoint_abs_omt &omt_tgt,
                                   int chance = 100,
                                   bool take = true );
static void apply_camp_ownership( const tripoint &camp_pos, int radius );
/*
 * Counts or cuts trees into trunks and trunks into logs
 * @param omt_tgt the targeted OM tile
 * @param chance chance of destruction, 0 to 100
 * @param estimate if true, non-destructive count of trees
 * @force_cut_trunk if true and estimate is false, chop tree trunks into logs
 */
static int om_cutdown_trees( const tripoint_abs_omt &omt_tgt, int chance = 100,
                             bool estimate = false,
                             bool force_cut_trunk = true );
static int om_cutdown_trees_est( const tripoint_abs_omt &omt_tgt, int chance = 100 );
static int om_cutdown_trees_logs( const tripoint_abs_omt &omt_tgt, int chance = 100 );
static int om_cutdown_trees_trunks( const tripoint_abs_omt &omt_tgt, int chance = 100 );

/// Creates an improvised shelter at @ref omt_tgt and dumps the @ref itms into the building
static bool om_set_hide_site( npc &comp, const tripoint_abs_omt &omt_tgt,
                              const std::vector<item *> &itms,
                              const std::vector<item *> &itms_rem = {} );
/**
 * Opens the overmap so that you can select points for missions or constructions.
 * @param omt_pos where your camp is, used for calculating travel distances
 * @param min_range
 * @param range max number of OM tiles the user can select
 * @param possible_om_types requires the user to reselect if the OM picked isn't in the list
 * @param must_see whether the user can select points in the unknown/fog of war
 * @param popup_notice toggles if the user should be shown ranges before being allowed to pick
 * @param source if you are selecting multiple points this is where the OM is centered to start
 * @param bounce
 */
static tripoint_abs_omt om_target_tile(
    const tripoint_abs_omt &omt_pos, int min_range = 1, int range = 1,
    const std::vector<std::string> &possible_om_types = {}, ot_match_type match_type =
        ot_match_type::exact, bool must_see = true,
    bool popup_notice = true,
    const tripoint_abs_omt &source = tripoint_abs_omt( -999, -999, -999 ),
    bool bounce = false );
static void om_range_mark( const tripoint_abs_omt &origin, int range, bool add_notes = true,
                           const std::string &message = "Y;X: MAX RANGE" );
static void om_line_mark(
    const tripoint_abs_omt &origin, const tripoint_abs_omt &dest, bool add_notes = true,
    const std::string &message = "R;X: PATH" );
static std::vector<tripoint_abs_omt> om_companion_path(
    const tripoint_abs_omt &start, int range_start = 90, bool bounce = true );
/**
 * Can be used to calculate total trip time for an NPC mission or just the traveling portion.
 * Doesn't use the pathingalgorithms yet.
 * @param omt_pos start point
 * @param omt_tgt target point
 * @param work how much time the NPC will stay at the target
 * @param trips how many trips back and forth the NPC will make
 */
static time_duration companion_travel_time_calc( const tripoint_abs_omt &pos,
        const tripoint_abs_omt &tgt,
        time_duration work, int trips = 1, int haulage = 0 );
static time_duration companion_travel_time_calc(
    const std::vector<tripoint_abs_omt> &journey, time_duration work, int trips = 1,
    int haulage = 0 );
/// Determines how many round trips a given NPC @ref comp will take to move all of the
/// items @ref itms
static int om_carry_weight_to_trips( const std::vector<item *> &itms,
                                     const npc_ptr &comp = nullptr );
/// Determines how many trips it takes to move @ref mass and @ref volume of items
/// with @ref carry_mass and @ref carry_volume moved per trip
static int om_carry_weight_to_trips( const units::mass &mass, const units::volume &volume,
                                     const units::mass &carry_mass, const units::volume &carry_volume );
/// Formats the variables into a standard looking description to be displayed in a ynquery window
static std::string camp_trip_description( const time_duration &total_time,
        const time_duration &working_time,
        const time_duration &travel_time,
        int distance, int trips, int need_food );

/// Changes the faction food supply by @ref change, 0 returns total food supply, a negative
/// total food supply hurts morale
static int camp_food_supply( int change = 0, bool return_days = false );
/// Same as above but takes a time_duration and consumes from faction food supply for that
/// duration of work
static int camp_food_supply( time_duration work );
/// Returns the total charges of food time_duration @ref work costs
static int time_to_food( time_duration work );
/// Changes the faction respect for you by @ref change, returns respect
static int camp_discipline( int change = 0 );
/// Changes the faction opinion for you by @ref change, returns opinion
static int camp_morale( int change = 0 );
/*
 * check if a companion survives a random encounter
 * @param comp the companion
 * @param situation what the survivor is doing
 * @param favor a number added to the survivor's skills to see if he can avoid the encounter
 * @param threat a number indicating how dangerous the encounter is
 * TODO: Convert to JSON basic on dynamic line type structure
 */
static bool survive_random_encounter( npc &comp, std::string &situation, int favor, int threat );

//  Hard coded blueprint names used to route the code to the salt water pipe code.
static const std::string faction_expansion_salt_water_pipe_swamp_base =
    "faction_expansion_salt_water_pipe_swamp_";
static const std::string faction_expansion_salt_water_pipe_swamp_N =
    faction_expansion_salt_water_pipe_swamp_base + "N";
static const std::string faction_expansion_salt_water_pipe_swamp_NE =
    faction_expansion_salt_water_pipe_swamp_base + "NE";
static const std::string faction_expansion_salt_water_pipe_base =
    "faction_expansion_salt_water_pipe_";
static const std::string faction_expansion_salt_water_pipe_N =
    faction_expansion_salt_water_pipe_base +
    "N";
static const std::string faction_expansion_salt_water_pipe_NE =
    faction_expansion_salt_water_pipe_base +
    "NE";

static std::string mission_ui_activity_of( const mission_id &miss_id )
{
    const std::string dir_abbr = base_camps::all_directions.at(
                                     miss_id.dir.value() ).bracket_abbr.translated();

    switch( miss_id.id ) {
        case Camp_Distribute_Food:
            return _( "Distribute Food" );

        case Camp_Assign_Jobs:
            return _( "Assign Jobs" );

        case Camp_Assign_Workers:
            return _( "Assign Workers" );

        case Camp_Abandon:
            return _( "Abandon Camp" );

        case Camp_Upgrade:
            return dir_abbr + _( " Upgrade Camp " );

        case Camp_Crafting:
            return dir_abbr + _( " Crafting" );

        case Camp_Gather_Materials:
            return dir_abbr + _( " Gather Materials" );

        case Camp_Collect_Firewood:
            return dir_abbr + _( " Collect Firewood" );

        case Camp_Menial:
            return dir_abbr + _( " Menial Labor" );

        case Camp_Survey_Expansion:
            return _( "Expand Base" );

        case Camp_Cut_Logs:
            return dir_abbr + _( " Cut Logs" );

        case Camp_Clearcut:
            return dir_abbr + _( " Clear a forest" );

        case Camp_Setup_Hide_Site:
            return dir_abbr + _( " Setup Hide Site" );

        case Camp_Relay_Hide_Site:
            return dir_abbr + _( " Relay Hide Site" );

        case Camp_Foraging:
            return dir_abbr + _( " Forage for plants" );

        case Camp_Trapping:
            return dir_abbr + _( " Trap Small Game" );

        case Camp_Hunting:
            return dir_abbr + _( " Hunt Large Animals" );

        case Camp_OM_Fortifications:
            if( miss_id.parameters == camp_om_fortifications_trench_parameter ) {
                return dir_abbr + _( " Construct Map Fortifications" );
            } else {
                return dir_abbr + _( " Construct Spiked Trench" );
            }

        case Camp_Recruiting:
            return dir_abbr + _( " Recruit Companions" );

        case Camp_Scouting:
            return dir_abbr + _( " Scout Mission" );

        case Camp_Combat_Patrol:
            return dir_abbr + _( " Combat Patrol" );

        case Camp_Plow:
            return dir_abbr + _( " Plow Fields" );

        case Camp_Plant:
            return dir_abbr + _( " Plant Fields" );

        case Camp_Harvest:
            return dir_abbr + _( " Harvest Fields" );

        case Camp_Chop_Shop:  //  Obsolete removed during 0.E
            return _( " Chop Shop.  Obsolete.  Can only be recalled" );

        //  Actions that won't be used here
        case No_Mission:
        case Scavenging_Patrol_Job:
        case Scavenging_Raid_Job:
        case Menial_Job:
        case Carpentry_Job:
        case Forage_Job:
        case Caravan_Commune_Center_Job:
        case Purchase_East_Field:
        case Upgrade_East_Field:
        case Plant_East_Field:
        case Harvest_East_Field:
        case Camp_Emergency_Recall:
        default:
            return "";

    }
}

static std::map<std::string, comp_list> companion_per_recipe_building_type( comp_list &npc_list )
{
    std::map<std::string, comp_list> result;

    for( const npc_ptr &comp : npc_list ) {
        const mission_id miss_id = comp->get_companion_mission().miss_id;
        const std::string bldg = recipe_group::get_building_of_recipe( miss_id.parameters );

        if( result[bldg].empty() ) {
            comp_list temp;
            result.insert( std::pair<std::string, comp_list>( bldg, temp ) );
        }
        result[bldg].emplace_back( comp );
    }
    return result;
}

static bool update_time_left( std::string &entry, const comp_list &npc_list )
{
    bool avail = false;
    Character &player_character = get_player_character();
    for( const auto &comp : npc_list ) {
        entry += comp->get_name();
        if( comp->companion_mission_time_ret < calendar::turn ) {
            entry +=  _( " [DONE]\n" );
            avail = true;
        } else {
            entry += " [" +
                     to_string( comp->companion_mission_time_ret - calendar::turn ) +
                     _( " left]\n" );
            avail = player_character.has_trait( trait_DEBUG_HS );
        }
    }
    if( avail ) {
        entry += _( "\n\nDo you wish to bring your allies back into your party?" );
    }
    return avail;
}

static bool update_time_fixed( std::string &entry, const comp_list &npc_list,
                               const time_duration &duration )
{
    bool avail = false;
    for( const auto &comp : npc_list ) {
        const time_duration elapsed = calendar::turn - comp->companion_mission_time;
        entry += "\n  " +  comp->get_name() + " [" + to_string( elapsed ) + " / " +
                 to_string( duration ) + "]";
        avail |= elapsed >= duration;
    }
    if( avail ) {
        entry += _( "\n\nDo you wish to bring your allies back into your party?" );
    }
    return avail;
}

static bool update_emergency_recall( std::string &entry, const comp_list &npc_list,
                                     const time_duration &duration )
{
    bool avail = false;
    for( const auto &comp : npc_list ) {
        const time_duration elapsed = calendar::turn - comp->companion_mission_time;
        const mission_id miss_id = comp->get_companion_mission().miss_id;

        entry += "\n  " + comp->get_name() + " [" + to_string( elapsed ) + " / " +
                 to_string( duration ) + "] " + mission_ui_activity_of( miss_id );
        avail |= elapsed >= duration;
    }
    if( avail ) {
        entry += _( "\n\nDo you wish to bring your allies back into your party?" );
    }
    return avail;
}

static bool extract_and_check_orientation_flags( const recipe_id recipe,
        const point &dir,
        bool &mirror_horizontal,
        bool &mirror_vertical,
        int &rotation,
        const std::string base_error_message,
        const std::string actor )
{
    mirror_horizontal = recipe->has_flag( "MAP_MIRROR_HORIZONTAL" );
    mirror_vertical = recipe->has_flag( "MAP_MIRROR_VERTICAL" );
    rotation = 0;
    std::string dir_string;

    const auto check_rotation = [&]( const std::string & flag, int rotation_value ) {
        if( recipe->has_flag( flag ) ) {
            if( rotation != 0 ) {
                debugmsg(
                    "%s, the blueprint specifies multiple concurrent rotations, which is not "
                    "supported",
                    string_format( base_error_message, actor, recipe->get_blueprint().str() ) );
                return false;
            }
            rotation = rotation_value;
        }
        return true;
    };

    if( !check_rotation( "MAP_ROTATE_90", 1 ) ) {
        return false;
    }

    if( !check_rotation( "MAP_ROTATE_180", 2 ) ) {
        return false;
    }

    if( !check_rotation( "MAP_ROTATE_270", 3 ) ) {
        return false;
    }

    if( dir == point_north_west ) {
        dir_string = "NW";
    } else if( dir == point_north ) {
        dir_string = "N";
    } else if( dir == point_north_east ) {
        dir_string = "NE";
    } else if( dir == point_west ) {
        dir_string = "W";
    } else if( dir == point_zero ) {
        dir_string = "";  //  Will result in "hidden" flags that can actually affect the core.
    } else if( dir == point_east ) {
        dir_string = "E";
    } else if( dir == point_south_west ) {
        dir_string = "SW";
    } else if( dir == point_south ) {
        dir_string = "S";
    } else if( dir == point_south_east ) {
        dir_string = "SE";
    }

    if( recipe->has_flag( "MAP_MIRROR_HORIZONTAL_IF_" + dir_string ) ) {
        if( mirror_horizontal ) {
            debugmsg(
                "%s, the blueprint specifies multiple concurrent horizontal mirroring, which is "
                "not supported",
                string_format( base_error_message, actor, recipe->get_blueprint().str() ) );
            return false;
        }
        mirror_horizontal = true;
    }

    if( recipe->has_flag( "MAP_MIRROR_VERTICAL_IF_" + dir_string ) ) {
        if( mirror_vertical ) {
            debugmsg(
                "%s, the blueprint specifies multiple concurrent vertical mirroring, which is not "
                "supported",
                string_format( base_error_message, actor, recipe->get_blueprint().str() ) );
            return false;
        }
        mirror_vertical = true;
    }

    if( !check_rotation( "MAP_ROTATE_90_IF_" + dir_string, 1 ) ) {
        return false;
    }

    if( !check_rotation( "MAP_ROTATE_180_IF_" + dir_string, 2 ) ) {
        return false;
    }

    if( !check_rotation( "MAP_ROTATE_270_IF_" + dir_string, 3 ) ) {
        return false;
    }

    return true;
}

static cata::optional<basecamp *> get_basecamp( npc &p, const std::string &camp_type = "default" )
{
    tripoint_abs_omt omt_pos = p.global_omt_location();
    cata::optional<basecamp *> bcp = overmap_buffer.find_camp( omt_pos.xy() );
    if( bcp ) {
        return bcp;
    }
    get_map().add_camp( omt_pos, "faction_camp" );
    bcp = overmap_buffer.find_camp( omt_pos.xy() );
    if( !bcp ) {
        return cata::nullopt;
    }
    basecamp *temp_camp = *bcp;
    temp_camp->define_camp( omt_pos, camp_type );
    return temp_camp;
}

recipe_id base_camps::select_camp_option( const std::map<recipe_id, translation> &pos_options,
        const std::string &option )
{
    if( pos_options.size() == 1 ) {
        return pos_options.begin()->first;
    }

    std::vector<std::string> pos_names;
    int choice = 0;

    pos_names.reserve( pos_options.size() );
    for( const auto &it : pos_options ) {
        pos_names.push_back( it.second.translated() );
    }

    std::sort( pos_names.begin(), pos_names.end(), localized_compare );

    choice = uilist( option, pos_names );

    if( choice < 0 || static_cast<size_t>( choice ) >= pos_names.size() ) {
        popup( _( "You choose to wait…" ) );
        return recipe_id::NULL_ID();
    }

    std::string selected_name = pos_names[choice];

    std::map<recipe_id, translation>::const_iterator iter = find_if( pos_options.begin(),
    pos_options.end(), [selected_name]( const std::pair<const recipe_id, translation> &node ) {
        return node.second.translated() == selected_name;
    } );
    return iter->first;
}

void talk_function::start_camp( npc &p )
{
    const tripoint_abs_omt omt_pos = p.global_omt_location();
    const oter_id &omt_ref = overmap_buffer.ter( omt_pos );

    const auto &pos_camps = recipe_group::get_recipes_by_id( "all_faction_base_types",
                            omt_ref.id().c_str() );
    if( pos_camps.empty() ) {
        popup( _( "You cannot build a camp here." ) );
        return;
    }
    const recipe_id camp_type = base_camps::select_camp_option( pos_camps,
                                _( "Select a camp type:" ) );
    if( !camp_type ) {
        return;
    }

    for( const auto &om_near : om_building_region( omt_pos, 3 ) ) {
        const oter_id &om_type = oter_id( om_near.first );
        if( is_ot_match( "faction_base", om_type, ot_match_type::contains ) ) {
            popup( _( "You are too close to another camp!" ) );
            return;
        }
    }
    const recipe &making = camp_type.obj();
    if( !run_mapgen_update_func( making.get_blueprint(), omt_pos ) ) {
        popup( _( "%s failed to start the %s basecamp, perhaps there is a vehicle in the way." ),
               p.disp_name(), making.get_blueprint().str() );
        return;
    }
    get_basecamp( p, camp_type.str() );
}

void talk_function::recover_camp( npc &p )
{
    const tripoint_abs_omt omt_pos = p.global_omt_location();
    const std::string &omt_ref = overmap_buffer.ter( omt_pos ).id().c_str();
    if( omt_ref.find( "faction_base_camp" ) == std::string::npos ) {
        popup( _( "There is no faction camp here to recover!" ) );
        return;
    }
    get_basecamp( p );
}

void talk_function::remove_overseer( npc &p )
{
    size_t suffix = p.get_name().find( _( ", Camp Manager" ) );
    if( suffix != std::string::npos ) {
        p.get_name() = p.get_name().substr( 0, suffix );
    }

    add_msg( _( "%s has abandoned the camp." ), p.get_name() );
    p.companion_mission_role_id.clear();
    stop_guard( p );
}

void talk_function::basecamp_mission( npc &p )
{
    const std::string title = _( "Base Missions" );
    const tripoint_abs_omt omt_pos = p.global_omt_location();
    mission_data mission_key;

    cata::optional<basecamp *> temp_camp = get_basecamp( p );
    if( !temp_camp ) {
        return;
    }
    basecamp *bcp = *temp_camp;
    bcp->set_by_radio( get_avatar().dialogue_by_radio );
    if( bcp->get_dumping_spot() == tripoint_abs_ms{} ) {
        map &here = get_map();
        auto &mgr = zone_manager::get_manager();
        if( here.check_vehicle_zones( here.get_abs_sub().z() ) ) {
            mgr.cache_vzones();
        }
        tripoint src_loc;
        const tripoint_abs_ms abspos = p.get_location();
        if( mgr.has_near( zone_type_CAMP_STORAGE, abspos, 60 ) ) {
            const std::unordered_set<tripoint_abs_ms> &src_set =
                mgr.get_near( zone_type_CAMP_STORAGE, abspos );
            const std::vector<tripoint_abs_ms> &src_sorted =
                get_sorted_tiles_by_distance( abspos, src_set );
            // Find the nearest unsorted zone to dump objects at
            if( !src_sorted.empty() ) {
                src_loc = here.getlocal( src_sorted.front() );
            }
        }
        bcp->set_dumping_spot( here.getglobal( src_loc ) );
    }
    bcp->get_available_missions( mission_key );
    if( display_and_choose_opts( mission_key, omt_pos, base_camps::id, title ) ) {
        bcp->handle_mission( { mission_key.cur_key.id.id, false } );
    }
}

void basecamp::add_available_recipes( mission_data &mission_key, mission_kind kind,
                                      const point &dir,
                                      const std::map<recipe_id, translation> &craft_recipes )
{
    const std::string dir_id = base_camps::all_directions.at( dir ).id;
    const std::string dir_abbr = base_camps::all_directions.at( dir ).bracket_abbr.translated();
    for( const auto &recipe_data : craft_recipes ) {
        const mission_id miss_id = {kind, recipe_data.first.str(), dir};
        const std::string &title_e = dir_abbr + recipe_data.second;
        const std::string &entry = craft_description( recipe_data.first );
        const recipe &recp = recipe_data.first.obj();
        bool craftable = recp.deduped_requirements().can_make_with_inventory(
                             _inv, recp.get_component_filter() );
        mission_key.add_start( miss_id, title_e, entry, craftable );
    }
}

void basecamp::get_available_missions_by_dir( mission_data &mission_key, const point &dir )
{
    std::string entry;

    const std::string dir_id = base_camps::all_directions.at( dir ).id;
    const std::string dir_abbr = base_camps::all_directions.at( dir ).bracket_abbr.translated();
    const tripoint_abs_omt omt_trg = omt_pos + dir;

    {
        // return legacy workers. How old is this legacy?...
        mission_id miss_id = { Camp_Upgrade, "", dir };
        comp_list npc_list = get_mission_workers( miss_id ); // Don't match any blueprints

        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, _( base_camps::recover_ally_string )
                                    + dir_abbr + _( base_camps::expansion_string ),
                                    entry, avail );
        }
        // Generate upgrade missions for expansions
        std::vector<basecamp_upgrade> upgrades = available_upgrades( dir );

        std::sort( upgrades.begin(), upgrades.end(), []( const basecamp_upgrade & p,
                   const basecamp_upgrade & q )->bool {return p.name.translated_lt( q.name ); } );

        for( const basecamp_upgrade &upgrade : upgrades ) {
            miss_id.parameters = upgrade.bldg;

            comp_list npc_list = get_mission_workers( miss_id );

            if( npc_list.empty() ) {
                entry = om_upgrade_description( upgrade.bldg );
                mission_key.add_start( miss_id,
                                       mission_ui_activity_of( miss_id ) + upgrade.name, entry,
                                       upgrade.avail );
            } else {
                entry = action_of( miss_id.id );
                bool avail = update_time_left( entry, npc_list );
                mission_key.add_return( miss_id,
                                        _( base_camps::recover_ally_string ) + dir_abbr +
                                        " " + upgrade.name, entry, avail );
            }
        }
    }

    if( has_provides( "gathering", dir ) ) {
        const mission_id miss_id = { Camp_Gather_Materials, "", dir };
        std::string gather_bldg = "null";
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Send a companion to gather materials for the next camp "
                                  "upgrade.\n\n"
                                  "Skill used: survival\n"
                                  "Difficulty: N/A\n"
                                  "Gathering Possibilities:\n"
                                  "%s\n"
                                  "Risk: Very Low\n"
                                  "Time: 3 Hours, Repeated\n"
                                  "Positions: %d/3\n" ), gathering_description( gather_bldg ),
                               npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.size() < 3 );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_fixed( entry, npc_list, 3_hours );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Ally from Gathering" ),
                                    entry, avail );
        }
    }
    if( has_provides( "firewood", dir ) ) {
        const mission_id miss_id = { Camp_Collect_Firewood, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Send a companion to gather light brush and stout branches.\n\n"
                                  "Skill used: survival\n"
                                  "Difficulty: N/A\n"
                                  "Gathering Possibilities:\n"
                                  "> stout branches\n"
                                  "> withered plants\n"
                                  "> splintered wood\n\n"
                                  "Risk: Very Low\n"
                                  "Time: 3 Hours, Repeated\n"
                                  "Positions: %d/3\n" ), npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.size() < 3 );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_fixed( entry, npc_list, 3_hours );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Firewood Gatherers" ),
                                    entry, avail );
        }
    }
    if( has_provides( "sorting", dir ) ) {
        const mission_id miss_id = { Camp_Menial, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Send a companion to do low level chores and sort "
                                  "supplies.\n\n"
                                  "Skill used: fabrication\n"
                                  "Difficulty: N/A\n"
                                  "Effects:\n"
                                  "> Material left in the unsorted loot zone will be sorted "
                                  "into a defined loot zone."
                                  "\n\nRisk: None\n"
                                  "Time: 3 Hours\n"
                                  "Positions: %d/1\n" ), npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Menial Laborer" ),
                                    entry, avail );
        }
    }

    if( has_provides( "logging", dir ) ) {
        const mission_id miss_id = { Camp_Cut_Logs, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Send a companion to a nearby forest to cut logs.\n\n"
                                  "Skill used: fabrication\n"
                                  "Difficulty: 2\n"
                                  "Effects:\n"
                                  "> 50%% of trees/trunks at the forest position will be "
                                  "cut down.\n"
                                  "> 100%% of total material will be brought back.\n"
                                  "> Repeatable with diminishing returns.\n"
                                  "> Will eventually turn forests into fields.\n"
                                  "Risk: None\n"
                                  "Time: 6 Hour Base + Travel Time + Cutting Time\n"
                                  "Positions: %d/1\n" ), npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Log Cutter" ),
                                    entry, avail );
        }
    }

    if( has_provides( "logging", dir ) ) {
        const mission_id miss_id = { Camp_Clearcut, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Send a companion to clear a nearby forest.\n\n"
                                  "Skill used: fabrication\n"
                                  "Difficulty: 1\n"
                                  "Effects:\n"
                                  "> 95%% of trees/trunks at the forest position"
                                  " will be cut down.\n"
                                  "> 0%% of total material will be brought back.\n"
                                  "> Forest should become a field tile.\n"
                                  "> Useful for clearing land for another faction camp.\n\n"
                                  "Risk: None\n"
                                  "Time: 6 Hour Base + Travel Time + Cutting Time\n"
                                  "Positions: %d/1\n" ), npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Clearcutter" ),
                                    entry, avail );
        }
    }

    if( has_provides( "relaying", dir ) ) {
        const mission_id miss_id = { Camp_Setup_Hide_Site, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Send a companion to build an improvised shelter and stock it "
                                  "with equipment at a distant map location.\n\n"
                                  "Skill used: survival\n"
                                  "Difficulty: 3\n"
                                  "Effects:\n"
                                  "> Good for setting up resupply or contingency points.\n"
                                  "> Gear is left unattended and could be stolen.\n"
                                  "> Time dependent on weight of equipment being sent forward.\n\n"
                                  "Risk: Medium\n"
                                  "Time: 6 Hour Construction + Travel\n"
                                  "Positions: %d/1\n" ), npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Hide Setup" ),
                                    entry, avail );
        }
    }

    if( has_provides( "relaying", dir ) ) {
        const mission_id miss_id = { Camp_Relay_Hide_Site, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Push gear out to a hide site or bring gear back from one.\n\n"
                                  "Skill used: survival\n"
                                  "Difficulty: 1\n"
                                  "Effects:\n"
                                  "> Good for returning equipment you left in the hide site "
                                  "shelter.\n"
                                  "> Gear is left unattended and could be stolen.\n"
                                  "> Time dependent on weight of equipment being sent forward or "
                                  "back.\n\n"
                                  "Risk: Medium\n"
                                  "Time: 1 Hour Base + Travel\n"
                                  "Positions: %d/1\n" ), npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ), entry,
                               npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Hide Relay" ),
                                    entry, avail );
        }
    }

    if( has_provides( "foraging", dir ) ) {
        const mission_id miss_id = { Camp_Foraging, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Send a companion to forage for edible plants.\n\n"
                                  "Skill used: survival\n"
                                  "Difficulty: N/A\n"
                                  "Foraging Possibilities:\n"
                                  "> wild vegetables\n"
                                  "> fruits and nuts depending on season\n"
                                  "May produce less food than consumed!\n"
                                  "Risk: Very Low\n"
                                  "Time: 4 Hours, Repeated\n"
                                  "Positions: %d/3\n" ), npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.size() < 3 );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_fixed( entry, npc_list, 4_hours );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Foragers" ),
                                    entry, avail );
        }
    }

    if( has_provides( "trapping", dir ) ) {
        const mission_id miss_id = { Camp_Trapping, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Send a companion to set traps for small game.\n\n"
                                  "Skill used: devices\n"
                                  "Difficulty: N/A\n"
                                  "Trapping Possibilities:\n"
                                  "> small and tiny animal corpses\n"
                                  "May produce less food than consumed!\n"
                                  "Risk: Low\n"
                                  "Time: 6 Hours, Repeated\n"
                                  "Positions: %d/2\n" ), npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.size() < 2 );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_fixed( entry, npc_list, 6_hours );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Trappers" ),
                                    entry, avail );
        }
    }

    if( has_provides( "hunting", dir ) ) {
        const mission_id miss_id = { Camp_Hunting, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Send a companion to hunt large animals.\n\n"
                                  "Skill used: marksmanship\n"
                                  "Difficulty: N/A\n"
                                  "Hunting Possibilities:\n"
                                  "> small, medium, or large animal corpses\n"
                                  "May produce less food than consumed!\n"
                                  "Risk: Medium\n"
                                  "Time: 6 Hours, Repeated\n"
                                  "Positions: %d/1\n" ), npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_fixed( entry, npc_list, 6_hours );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Hunter" ),
                                    entry, avail );
        }
    }

    if( has_provides( "walls", dir ) ) {
        mission_id miss_id = { Camp_OM_Fortifications, camp_om_fortifications_trench_parameter, dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = om_upgrade_description( faction_wall_level_n_0_string );
        //  Should add check for materials as well as active mission.
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Finish Map Fortifications" ),
                                    entry, avail );
        }

        entry = om_upgrade_description( faction_wall_level_n_1_string );
        miss_id.parameters = camp_om_fortifications_spiked_trench_parameter;
        npc_list = get_mission_workers( miss_id );
        //  Should add check for materials as well as active mission.
        //  Should also check if there are any trenches to improve.
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry,
                               npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Finish Map Fortification Update" ),
                                    entry, avail );
        }

        //  Code to deal with legacy construction (Changed during 0.F)
        miss_id.parameters = "";
        npc_list = get_mission_workers( miss_id );

        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Finish Map Fortifications" ),
                                    entry, avail );
        }

    }

    if( has_provides( "recruiting", dir ) ) {
        const mission_id miss_id = { Camp_Recruiting, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = recruit_description( npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ), entry,
                               npc_list.empty() );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Recruiter" ),
                                    entry, avail );
        }
    }

    if( has_provides( "scouting", dir ) ) {
        const mission_id miss_id = { Camp_Scouting, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Send a companion out into the great unknown.  High survival "
                                  "skills are needed to avoid combat but you should expect an "
                                  "encounter or two.\n\n"
                                  "Skill used: survival\n"
                                  "Difficulty: 3\n"
                                  "Effects:\n"
                                  "> Select checkpoints to customize path.\n"
                                  "> Reveals terrain around the path.\n"
                                  "> Can bounce off hide sites to extend range.\n\n"
                                  "Risk: High\n"
                                  "Time: Travel\n"
                                  "Positions: %d/3\n" ), npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.size() < 3 );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Scout" ),
                                    entry, avail );
        }
    }

    if( has_provides( "patrolling", dir ) ) {
        const mission_id miss_id = { Camp_Combat_Patrol, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        entry = string_format( _( "Notes:\n"
                                  "Send a companion to purge the wasteland.  Their goal is to "
                                  "kill anything hostile they encounter and return when "
                                  "their wounds are too great or the odds are stacked against "
                                  "them.\n\n"
                                  "Skill used: survival\n"
                                  "Difficulty: 4\n"
                                  "Effects:\n"
                                  "> Pulls creatures encountered into combat instead of "
                                  "fleeing.\n"
                                  "> Select checkpoints to customize path.\n"
                                  "> Can bounce off hide sites to extend range.\n\n"
                                  "Risk: Very High\n"
                                  "Time: Travel\n"
                                  "Positions: %d/3\n" ), npc_list.size() );
        mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                               entry, npc_list.size() < 3 );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id, dir_abbr + _( " Recover Combat Patrol" ),
                                    entry, avail );
        }
    }

    if( has_provides( "dismantling",
                      dir ) ) {  //  Obsolete (during 0.E), but we still have to be able to process existing missions.
        const mission_id miss_id = { Camp_Chop_Shop, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        if( !npc_list.empty() ) {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id,
                                    dir_abbr + _( " (Finish) Chop Shop" ), entry, avail );
        }
    }
    std::map<recipe_id, translation> craft_recipes = recipe_deck( dir );
    {
        mission_id miss_id = { Camp_Crafting, "", dir };
        comp_list npc_list = get_mission_workers( miss_id, true );
        if( npc_list.size() < 3 ) {
            add_available_recipes( mission_key, Camp_Crafting, dir, craft_recipes );
        }

        if( !npc_list.empty() ) {
            std::map<std::string, comp_list> lists = companion_per_recipe_building_type( npc_list );

            for( std::pair<std::string, comp_list> npcs : lists ) {
                const std::string bldg = npcs.first;
                miss_id.parameters = npcs.second.at( 0 )->get_companion_mission().miss_id.parameters;
                bool avail = false;
                entry.clear();

                //  Room for moving the match of recipe group 'building_type' to return string into JSON
                std::string return_craft;

                if( bldg == base_recipe_group_string ) {
                    return_craft = _( " (Finish) Crafting" );

                } else if( bldg == cook_recipe_group_string ) {
                    return_craft = _( " (Finish) Cooking" );

                } else if( bldg == farm_recipe_group_string ) {
                    return_craft = _( " (Finish) Crafting" );

                } else if( bldg == smith_recipe_group_string ) {
                    return_craft = _( " (Finish) Smithing" );
                }

                else {  //  No matching recipe group
                    return_craft = _( " (Finish) Crafting" );
                }

                for( npc_ptr &comp : npcs.second ) {
                    const bool done = comp->companion_mission_time_ret < calendar::turn;
                    avail |= done;
                    entry += comp->get_name() + " ";
                    if( done ) {
                        entry += _( "[DONE]\n" );
                    } else {
                        entry += " [" +
                                 to_string( comp->companion_mission_time_ret - calendar::turn ) +
                                 _( " left] " ) + action_of( miss_id.id );
                    }
                }

                mission_key.add_return( miss_id,
                                        dir_abbr + return_craft, entry, avail );
            }
        }
    }

    if( has_provides( "farming", dir ) ) {
        size_t plots = 0;
        const mission_id miss_id = { Camp_Plow, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        if( npc_list.empty() ) {
            entry = _( "Notes:\n"
                       "Plow any spaces that have reverted to dirt or grass.\n\n" ) +
                    farm_description( omt_trg, plots, farm_ops::plow ) +
                    _( "\n\n"
                       "Skill used: fabrication\n"
                       "Difficulty: N/A\n"
                       "Effects:\n"
                       "> Restores only the plots created in the last expansion upgrade.\n"
                       "> Does not damage existing crops.\n\n"
                       "Risk: None\n"
                       "Time: 5 Min / Plot\n"
                       "Positions: 0/1\n" );
            mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                                   entry, plots > 0 );
        } else {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id,
                                    dir_abbr + _( " (Finish) Plow fields" ), entry, avail );
        }
    }
    if( has_provides( "farming", dir ) ) {
        size_t plots = 0;
        const mission_id miss_id = { Camp_Plant, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        if( npc_list.empty() ) {
            entry = _( "Notes:\n"
                       "Plant designated seeds in the spaces that have already been "
                       "tilled.\n\n" ) +
                    farm_description( omt_trg, plots, farm_ops::plant ) +
                    _( "\n\n"
                       "Skill used: survival\n"
                       "Difficulty: N/A\n"
                       "Effects:\n"
                       "> Choose which seed type or all of your seeds.\n"
                       "> Stops when out of seeds or planting locations.\n"
                       "> Will plant in ALL dirt mounds in the expansion.\n\n"
                       "Risk: None\n"
                       "Time: 1 Min / Plot\n"
                       "Positions: 0/1\n" );
            mission_key.add_start( miss_id,
                                   mission_ui_activity_of( miss_id ), entry,
                                   plots > 0 && warm_enough_to_plant( omt_trg ) );
        } else {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id,
                                    dir_abbr + _( " (Finish) Plant Fields" ), entry, avail );
        }
    }
    if( has_provides( "farming", dir ) ) {
        size_t plots = 0;
        const mission_id miss_id = { Camp_Harvest, "", dir };
        comp_list npc_list = get_mission_workers( miss_id );
        if( npc_list.empty() ) {
            entry = _( "Notes:\n"
                       "Harvest any plants that are ripe and bring the produce back.\n\n" ) +
                    farm_description( omt_trg, plots, farm_ops::harvest ) +
                    _( "\n\n"
                       "Skill used: survival\n"
                       "Difficulty: N/A\n"
                       "Effects:\n"
                       "> Will dump all harvesting products onto your location.\n\n"
                       "Risk: None\n"
                       "Time: 3 Min / Plot\n"
                       "Positions: 0/1\n" );
            mission_key.add_start( miss_id,
                                   mission_ui_activity_of( miss_id ), entry,
                                   plots > 0 );
        } else {
            entry = action_of( miss_id.id );
            bool avail = update_time_left( entry, npc_list );
            mission_key.add_return( miss_id,
                                    dir_abbr + _( " (Finish) Harvest Fields" ), entry, avail );
        }
    }
}

void basecamp::get_available_missions( mission_data &mission_key )
{
    std::string entry;

    const point &base_dir = base_camps::base_dir;
    const base_camps::direction_data &base_data = base_camps::all_directions.at( base_dir );
    const std::string base_dir_id = base_data.id;
    reset_camp_resources();

    // Missions that belong exclusively to the central tile
    {
        if( can_expand() ) {
            const mission_id miss_id = { Camp_Survey_Expansion, "", base_dir };
            comp_list npc_list = get_mission_workers( miss_id );
            entry = string_format( _( "Notes:\n"
                                      "Your base has become large enough to support an expansion.  "
                                      "Expansions open up new opportunities but can be expensive and "
                                      "time consuming.  Pick them carefully, at most 8 can be built "
                                      "at each camp.\n\n"
                                      "Skill used: N/A\n"
                                      "Effects:\n"
                                      "> Choose any one of the available expansions.  Starting with "
                                      "a farm is always a solid choice since food is used to support "
                                      "companion missions and little is needed to get it going.  "
                                      "With minimal investment, and a forge provides the resources "
                                      "to make charcoal.\n\n"
                                      "NOTE: Actions available through expansions are located in "
                                      "separate tabs of the Camp Manager window.\n\n"
                                      "Risk: None\n"
                                      "Time: 3 Hours\n"
                                      "Positions: %d/1\n" ), npc_list.size() );
            mission_key.add_start( miss_id, mission_ui_activity_of( miss_id ),
                                   entry, npc_list.empty() );
            if( !npc_list.empty() ) {
                entry = action_of( miss_id.id );
                bool avail = update_time_left( entry, npc_list );
                mission_key.add_return( miss_id, _( "Recover Surveyor" ),
                                        entry, avail );
            }
        } else {
            // Unless maximum expansions have been reached, show "Expand Base",
            // but in a disabled state, with a message about what is required.
            if( directions.size() < 8 ) {
                const mission_id miss_id = { Camp_Survey_Expansion, "", base_dir };
                entry = _( "You will need more beds before you can expand your base." );
                mission_key.add_start( miss_id, _( "Expand Base" ),
                                       entry, false );
            }
        }
    }

    if( !by_radio ) {
        {
            const mission_id miss_id = { Camp_Distribute_Food, "", base_dir };
            entry = string_format( _( "Notes:\n"
                                      "Distribute food to your follower and fill you larders.  "
                                      "Place the food you wish to distribute in the camp food zone.  "
                                      "You must have a camp food zone, and a camp storage zone, "
                                      "or you will be prompted to create them using the zone manager.\n"
                                      "Effects:\n"
                                      "> Increases your faction's food supply value which in "
                                      "turn is used to pay laborers for their time\n\n"
                                      "Must have enjoyability >= -6\n"
                                      "Perishable food liquidated at penalty depending on "
                                      "upgrades and rot time:\n"
                                      "> Rotten: 0%%\n"
                                      "> Rots in < 2 days: 60%%\n"
                                      "> Rots in < 5 days: 80%%\n\n"
                                      "Total faction food stock: %d kcal\nor %d day's rations" ),
                                   camp_food_supply(), camp_food_supply( 0, true ) );
            mission_key.add( { miss_id, false }, mission_ui_activity_of( miss_id ),
                             entry );
        }
        {
            validate_assignees();
            const mission_id miss_id = { Camp_Assign_Jobs, "", base_dir };
            entry = string_format( _( "Notes:\n"
                                      "Assign repeating job duties to NPCs stationed here.\n"
                                      "Difficulty: N/A\n"
                                      "Effects:\n"
                                      "\n\nRisk: None\n"
                                      "Time: Ongoing" ) );
            mission_key.add( {miss_id, false}, mission_ui_activity_of( miss_id ), entry );
        }
        {
            const mission_id miss_id = { Camp_Assign_Workers, "", base_dir };
            entry = string_format( _( "Notes:\n"
                                      "Assign followers to work at this camp." ) );
            mission_key.add( {miss_id, false}, mission_ui_activity_of( miss_id ), entry );
        }
        {
            const mission_id miss_id = { Camp_Abandon, "", base_dir };
            entry = _( "Notes:\nAbandon this camp" );
            mission_key.add( {miss_id, false}, mission_ui_activity_of( miss_id ), entry );
        }
    }
    // Missions assigned to the central tile that could be done by an expansion
    get_available_missions_by_dir( mission_key, base_camps::base_dir );

    // Loop over expansions
    for( const point &dir : directions ) {
        get_available_missions_by_dir( mission_key, dir );
    }

    if( !camp_workers.empty() ) {
        const mission_id miss_id = { Camp_Emergency_Recall, "", base_dir };
        entry = string_format( _( "Notes:\n"
                                  "Cancel a current mission and force the immediate return of a "
                                  "companion.  No work will be done on the mission and all "
                                  "resources used on the mission will be lost.\n\n"
                                  "WARNING: All resources used on the mission will be lost and "
                                  "no work will be done.  Only use this mission to recover a "
                                  "companion who cannot otherwise be recovered.\n\n"
                                  "Companions must be on missions for at least 24 hours before "
                                  "emergency recall becomes available." ) );
        bool avail = update_emergency_recall( entry, camp_workers, 24_hours );
        mission_key.add_return( miss_id, _( "Emergency Recall" ),
                                entry, avail );
    }
}

bool basecamp::handle_mission( const ui_mission_id &miss_id )
{
    if( miss_id.id.id == No_Mission ) {
        return true;
    }

    const point &miss_dir = miss_id.id.dir.value();
    //  All missions should supply dir. Bug if they don't, so blow up during testing.

    const tripoint_abs_omt omt_trg = omt_pos + miss_dir;

    switch( miss_id.id.id ) {
        case Camp_Distribute_Food:
            distribute_food();
            break;

        case Camp_Assign_Jobs:
            job_assignment_ui();
            break;

        case Camp_Assign_Workers:
            worker_assignment_ui();
            break;

        case Camp_Abandon:
            abandon_camp();
            break;

        case Camp_Upgrade:
            if( miss_id.ret ) {
                upgrade_return( miss_id.id );
            } else {
                start_upgrade( miss_id.id );
            }
            break;

        case Camp_Emergency_Recall:
            emergency_recall( miss_id.id );
            break;

        case Camp_Crafting:
            if( miss_id.ret ) {
                const std::string bldg = recipe_group::get_building_of_recipe( miss_id.id.parameters );

                std::string msg;

                if( bldg == base_recipe_group_string ) {
                    msg = _( "returns to you with something…" );

                } else if( bldg == cook_recipe_group_string ) {
                    msg = _( "returns from your kitchen with something…" );

                } else if( bldg == farm_recipe_group_string ) {
                    msg = _( "returns from your farm with something…" );

                } else if( bldg == smith_recipe_group_string ) {
                    msg = _( "returns from your blacksmith shop with something…" );
                }

                else {
                    msg = _( "returns to you with something…" );
                }

                crafting_mission_return( miss_id.id,
                                         msg,
                                         skill_construction.str(), 2 );
            } else {
                start_crafting( recipe_group::get_building_of_recipe( miss_id.id.parameters ), miss_id.id );
            }
            break;

        case Camp_Gather_Materials:
            if( miss_id.ret ) {
                gathering_return( miss_id.id, 3_hours );
            } else {
                start_mission( miss_id.id, 3_hours, true,
                               _( "departs to search for materials…" ), false, {}, skill_survival, 0 );
            }
            break;

        case Camp_Collect_Firewood:
            if( miss_id.ret ) {
                gathering_return( miss_id.id, 3_hours );
            } else {
                start_mission( miss_id.id, 3_hours, true,
                               _( "departs to search for firewood…" ), false, {}, skill_survival, 0 );
            }
            break;

        case Camp_Menial:
            if( miss_id.ret ) {
                menial_return( miss_id.id );
            } else {
                start_menial_labor();
            }
            break;

        case Camp_Survey_Expansion:
            if( miss_id.ret ) {
                survey_return( miss_id.id );
            } else {
                start_mission( miss_id.id, 3_hours, true,
                               _( "departs to survey land…" ), false, {}, skill_gun, 0 );
            }
            break;

        case Camp_Cut_Logs:
            if( miss_id.ret ) {
                mission_return( miss_id.id, 6_hours, true,
                                _( "returns from working in the woods…" ),
                                skill_construction.str(), 2 );
            } else {
                start_cut_logs( miss_id.id );
            }
            break;

        case Camp_Clearcut:
            if( miss_id.ret ) {
                mission_return( miss_id.id, 6_hours, true,
                                _( "returns from working in the woods…" ),
                                skill_construction.str(), 1 );
            } else {
                start_clearcut( miss_id.id );
            }
            break;

        case Camp_Setup_Hide_Site:
            if( miss_id.ret ) {
                mission_return( miss_id.id, 3_hours, true,
                                _( "returns from working on the hide site…" ), skill_survival.str(), 3 );
            } else {
                start_setup_hide_site( miss_id.id );
            }
            break;

        case Camp_Relay_Hide_Site:
            if( miss_id.ret ) {
                const std::string msg = _( "returns from shuttling gear between the hide site…" );
                mission_return( miss_id.id, 3_hours, true,
                                msg, skill_survival.str(), 3 );
            } else {
                start_relay_hide_site( miss_id.id );
            }
            break;

        case Camp_Foraging:
            if( miss_id.ret ) {
                gathering_return( miss_id.id, 4_hours );
            } else {
                start_mission( miss_id.id, 4_hours, true,
                               _( "departs to search for edible plants…" ), false, {}, skill_survival, 0 );
            }
            break;

        case Camp_Trapping:
            if( miss_id.ret ) {
                gathering_return( miss_id.id, 6_hours );
            } else {
                start_mission( miss_id.id, 6_hours, true,
                               _( "departs to set traps for small animals…" ), false, {}, skill_traps, 0 );
            }
            break;

        case Camp_Hunting:
            if( miss_id.ret ) {
                gathering_return( miss_id.id, 6_hours );
            } else {
                start_mission( miss_id.id, 6_hours, true,
                               _( "departs to hunt for meat…" ), false, {}, skill_gun, 0 );
            }
            break;

        case Camp_OM_Fortifications:
            if( miss_id.ret ) {
                fortifications_return( miss_id.id );
            } else {
                std::string bldg_exp = miss_id.id.parameters;
                start_fortifications( miss_id.id );
            }
            break;

        case Camp_Recruiting:
            if( miss_id.ret ) {
                recruit_return( miss_id.id,
                                recruit_evaluation() );
            } else {
                start_mission( miss_id.id, 4_days, true,
                               _( "departs to search for recruits…" ), false, {}, skill_gun, 0 );
            }
            break;

        case Camp_Scouting:
        case Camp_Combat_Patrol:
            if( miss_id.ret ) {
                combat_mission_return( miss_id.id );
            } else {
                start_combat_mission( miss_id.id );
            }
            break;

        case Camp_Chop_Shop:  //  Removed during 0.E
            debugmsg( "Obsolete Function.  Use Vehicle Deconstruct zone instead.  Recover your companion with Emergency Recall." );
            break;

        case Camp_Plow:
        case Camp_Plant:
        case Camp_Harvest:
            if( miss_id.ret ) {
                farm_return( miss_id.id, omt_trg );
            } else {
                start_farm_op( omt_trg, miss_id.id );
            }
            break;

        default:
            break;
    }

    return true;
}

// camp faction companion mission start functions
npc_ptr basecamp::start_mission( const mission_id &miss_id, time_duration duration,
                                 bool must_feed, const std::string &desc, bool /*group*/,
                                 const std::vector<item *> &equipment,
                                 const skill_id &skill_tested, int skill_level )
{
    std::map<skill_id, int> required_skills;
    required_skills[ skill_tested ] = skill_level;
    return start_mission( miss_id, duration, must_feed, desc, false, equipment, required_skills );
}

npc_ptr basecamp::start_mission( const mission_id &miss_id, time_duration duration,
                                 bool must_feed, const std::string &desc, bool /*group*/,
                                 const std::vector<item *> &equipment,
                                 const std::map<skill_id, int> &required_skills )
{
    if( must_feed && camp_food_supply() < time_to_food( duration ) ) {
        popup( _( "You don't have enough food stored to feed your companion." ) );
        return nullptr;
    }
    npc_ptr comp = talk_function::individual_mission( omt_pos, base_camps::id, desc, miss_id,
                   false, equipment, required_skills );
    if( comp != nullptr ) {
        comp->companion_mission_time_ret = calendar::turn + duration;
        if( must_feed ) {
            camp_food_supply( duration );
        }
    }
    return comp;
}

void basecamp::start_upgrade( const mission_id &miss_id )
{
    const recipe &making = recipe_id( miss_id.parameters ).obj();
    if( making.get_blueprint().str() == faction_expansion_salt_water_pipe_swamp_N ) {
        start_salt_water_pipe( miss_id );
        return;
    } else if( making.get_blueprint().str() == faction_expansion_salt_water_pipe_N ) {
        continue_salt_water_pipe( miss_id );
        return;
    }

    //Stop upgrade if you don't have materials
    if( making.deduped_requirements().can_make_with_inventory(
            _inv, making.get_component_filter() ) ) {
        bool must_feed = miss_id.parameters != "faction_base_camp_1";

        basecamp_action_components components( making, 1, *this );
        if( !components.choose_components() ) {
            return;
        }

        time_duration work_days = base_camps::to_workdays( making.batch_duration(
                                      get_player_character() ) );
        npc_ptr comp = nullptr;
        if( making.required_skills.empty() ) {
            if( making.skill_used.is_valid() ) {
                comp = start_mission( miss_id, work_days, must_feed,
                                      _( "begins to upgrade the camp…" ), false, {},
                                      making.skill_used, making.difficulty );
            } else {
                comp = start_mission( miss_id, work_days, must_feed,
                                      _( "begins to upgrade the camp…" ), false, {} );
            }
        } else {
            comp = start_mission( miss_id, work_days, must_feed, _( "begins to upgrade the camp…" ),
                                  false, {}, making.required_skills );
        }
        if( comp == nullptr ) {
            return;
        }
        components.consume_components();
        update_in_progress( miss_id.parameters,
                            miss_id.dir.value() );  //  Dir should always have a value for upgrades.
    } else {
        popup( _( "You don't have the materials for the upgrade." ) );
    }
}

void basecamp::abandon_camp()
{
    validate_assignees();
    for( npc_ptr &guy : overmap_buffer.get_companion_mission_npcs( 10 ) ) {
        npc_companion_mission c_mission = guy->get_companion_mission();
        if( c_mission.role_id != base_camps::id ) {
            continue;
        }
        const std::string return_msg = _( "responds to the emergency recall…" );
        finish_return( *guy, false, return_msg, skill_menial.str(), 0, true );
    }
    for( npc_ptr &guy : get_npcs_assigned() ) {
        talk_function::stop_guard( *guy );
    }
    overmap_buffer.remove_camp( *this );
    map &here = get_map();
    const tripoint sm_pos = omt_to_sm_copy( omt_pos.raw() );
    const tripoint ms_pos = sm_to_ms_copy( sm_pos );
    // We cannot use bb_pos here, because bb_pos may be {0,0,0} if you haven't examined the bulletin board on camp ever.
    // here.remove_submap_camp( here.getlocal( bb_pos ) );
    here.remove_submap_camp( here.getlocal( ms_pos ) );
    add_msg( m_info, _( "You abandon %s." ), name );
}

void basecamp::worker_assignment_ui()
{
    int entries_per_page = 0;
    catacurses::window w_followers;

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        const point term( TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 2 : 0,
                          TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 2 : 0 );

        w_followers = catacurses::newwin( FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH,
                                          point( term.y, term.x ) );
        entries_per_page = FULL_SCREEN_HEIGHT - 5;

        ui.position_from_window( w_followers );
    } );
    ui.mark_resize();

    size_t selection = 0;
    input_context ctxt( "FACTION_MANAGER" );
    ctxt.register_action( "INSPECT_NPC" );
    ctxt.register_updown();
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    validate_assignees();
    g->validate_npc_followers();

    std::vector<npc *> followers;
    npc *cur_npc = nullptr;

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_followers );

        // entries_per_page * page number
        const size_t top_of_page = entries_per_page * ( selection / entries_per_page );

        for( int i = 0; i < FULL_SCREEN_HEIGHT - 2; i++ ) {
            mvwputch( w_followers, point( 45, i ), BORDER_COLOR, LINE_XOXO );
        }
        draw_border( w_followers );
        const nc_color col = c_white;
        const std::string no_npcs = _( "You have no companions following you." );
        if( !followers.empty() ) {
            draw_scrollbar( w_followers, selection, entries_per_page, followers.size(),
                            point( 0, 3 ) );
            for( size_t i = top_of_page; i < followers.size(); i++ ) {
                const int y = i - top_of_page + 3;
                trim_and_print( w_followers, point( 1, y ), 43, selection == i ? hilite( col ) : col,
                                followers[i]->disp_name() );
            }
        } else {
            mvwprintz( w_followers, point( 1, 4 ), c_light_red, no_npcs );
        }
        mvwprintz( w_followers, point( 1, FULL_SCREEN_HEIGHT - 2 ), c_light_gray,
                   _( "Press %s to inspect this follower." ), ctxt.get_desc( "INSPECT_NPC" ) );
        mvwprintz( w_followers, point( 1, FULL_SCREEN_HEIGHT - 1 ), c_light_gray,
                   _( "Press %s to assign this follower to this camp." ), ctxt.get_desc( "CONFIRM" ) );
        wnoutrefresh( w_followers );
    } );

    while( true ) {
        // create a list of npcs stationed at this camp
        followers.clear();
        for( const character_id &elem : g->get_follower_list() ) {
            shared_ptr_fast<npc> npc_to_get = overmap_buffer.find_npc( elem );
            if( !npc_to_get || !npc_to_get->is_following() || npc_to_get->is_hallucination() ) {
                continue;
            }
            npc *npc_to_add = npc_to_get.get();
            followers.push_back( npc_to_add );
        }
        cur_npc = nullptr;
        if( !followers.empty() ) {
            cur_npc = followers[selection];
        }

        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        if( action == "INSPECT_NPC" ) {
            if( cur_npc ) {
                cur_npc->disp_info();
            }
        } else if( action == "DOWN" ) {
            selection++;
            if( selection >= followers.size() ) {
                selection = 0;
            }
        } else if( action == "UP" ) {
            if( selection == 0 ) {
                selection = followers.empty() ? 0 : followers.size() - 1;
            } else {
                selection--;
            }
        } else if( action == "CONFIRM" ) {
            if( !followers.empty() && cur_npc ) {
                talk_function::assign_camp( *cur_npc );
            }
        } else if( action == "QUIT" ) {
            break;
        }
    }
}

void basecamp::job_assignment_ui()
{
    int entries_per_page = 0;
    catacurses::window w_jobs;

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        const point term( TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 2 : 0,
                          TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 2 : 0 );

        w_jobs = catacurses::newwin( FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH,
                                     point( term.y, term.x ) );

        entries_per_page = FULL_SCREEN_HEIGHT - 5;

        ui.position_from_window( w_jobs );
    } );
    ui.mark_resize();

    size_t selection = 0;
    input_context ctxt( "FACTION_MANAGER" );
    ctxt.register_action( "INSPECT_NPC" );
    ctxt.register_updown();
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    validate_assignees();

    std::vector<npc *> stationed_npcs;
    npc *cur_npc = nullptr;

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w_jobs );
        const size_t top_of_page = entries_per_page * ( selection / entries_per_page );
        for( int i = 0; i < FULL_SCREEN_HEIGHT - 2; i++ ) {
            mvwputch( w_jobs, point( 45, i ), BORDER_COLOR, LINE_XOXO );
        }
        draw_border( w_jobs );
        mvwprintz( w_jobs, point( 46, 1 ), c_white, _( "Job/Priority" ) );
        const nc_color col = c_white;
        const std::string no_npcs = _( "There are no npcs stationed here" );
        if( !stationed_npcs.empty() ) {
            draw_scrollbar( w_jobs, selection, entries_per_page, stationed_npcs.size(),
                            point( 0, 3 ) );
            for( size_t i = top_of_page; i < stationed_npcs.size(); i++ ) {
                const int y = i - top_of_page + 3;
                trim_and_print( w_jobs, point( 1, y ), 43, selection == i ? hilite( col ) : col,
                                stationed_npcs[i]->disp_name() );
            }
            if( selection < stationed_npcs.size() ) {
                int start_y = 3;
                if( cur_npc ) {
                    if( cur_npc->has_job() ) {
                        for( activity_id &elem : cur_npc->job.get_prioritised_vector() ) {
                            const int priority = cur_npc->job.get_priority_of_job( elem );
                            player_activity test_act = player_activity( elem );
                            mvwprintz( w_jobs, point( 46, start_y ), c_light_gray, string_format( _( "%s : %s" ),
                                       test_act.get_verb(), std::to_string( priority ) ) );
                            start_y++;
                        }
                    } else {
                        mvwprintz( w_jobs, point( 46, start_y ), c_light_red, _( "No current job." ) );
                    }
                }
            } else {
                mvwprintz( w_jobs, point( 46, 4 ), c_light_red, no_npcs );
            }
        } else {
            mvwprintz( w_jobs, point( 46, 4 ), c_light_red, no_npcs );
        }
        mvwprintz( w_jobs, point( 1, FULL_SCREEN_HEIGHT - 2 ), c_light_gray,
                   _( "Press %s to inspect this follower." ), ctxt.get_desc( "INSPECT_NPC" ) );
        mvwprintz( w_jobs, point( 1, FULL_SCREEN_HEIGHT - 1 ), c_light_gray,
                   _( "Press %s to change this workers job priorities." ), ctxt.get_desc( "CONFIRM" ) );
        wnoutrefresh( w_jobs );
    } );

    while( true ) {
        // create a list of npcs stationed at this camp
        stationed_npcs.clear();
        for( const auto &elem : get_npcs_assigned() ) {
            if( elem ) {
                stationed_npcs.push_back( elem.get() );
            }
        }
        cur_npc = nullptr;
        // entries_per_page * page number
        if( !stationed_npcs.empty() ) {
            cur_npc = stationed_npcs[selection];
        }

        ui_manager::redraw();

        const std::string action = ctxt.handle_input();
        if( action == "INSPECT_NPC" ) {
            if( cur_npc ) {
                cur_npc->disp_info();
            }
        } else if( action == "DOWN" ) {
            selection++;
            if( selection >= stationed_npcs.size() ) {
                selection = 0;
            }
        } else if( action == "UP" ) {
            if( selection == 0 ) {
                selection = stationed_npcs.empty() ? 0 : stationed_npcs.size() - 1;
            } else {
                selection--;
            }
        } else if( action == "CONFIRM" ) {
            if( cur_npc ) {
                while( true ) {
                    uilist smenu;
                    smenu.text = _( "Assign job priority ( 0 to disable )" );
                    int count = 0;
                    std::vector<activity_id> job_vec = cur_npc->job.get_prioritised_vector();
                    smenu.addentry( count, true, 'C', _( "Clear all priorities" ) );
                    count++;
                    for( const activity_id &elem : job_vec ) {
                        player_activity test_act = player_activity( elem );
                        const int priority = cur_npc->job.get_priority_of_job( elem );
                        smenu.addentry( count, true, MENU_AUTOASSIGN, string_format( _( "%s : %s" ), test_act.get_verb(),
                                        std::to_string( priority ) ) );
                        count++;
                    }
                    smenu.query();
                    if( smenu.ret == UILIST_CANCEL ) {
                        break;
                    }
                    if( smenu.ret == 0 ) {
                        cur_npc->job.clear_all_priorities();
                    } else if( smenu.ret > 0 && smenu.ret <= static_cast<int>( job_vec.size() ) ) {
                        activity_id sel_job = job_vec[smenu.ret - 1];
                        player_activity test_act = player_activity( sel_job );
                        const std::string formatted = string_format( _( "Priority for %s " ), test_act.get_verb() );
                        const int amount = string_input_popup()
                                           .title( formatted )
                                           .width( 20 )
                                           .only_digits( true )
                                           .query_int();
                        cur_npc->job.set_task_priority( sel_job, amount );
                    } else {
                        break;
                    }
                }
            }
        } else if( action == "QUIT" ) {
            break;
        }
    }
}

void basecamp::start_menial_labor()
{
    if( camp_food_supply() < time_to_food( 3_hours ) ) {
        popup( _( "You don't have enough food stored to feed your companion." ) );
        return;
    }
    shared_ptr_fast<npc> comp = talk_function::companion_choose();
    if( comp == nullptr ) {
        return;
    }
    validate_sort_points();

    comp->assign_activity( ACT_MOVE_LOOT );
    popup( _( "%s goes off to clean toilets and sort loot." ), comp->disp_name() );
}

void basecamp::start_cut_logs( const mission_id miss_id )
{
    std::vector<std::string> log_sources = { "forest", "forest_thick", "forest_water", "forest_trail" };
    popup( _( "Forests and swamps are the only valid cutting locations." ) );
    tripoint_abs_omt forest = om_target_tile( omt_pos, 1, 50, log_sources, ot_match_type::type );
    if( forest != tripoint_abs_omt( -999, -999, -999 ) ) {
        standard_npc sample_npc( "Temp" );
        sample_npc.set_fake( true );
        int tree_est = om_cutdown_trees_est( forest, 50 );
        int tree_young_est = om_harvest_ter_est( sample_npc, forest,
                             ter_id( "t_tree_young" ), 50 );
        int dist = rl_dist( forest.xy(), omt_pos.xy() );
        //Very roughly what the player does + 6 hours for prep, clean up, breaks
        time_duration chop_time = 6_hours + 1_hours * tree_est + 7_minutes * tree_young_est;
        int haul_items = 2 * tree_est + 3 * tree_young_est;
        time_duration travel_time = companion_travel_time_calc( forest, omt_pos, 0_minutes,
                                    2, haul_items );
        time_duration work_time = travel_time + chop_time;
        if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( work_time,
                       chop_time, travel_time, dist, 2, time_to_food( work_time ) ) ) ) {
            return;
        }

        npc_ptr comp = start_mission( miss_id,
                                      work_time, true,
                                      _( "departs to cut logs…" ), false, {},
                                      skill_fabrication, 2 );
        if( comp != nullptr ) {
            om_cutdown_trees_logs( forest, 50 );
            om_harvest_ter( *comp, forest, ter_id( "t_tree_young" ), 50 );
            mass_volume harvest = om_harvest_itm( comp, forest, 95 );
            // recalculate trips based on actual load and capacity
            travel_time = companion_travel_time_calc( forest, omt_pos, 0_minutes, 2,
                          harvest.count );
            work_time = travel_time + chop_time;
            comp->companion_mission_time_ret = calendar::turn + work_time;
            //If we cleared a forest...
            if( om_cutdown_trees_est( forest ) < 5 ) {
                const oter_id &omt_trees = overmap_buffer.ter( forest );
                //Do this for swamps "forest_wet" if we have a swamp without trees...
                if( omt_trees.id() != oter_forest_wet ) {
                    overmap_buffer.ter_set( forest, oter_id( "field" ) );
                }
            }
        }
    }
}

void basecamp::start_clearcut( const mission_id miss_id )
{
    std::vector<std::string> log_sources = { "forest", "forest_thick", "forest_trail" };
    popup( _( "Forests are the only valid cutting locations." ) );
    tripoint_abs_omt forest = om_target_tile( omt_pos, 1, 50, log_sources, ot_match_type::type );
    if( forest != tripoint_abs_omt( -999, -999, -999 ) ) {
        standard_npc sample_npc( "Temp" );
        sample_npc.set_fake( true );
        int tree_est = om_cutdown_trees_est( forest, 95 );
        int tree_young_est = om_harvest_ter_est( sample_npc, forest,
                             ter_id( "t_tree_young" ), 95 );
        int dist = rl_dist( forest.xy(), omt_pos.xy() );
        //Very roughly what the player does + 6 hours for prep, clean up, breaks
        time_duration chop_time = 6_hours + 1_hours * tree_est + 7_minutes * tree_young_est;
        time_duration travel_time = companion_travel_time_calc( forest, omt_pos, 0_minutes, 2 );
        time_duration work_time = travel_time + chop_time;
        if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( work_time,
                       chop_time, travel_time, dist, 2, time_to_food( work_time ) ) ) ) {
            return;
        }

        npc_ptr comp = start_mission( miss_id,
                                      work_time,
                                      true, _( "departs to clear a forest…" ), false, {},
                                      skill_fabrication, 1 );
        if( comp != nullptr ) {
            om_cutdown_trees_trunks( forest, 95 );
            om_harvest_ter_break( *comp, forest, ter_id( "t_tree_young" ), 95 );
            //If we cleared a forest...
            if( om_cutdown_trees_est( forest ) < 5 ) {
                overmap_buffer.ter_set( forest, oter_id( "field" ) );
            }
        }
    }
}

void basecamp::start_setup_hide_site( const mission_id miss_id )
{
    std::vector<std::string> hide_locations = { "forest", "forest_thick", "forest_water", "forest_trail"
                                                "field"
                                              };
    popup( _( "Forests, swamps, and fields are valid hide site locations." ) );
    tripoint_abs_omt forest = om_target_tile( omt_pos, 10, 90, hide_locations, ot_match_type::type,
                              true, true, omt_pos, true );
    if( forest != tripoint_abs_omt( -999, -999, -999 ) ) {
        int dist = rl_dist( forest.xy(), omt_pos.xy() );
        inventory tgt_inv = *get_player_character().inv;
        std::vector<item *> pos_inv = tgt_inv.items_with( []( const item & itm ) {
            return !itm.can_revive();
        } );
        if( !pos_inv.empty() ) {
            std::vector<item *> losing_equipment = give_equipment( pos_inv,
                                                   _( "Do you wish to give your companion "
                                                           "additional items?" ) );
            int trips = om_carry_weight_to_trips( losing_equipment );
            int haulage = trips <= 2 ? 0 : losing_equipment.size();
            time_duration build_time = 6_hours;
            time_duration travel_time = companion_travel_time_calc( forest, omt_pos, 0_minutes,
                                        2, haulage );
            time_duration work_time = travel_time + build_time;
            if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( work_time,
                           build_time, travel_time, dist, trips, time_to_food( work_time ) ) ) ) {
                return;
            }
            npc_ptr comp = start_mission( miss_id,
                                          work_time, true,
                                          _( "departs to build a hide site…" ), false, {},
                                          skill_survival, 3 );
            if( comp != nullptr ) {
                trips = om_carry_weight_to_trips( losing_equipment, comp );
                haulage = trips <= 2 ? 0 : losing_equipment.size();
                work_time = companion_travel_time_calc( forest, omt_pos, 0_minutes, 2, haulage ) +
                            build_time;
                comp->companion_mission_time_ret = calendar::turn + work_time;
                om_set_hide_site( *comp, forest, losing_equipment );
            }
        } else {
            popup( _( "You need equipment to setup a hide site…" ) );
        }
    }
}

void basecamp::start_relay_hide_site( const mission_id miss_id )
{
    std::vector<std::string> hide_locations = { faction_hide_site_0_string };
    popup( _( "You must select an existing hide site." ) );
    tripoint_abs_omt forest = om_target_tile( omt_pos, 10, 90, hide_locations, ot_match_type::exact,
                              true, true, omt_pos, true );
    if( forest != tripoint_abs_omt( -999, -999, -999 ) ) {
        int dist = rl_dist( forest.xy(), omt_pos.xy() );
        inventory tgt_inv = *get_player_character().inv;
        std::vector<item *> pos_inv = tgt_inv.items_with( []( const item & itm ) {
            return !itm.can_revive();
        } );
        std::vector<item *> losing_equipment;
        if( !pos_inv.empty() ) {
            losing_equipment = give_equipment( pos_inv,
                                               _( "Do you wish to give your companion "
                                                  "additional items?" ) );
        }
        //Check items in improvised shelters at hide site
        tinymap target_bay;
        target_bay.load( project_to<coords::sm>( forest ), false );
        std::vector<item *> hide_inv;
        for( item &i : target_bay.i_at( point( 11, 10 ) ) ) {
            hide_inv.push_back( &i );
        }
        std::vector<item *> gaining_equipment;
        if( !hide_inv.empty() ) {
            gaining_equipment = give_equipment( hide_inv, _( "Bring gear back?" ) );
        }
        if( !losing_equipment.empty() || !gaining_equipment.empty() ) {
            //Only get charged the greater trips since return is free for both
            int trips = std::max( om_carry_weight_to_trips( gaining_equipment ),
                                  om_carry_weight_to_trips( losing_equipment ) );
            int haulage = trips <= 2 ? 0 : std::max( gaining_equipment.size(),
                          losing_equipment.size() );
            time_duration build_time = 6_hours;
            time_duration travel_time = companion_travel_time_calc( forest, omt_pos, 0_minutes,
                                        trips, haulage );
            time_duration work_time = travel_time + build_time;
            if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( work_time, build_time,
                           travel_time, dist, trips, time_to_food( work_time ) ) ) ) {
                return;
            }

            npc_ptr comp = start_mission( miss_id,
                                          work_time, true,
                                          _( "departs for the hide site…" ), false, {},
                                          skill_survival, 3 );
            if( comp != nullptr ) {
                // recalculate trips based on actual load
                trips = std::max( om_carry_weight_to_trips( gaining_equipment, comp ),
                                  om_carry_weight_to_trips( losing_equipment, comp ) );
                int haulage = trips <= 2 ? 0 : std::max( gaining_equipment.size(),
                              losing_equipment.size() );
                work_time = companion_travel_time_calc( forest, omt_pos, 0_minutes, trips,
                                                        haulage ) + build_time;
                comp->companion_mission_time_ret = calendar::turn + work_time;
                om_set_hide_site( *comp, forest, losing_equipment, gaining_equipment );
            }
        } else {
            popup( _( "You need equipment to transport between the hide site…" ) );
        }
    }
}

void basecamp::start_fortifications( const mission_id miss_id )
{
    std::vector<std::string> allowed_locations = {
        "forest", "forest_thick", "forest_water", "forest_trail", "field"
    };
    popup( _( "Select a start and end point.  Line must be straight.  Fields, forests, and "
              "swamps are valid fortification locations.  In addition to existing fortification "
              "constructions." ) );
    tripoint_abs_omt start = om_target_tile( omt_pos, 2, 90, allowed_locations, ot_match_type::type );
    popup( _( "Select an end point." ) );
    tripoint_abs_omt stop = om_target_tile( omt_pos, 2, 90, allowed_locations, ot_match_type::type,
                                            true, false, start );
    if( start != tripoint_abs_omt( -999, -999, -999 ) &&
        stop != tripoint_abs_omt( -999, -999, -999 ) ) {
        const recipe &making = recipe_id( miss_id.parameters ).obj();
        bool change_x = start.x() != stop.x();
        bool change_y = start.y() != stop.y();
        if( change_x && change_y ) {
            popup( _( "Construction line must be straight!" ) );
            return;
        }
        if( miss_id.parameters == faction_wall_level_n_1_string ) {
            std::vector<tripoint_abs_omt> tmp_line = line_to( stop, start );
            int line_count = tmp_line.size();
            int yes_count = 0;
            for( auto elem : tmp_line ) {
                if( std::find( fortifications.begin(), fortifications.end(), elem ) != fortifications.end() ) {
                    yes_count += 1;
                }
            }
            if( yes_count < line_count ) {
                popup( _( "Spiked pits must be built over existing trenches!" ) );
                return;
            }
        }
        std::vector<tripoint_abs_omt> fortify_om;
        if( ( change_x && stop.x() < start.x() ) || ( change_y && stop.y() < start.y() ) ) {
            //line_to doesn't include the origin point
            fortify_om.push_back( stop );
            std::vector<tripoint_abs_omt> tmp_line = line_to( stop, start );
            fortify_om.insert( fortify_om.end(), tmp_line.begin(), tmp_line.end() );
        } else {
            fortify_om.push_back( start );
            std::vector<tripoint_abs_omt> tmp_line = line_to( start, stop );
            fortify_om.insert( fortify_om.end(), tmp_line.begin(), tmp_line.end() );
        }
        int trips = 0;
        time_duration build_time = 0_hours;
        time_duration travel_time = 0_hours;
        int dist = 0;
        for( auto fort_om : fortify_om ) {
            bool valid = false;
            const oter_id &omt_ref = overmap_buffer.ter( fort_om );
            for( const std::string &pos_om : allowed_locations ) {
                if( omt_ref.id().c_str() == pos_om ) {
                    valid = true;
                    break;
                }
            }

            if( !valid ) {
                popup( _( "Invalid terrain in construction path." ) );
                return;
            }
            trips += 2;
            build_time += making.batch_duration( get_player_character() );
            dist += rl_dist( fort_om.xy(), omt_pos.xy() );
            travel_time += companion_travel_time_calc( fort_om, omt_pos, 0_minutes, 2 );
        }
        time_duration total_time = base_camps::to_workdays( travel_time + build_time );
        int need_food = time_to_food( total_time );
        if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( total_time, build_time,
                       travel_time, dist, trips, need_food ) ) ) {
            return;
        } else if( !making.deduped_requirements().can_make_with_inventory( _inv,
                   making.get_component_filter(), ( fortify_om.size() * 2 ) - 2 ) ) {
            popup( _( "You don't have the material to build the fortification." ) );
            return;
        }

        const int batch_size = fortify_om.size() * 2 - 2;
        basecamp_action_components components( making, batch_size, *this );
        if( !components.choose_components() ) {
            return;
        }

        npc_ptr comp = start_mission(
                           miss_id, total_time, true,
                           _( "begins constructing fortifications…" ), false, {},
                           making.required_skills );
        if( comp != nullptr ) {
            components.consume_components();
            for( auto pt : fortify_om ) {
                comp->companion_mission_points.push_back( pt );
            }
        }
    }
}

static const int max_salt_water_pipe_distance = 10;
static const int max_salt_water_pipe_length =
    20;  //  It has to be able to wind around terrain it can't pass through, like the rest of the camp.

//  Hard coded strings used to construct expansion "provides" needed by recipes to coordinate salt water pipe construction
static const std::string salt_water_pipe_string_base = "salt_water_pipe_";
static const std::string salt_water_pipe_string_suffix = "_scheduled";
static const double diagonal_salt_pipe_cost = sqrt( 2.0 );
static const double salt_pipe_legal = 0.0;
static const double salt_pipe_illegal = -0.1;
static const double salt_pipe_swamp = -0.2;

//  The logic discourages diagonal connections when there are horizontal ones of the same number of tiles, as the original approach
//  resulted in rather odd paths. At the time of this writing there is no corresponding construction cost difference, though, as that
//  doesn't match with the fixed recipe approach taken.
static point check_salt_pipe_neighbors( double path_map[2 * max_salt_water_pipe_distance + 1][2 *
                                        max_salt_water_pipe_distance + 1],
                                        point pt );  //  Uglified to point parameter by demand from clang-tidy

point check_salt_pipe_neighbors( double path_map[2 * max_salt_water_pipe_distance + 1][2 *
                                 max_salt_water_pipe_distance + 1], point pt )
{
    point found = { -999, -999 };
    double lowest_found = -10000.0;
    double cost;

    for( int i = -1; i <= 1; i++ ) {
        for( int k = -1; k <= 1; k++ ) {
            if( pt.x + i > -max_salt_water_pipe_distance &&
                pt.x + i < max_salt_water_pipe_distance &&
                pt.y + k > -max_salt_water_pipe_distance &&
                pt.y + k < max_salt_water_pipe_distance ) {
                if( i != 0 && k != 0 ) {
                    cost = diagonal_salt_pipe_cost;
                } else {
                    cost = 1.0;
                }

                if( path_map[max_salt_water_pipe_distance + pt.x + i][max_salt_water_pipe_distance + pt.y + k] ==
                    salt_pipe_legal ||
                    ( path_map[max_salt_water_pipe_distance + pt.x + i][max_salt_water_pipe_distance + pt.y + k] >
                      0.0 &&
                      path_map[max_salt_water_pipe_distance + pt.x + i][max_salt_water_pipe_distance + pt.y + k] >
                      path_map[max_salt_water_pipe_distance + pt.x][max_salt_water_pipe_distance + pt.y] + cost ) ) {
                    path_map[max_salt_water_pipe_distance + pt.x + i][max_salt_water_pipe_distance + pt.y + k] =
                        path_map[max_salt_water_pipe_distance + pt.x][max_salt_water_pipe_distance + pt.y] + cost;

                } else if( path_map[max_salt_water_pipe_distance + pt.x + i][max_salt_water_pipe_distance + pt.y +
                           k] <=
                           salt_pipe_swamp ) {
                    if( path_map[max_salt_water_pipe_distance + pt.x + i][max_salt_water_pipe_distance + pt.y + k] ==
                        salt_pipe_swamp ||
                        path_map[max_salt_water_pipe_distance + pt.x + i][max_salt_water_pipe_distance + pt.y + k] < -
                        ( path_map[max_salt_water_pipe_distance + pt.x][max_salt_water_pipe_distance + pt.y] + cost ) ) {
                        path_map[max_salt_water_pipe_distance + pt.x + i][max_salt_water_pipe_distance + pt.y + k] = -
                                ( path_map[max_salt_water_pipe_distance + pt.x][max_salt_water_pipe_distance + pt.y] + cost );

                        if( path_map[max_salt_water_pipe_distance + pt.x + i][max_salt_water_pipe_distance + pt.y + k] >
                            lowest_found ) {
                            lowest_found = path_map[max_salt_water_pipe_distance + pt.x + i][max_salt_water_pipe_distance + pt.y
                                           + k];
                            found = pt + point( i, k );
                        }
                    }
                }
            }
        }
    }
    return found;
}

static int salt_water_pipe_segment_of( const recipe &making );

int salt_water_pipe_segment_of( const recipe &making )
{
    int segment_number = -1;
    const auto &requires = making.blueprint_requires();
    for( auto const &element : requires ) {
        if( element.first.substr( 0, salt_water_pipe_string_base.length() ) == salt_water_pipe_string_base
            &&
            element.first.substr( element.first.length() - salt_water_pipe_string_suffix.length(),
                                  salt_water_pipe_string_suffix.length() ) == salt_water_pipe_string_suffix ) {
            try {
                segment_number = stoi( element.first.substr( salt_water_pipe_string_base.length(),
                                       element.first.length() - salt_water_pipe_string_suffix.length() - 1 ) );
            } catch( ... ) {
                std::string msg = "Recipe 'blueprint_requires' that matches the hard coded '";
                msg += salt_water_pipe_string_base;
                msg += "#";
                msg += salt_water_pipe_string_suffix;
                msg += "' pattern without having a number in the # position";
                debugmsg( msg );
                return -1;
            }
            if( segment_number < 1 || segment_number >= max_salt_water_pipe_length ) {
                std::string msg = "Recipe 'blueprint_requires' that matches the hard coded '";
                msg += salt_water_pipe_string_base;
                msg += "#";
                msg += salt_water_pipe_string_suffix;
                msg += "' pattern with a number outside the ones supported and generated by the code";
                debugmsg( msg );
                return -1;
            }
        }
    }
    if( segment_number == -1 ) {
        debugmsg( "Failed to find recipe 'blueprint_requires' that matches the hard coded '" +
                  salt_water_pipe_string_base + "#" + salt_water_pipe_string_suffix + "' pattern" );
    }

    return segment_number;
}

//  Defines the direction of a tile adjacent to an expansion to which the expansion should make a connection.
//  Support operation for the salt water pipe functionality, but might be used if some other functionality
//  has a use for it.
//  The operation (ab)uses the conditional rotation/mirror flags of a recipe that uses a blueprint that isn't
//  actually going to be used as a blueprint for something constructed in the expansion itself to determine
//  the direction of the tile to connect to.
static point connection_direction_of( const point &dir, const recipe &making );

point connection_direction_of( const point &dir, const recipe &making )
{
    point connection_dir = point_north;
    const std::string suffix = base_camps::all_directions.at( dir ).id.substr( 1,
                               base_camps::all_directions.at( dir ).id.length() - 2 );
    int count = 0;

    if( making.has_flag( "MAP_ROTATE_90_IF_" + suffix ) ) {
        connection_dir = point_east;
        count++;
    }
    if( making.has_flag( "MAP_ROTATE_180_IF_" + suffix ) ) {
        connection_dir = point_south;
        count++;
    }
    if( making.has_flag( "MAP_ROTATE_270_IF_" + suffix ) ) {
        connection_dir = point_west;
        count++;
    }
    if( count > 1 ) {
        popup( _( "Bug, Incorrect recipe: More than one rotation per orientation isn't valid" ) );
        return {-999, -999};
    }

    if( making.has_flag( "MAP_MIRROR_HORIZONTAL_IF_" + suffix ) ) {
        connection_dir.x = -connection_dir.x;
    }
    if( making.has_flag( "MAP_MIRROR_VERTICALL_IF_" + suffix ) ) {
        connection_dir.y = -connection_dir.y;
    }

    return connection_dir;
}

bool basecamp::common_salt_water_pipe_construction(
    const mission_id &miss_id, expansion_salt_water_pipe *pipe, int segment_number )
{
    const recipe &making = recipe_id(
                               miss_id.parameters ).obj(); //  Actually a template recipe that we'll rotate and mirror as required.
    time_duration work_days = base_camps::to_workdays( making.batch_duration(
                                  get_player_character() ) );

    int remaining_segments = pipe->segments.size() - 1;

    if( segment_number == 0 ) {
        if( !query_yn( _( "Number of additional sessions required: %i" ), remaining_segments ) ) {
            return false;
        }
    }

    basecamp_action_components components( making, 1, *this );
    if( !components.choose_components() ) {
        return false;
    }

    npc_ptr comp;

    if( segment_number == 0 ) {
        comp = start_mission( miss_id, work_days, true,
                              _( "Start constructing salt water pipes…" ), false, {},
                              making.required_skills );
    } else {
        comp = start_mission( miss_id, work_days, true,
                              _( "Continue constructing salt water pipes…" ), false, {},
                              making.required_skills );
    }

    if( comp != nullptr ) {
        components.consume_components();
        update_in_progress( miss_id.parameters, miss_id.dir.value() );  // Dir will always have a value
        pipe->segments[segment_number].started = true;
    }

    return comp != nullptr;
}

void basecamp::start_salt_water_pipe( const mission_id &miss_id )
{
    const point dir = miss_id.dir.value();  //  Will always have a value
    const recipe &making = recipe_id( miss_id.parameters ).obj();
    point connection_dir = connection_direction_of( dir, making );

    if( connection_dir.x == -999 && connection_dir.y == -999 ) {
        return;
    }

    expansion_salt_water_pipe *pipe = nullptr;
    bool pipe_is_new = true;

    for( expansion_salt_water_pipe *element : salt_water_pipes ) {
        if( element->expansion == dir ) {
            if( element->segments[0].finished ) {
                debugmsg( "Trying to start construction of a salt water pipe that's already been constructed" );
                return;
            }
            //  Assume we've started the construction but it has been cancelled.
            pipe = element;
            pipe_is_new = false;
            break;
        }
    }

    if( pipe_is_new ) {
        pipe = new expansion_salt_water_pipe;
        pipe->expansion = dir;
        pipe->connection_direction = connection_dir;

        std::string allowed_start_location =
            "forest_water";  //  That's what a swamp is called, for some reason.
        std::vector<std::string> allowed_locations = {
            "forest", "forest_thick", "forest_trail", "field", "road"
        };
        double path_map[2 * max_salt_water_pipe_distance + 1][2 * max_salt_water_pipe_distance + 1];

        for( int i = -max_salt_water_pipe_distance; i <= max_salt_water_pipe_distance; i++ ) {
            for( int k = -max_salt_water_pipe_distance; k <= max_salt_water_pipe_distance; k++ ) {
                tripoint_abs_omt tile = tripoint_abs_omt( omt_pos.x() + dir.x + connection_dir.x + i,
                                        omt_pos.y() + dir.y + connection_dir.y + k, omt_pos.z() );
                const oter_id &omt_ref = overmap_buffer.ter( tile );
                bool match = false;
                for( const std::string &pos_om : allowed_locations ) {
                    if( omt_ref->get_type_id() == oter_type_str_id( pos_om ) ) {
                        match = true;
                        break;
                    }
                }
                if( match ) {
                    path_map[max_salt_water_pipe_distance + i][max_salt_water_pipe_distance + k] = salt_pipe_legal;
                } else if( omt_ref->get_type_id() == oter_type_str_id( allowed_start_location ) ) {
                    path_map[max_salt_water_pipe_distance + i][max_salt_water_pipe_distance + k] = salt_pipe_swamp;
                } else {
                    path_map[max_salt_water_pipe_distance + i][max_salt_water_pipe_distance + k] = salt_pipe_illegal;
                }
                //  if this is an expansion tile, forbid it. Only allocated ones have their type changed.
                if( i >= -dir.x - connection_dir.x - 1 && i <= -dir.x - connection_dir.x + 1 &&
                    k >= -dir.y - connection_dir.y - 1 && k <= -dir.y - connection_dir.y + 1 ) {
                    path_map[max_salt_water_pipe_distance + i][max_salt_water_pipe_distance + k] = salt_pipe_illegal;
                }
            }
        }

        if( path_map[max_salt_water_pipe_distance][max_salt_water_pipe_distance] == salt_pipe_illegal ) {
            auto e = expansions.find( dir );
            basecamp::update_provides( miss_id.parameters, e->second );

            popup( _( "This functionality cannot be constructed as the tile directly adjacent to "
                      "this expansion is not of a type a pipe can be constructed through.  Supported "
                      "terrain is forest, field, road, and swamp.  This recipe will now be "
                      "removed from the set of available recipes and won't show up again." ) );
            return;
        }

        point destination;
        double destination_cost = -10000.0;
        bool path_found = false;

        if( path_map[max_salt_water_pipe_distance][max_salt_water_pipe_distance] ==
            salt_pipe_swamp ) { //  The connection_dir tile is a swamp tile
            destination = point_zero;
            path_found = true;
        } else {
            path_map[max_salt_water_pipe_distance][max_salt_water_pipe_distance] =
                1.0;  //  Always an orthogonal connection to the connection tile.

            for( int distance = 1; distance <= max_salt_water_pipe_length; distance++ ) {
                int dist = distance > max_salt_water_pipe_distance ? max_salt_water_pipe_distance : distance;
                for( int i = -dist; i <= dist; i++ ) { //  No path that can be extended can reach further than dist.
                    for( int k = -dist; k <= dist; k++ ) {
                        if( path_map[max_salt_water_pipe_distance + i][max_salt_water_pipe_distance + k] >
                            0.0 ) { // Tile has been assigned a distance and isn't a swamp
                            point temp = check_salt_pipe_neighbors( path_map, { i, k } );
                            if( temp != point( -999, -999 ) ) {
                                if( path_map[max_salt_water_pipe_distance + temp.x][max_salt_water_pipe_distance + temp.y] >
                                    destination_cost ) {
                                    destination_cost = path_map[max_salt_water_pipe_distance + temp.x][max_salt_water_pipe_distance +
                                                       temp.y];
                                    destination = temp;
                                    path_found = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        if( !path_found ) {
            auto e = expansions.find( dir );
            basecamp::update_provides( miss_id.parameters, e->second );

            popup( _( "This functionality cannot be constructed as no valid path to a swamp has "
                      "been found with a maximum length (20 tiles) at a maximum range of 10 tiles.  "
                      "Supported terrain is forest, field, and road.  This recipe will now be "
                      "removed from the set of available recipes and won't show up again." ) );
            return;
        };

        point candidate;
        //  Flip the sign of the starting swamp tile to fit the logic expecting positive values rather than check that it isn't the first one every time.
        path_map[max_salt_water_pipe_distance + destination.x][max_salt_water_pipe_distance + destination.y]
            = -path_map[max_salt_water_pipe_distance + destination.x][max_salt_water_pipe_distance +
                    destination.y];

        while( destination != point_zero ) {
            pipe->segments.push_back( { tripoint_abs_omt( omt_pos.x() + dir.x + connection_dir.x + destination.x, omt_pos.y() + dir.y + connection_dir.y + destination.y, omt_pos.z() ), false, false } );
            path_found = false;  //  Reuse of existing variable after its original usability has been passed.
            for( int i = -1; i <= 1; i++ ) {
                for( int k = -1; k <= 1; k++ ) {
                    if( destination.x + i > -max_salt_water_pipe_distance &&
                        destination.x + i < max_salt_water_pipe_distance &&
                        destination.y + k > -max_salt_water_pipe_distance &&
                        destination.y + k < max_salt_water_pipe_distance ) {
                        if( path_map[max_salt_water_pipe_distance + destination.x + i][max_salt_water_pipe_distance +
                                destination.y + k] > 0.0 &&
                            path_map[max_salt_water_pipe_distance + destination.x + i][max_salt_water_pipe_distance +
                                    destination.y + k] < path_map[max_salt_water_pipe_distance +
                                            destination.x][max_salt_water_pipe_distance + destination.y] ) {
                            if( path_found ) {
                                if( path_map [max_salt_water_pipe_distance + candidate.x][max_salt_water_pipe_distance +
                                        candidate.y] >
                                    path_map[max_salt_water_pipe_distance + destination.x + i][max_salt_water_pipe_distance +
                                            destination.y + k] ) {
                                    candidate = destination + point( i, k );
                                }
                            } else {
                                candidate = destination +  point( i, k );
                                path_found = true;
                            }
                        }
                    }
                }
            }
            destination = candidate;
        }

        pipe->segments.push_back( { tripoint_abs_omt( omt_pos.x() + dir.x + connection_dir.x, omt_pos.y() + dir.y + connection_dir.y, omt_pos.z() ), false, false } );
    }

    if( common_salt_water_pipe_construction( miss_id, pipe, 0 ) ) {
        if( pipe_is_new ) {
            pipe->segments[0].started = true;
            salt_water_pipes.push_back( pipe );

            //  Provide "salt_water_pipe_*_scheduled" for all the segments needed.
            //  The guts of basecamp::update_provides modified to feed it generated tokens rather than
            //  those actually in the recipe, as the tokens needed can't be determined by the recipe.
            //  Shouldn't need to check that the tokens don't exist previously, so could just set them to 1.
            auto e = expansions.find( dir );
            for( size_t i = 1; i < pipe->segments.size(); i++ ) {
                std::string token = salt_water_pipe_string_base;
                token += std::to_string( i );
                token += salt_water_pipe_string_suffix;
                if( e->second.provides.find( token ) == e->second.provides.end() ) {
                    e->second.provides[token] = 0;
                }
                e->second.provides[token]++;
            }
        }
    } else if( pipe_is_new ) {
        delete pipe;
    }
}

void basecamp::continue_salt_water_pipe( const mission_id &miss_id )
{
    const point dir = miss_id.dir.value();  //  Dir will always have a value
    const recipe &making = recipe_id( miss_id.parameters ).obj();

    expansion_salt_water_pipe *pipe = nullptr;

    bool pipe_is_new = true;
    for( expansion_salt_water_pipe *element : salt_water_pipes ) {
        if( element->expansion == dir ) {
            pipe = element;
            pipe_is_new = false;
            break;
        }
    }

    if( pipe_is_new ) {
        debugmsg( "Trying to continue construction of a salt water pipe that isn't in progress" );
        return;
    }

    const int segment_number = salt_water_pipe_segment_of( making );

    if( segment_number == -1 ) {
        return;
    }

    if( pipe->segments[segment_number].finished ) {
        debugmsg( "Trying to construct a segment that's already constructed" );
        return;
    }

    common_salt_water_pipe_construction( miss_id, pipe, segment_number );
}

void basecamp::start_combat_mission( const mission_id &miss_id )
{
    popup( _( "Select checkpoints until you reach maximum range or select the last point again "
              "to end." ) );
    tripoint_abs_omt start = omt_pos;
    std::vector<tripoint_abs_omt> scout_points = om_companion_path( start, 90, true );
    if( scout_points.empty() ) {
        return;
    }
    int dist = scout_points.size();
    int trips = 2;
    time_duration travel_time = companion_travel_time_calc( scout_points, 0_minutes, trips );
    if( !query_yn( _( "Trip Estimate:\n%s" ), camp_trip_description( 0_hours, 0_hours,
                   travel_time, dist, trips, time_to_food( travel_time ) ) ) ) {
        return;
    }
    npc_ptr comp = start_mission( miss_id, travel_time, true, _( "departs on patrol…" ),
                                  false, {}, skill_survival, 3 );
    if( comp != nullptr ) {
        comp->companion_mission_points = scout_points;
    }
}

// the structure of this function has driven (at least) two devs insane
// recipe_deck returns a map of recipe ids to descriptions
// it first checks whether the mission id starts with the correct direction prefix,
// and then search for the mission id without direction prefix in the recipes
// if there's a match, the player has selected a crafting mission

void basecamp::start_crafting( const std::string &type, const mission_id &miss_id )
{
    const std::map<recipe_id, translation> &recipes = recipe_deck( type );
    const auto it = recipes.find( recipe_id( miss_id.parameters ) );
    if( it != recipes.end() ) {
        const recipe &making = it->first.obj();

        if( !making.deduped_requirements().can_make_with_inventory(
                _inv, making.get_component_filter() ) ) {
            popup( _( "You don't have the materials to craft that" ) );
            return;
        }

        int batch_size = 1;
        string_input_popup popup_input;
        int batch_max = recipe_batch_max( making );
        const std::string title = string_format( _( "Batch crafting %s [MAX: %d]: " ),
                                  making.result_name( /*decorated=*/true ), batch_max );
        popup_input.title( title ).edit( batch_size );

        if( popup_input.canceled() || batch_size <= 0 ) {
            return;
        }
        if( batch_size > recipe_batch_max( making ) ) {
            popup( _( "Your batch is too large!" ) );
            return;
        }

        basecamp_action_components components( making, batch_size, *this );
        if( !components.choose_components() ) {
            return;
        }

        time_duration work_days = base_camps::to_workdays( making.batch_duration( get_player_character(),
                                  batch_size ) );
        npc_ptr comp = start_mission( miss_id, work_days, true,
                                      _( "begins to work…" ), false, {},
                                      making.required_skills );
        if( comp != nullptr ) {
            components.consume_components();
            for( const item &results : making.create_results( batch_size ) ) {
                comp->companion_mission_inv.add_item( results );
                for( const item &byproducts : making.create_byproducts() ) {
                    comp->companion_mission_inv.add_item( byproducts );
                }
            }
        }
        return;
    }
}

static bool farm_valid_seed( const item &itm )
{
    return itm.is_seed() && itm.typeId() != itype_marloss_seed && itm.typeId() != itype_fungal_seeds;
}

static std::pair<size_t, std::string> farm_action( const tripoint_abs_omt &omt_tgt, farm_ops op,
        const npc_ptr &comp = nullptr )
{
    size_t plots_cnt = 0;
    std::string crops;

    const auto is_dirtmound = []( const tripoint & pos, tinymap & bay1, tinymap & bay2 ) {
        return ( bay1.ter( pos ) == t_dirtmound ) && ( !bay2.has_furn( pos ) );
    };
    const auto is_unplowed = []( const tripoint & pos, tinymap & farm_map ) {
        const ter_id &farm_ter = farm_map.ter( pos );
        return farm_ter->has_flag( ter_furn_flag::TFLAG_PLOWABLE );
    };

    std::set<std::string> plant_names;
    std::vector<item *> seed_inv;
    if( comp ) {
        seed_inv = comp->companion_mission_inv.items_with( farm_valid_seed );
    }

    // farm_map is what the area actually looks like
    tinymap farm_map;
    farm_map.load( project_to<coords::sm>( omt_tgt ), false );
    // farm_json is what the area should look like according to jsons (loaded on demand)
    std::unique_ptr<fake_map> farm_json;
    tripoint mapmin = tripoint( 0, 0, omt_tgt.z() );
    tripoint mapmax = tripoint( 2 * SEEX - 1, 2 * SEEY - 1, omt_tgt.z() );
    bool done_planting = false;
    Character &player_character = get_player_character();
    map &here = get_map();
    for( const tripoint &pos : farm_map.points_in_rectangle( mapmin, mapmax ) ) {
        if( done_planting ) {
            break;
        }
        switch( op ) {
            case farm_ops::plow: {
                if( !farm_json ) {
                    farm_json = std::make_unique<fake_map>();
                    mapgendata dat( omt_tgt, *farm_json, 0, calendar::turn, nullptr );
                    if( !run_mapgen_func( dat.terrain_type()->get_mapgen_id(), dat ) ) {
                        debugmsg( "Failed to run mapgen for farm map" );
                        break;
                    }
                }
                // Needs to be plowed to match json
                if( is_dirtmound( pos, *farm_json, farm_map ) && is_unplowed( pos, farm_map ) ) {
                    plots_cnt += 1;
                    if( comp ) {
                        farm_map.ter_set( pos, t_dirtmound );
                    }
                }
                break;
            }
            case farm_ops::plant:
                if( is_dirtmound( pos, farm_map, farm_map ) ) {
                    plots_cnt += 1;
                    if( comp ) {
                        if( seed_inv.empty() ) {
                            done_planting = true;
                            break;
                        }
                        item *tmp_seed = seed_inv.back();
                        seed_inv.pop_back();
                        std::list<item> used_seed;
                        if( tmp_seed->count_by_charges() ) {
                            used_seed.push_back( *tmp_seed );
                            tmp_seed->charges -= 1;
                            if( tmp_seed->charges > 0 ) {
                                seed_inv.push_back( tmp_seed );
                            }
                        }
                        used_seed.front().set_age( 0_turns );
                        farm_map.add_item_or_charges( pos, used_seed.front() );
                        farm_map.set( pos, t_dirt, f_plant_seed );
                    }
                }
                break;
            case farm_ops::harvest:
                if( farm_map.furn( pos ) == f_plant_harvest ) {
                    // Can't use item_stack::only_item() since there might be fertilizer
                    map_stack items = farm_map.i_at( pos );
                    const map_stack::iterator seed = std::find_if( items.begin(), items.end(), []( const item & it ) {
                        return it.is_seed();
                    } );
                    if( seed != items.end() && farm_valid_seed( *seed ) ) {
                        plots_cnt += 1;
                        if( comp ) {
                            int skillLevel = comp->get_skill_level( skill_survival );
                            ///\EFFECT_SURVIVAL increases number of plants harvested from a seed
                            int plant_cnt = rng( skillLevel / 2, skillLevel );
                            plant_cnt = std::min( std::max( plant_cnt, 1 ), 9 );
                            int seed_cnt = std::max( 1, rng( plant_cnt / 4, plant_cnt / 2 ) );
                            for( auto &i : iexamine::get_harvest_items( *seed->type, plant_cnt,
                                    seed_cnt, true ) ) {
                                here.add_item_or_charges( player_character.pos(), i );
                            }
                            farm_map.i_clear( pos );
                            farm_map.furn_set( pos, f_null );
                            farm_map.ter_set( pos, t_dirt );
                        } else {
                            plant_names.insert( item::nname( itype_id( seed->type->seed->fruit_id ) ) );
                        }
                    }
                }
                break;
            default:
                // let the callers handle no op argument
                break;
        }
    }
    if( comp ) {
        farm_map.save();
    }

    int total_c = 0;
    for( const std::string &i : plant_names ) {
        if( total_c < 5 ) {
            crops += "    " + i + "\n";
            total_c++;
        } else if( total_c == 5 ) {
            crops += _( "+ more\n" );
            break;
        }
    }

    return std::make_pair( plots_cnt, crops );
}

void basecamp::start_farm_op( const tripoint_abs_omt &omt_tgt, mission_id miss_id )
{
    farm_ops op = farm_ops::plow;
    if( miss_id.id == Camp_Plow ) {
        op = farm_ops::plow;
    } else if( miss_id.id == Camp_Plant ) {
        op = farm_ops::plant;
    } else if( miss_id.id == Camp_Harvest ) {
        op = farm_ops::harvest;
    } else {
        debugmsg( "Farm operations called with no matching operation" );
        return;
    }

    std::pair<size_t, std::string> farm_data = farm_action( omt_tgt, op );
    size_t plots_cnt = farm_data.first;
    if( !plots_cnt ) {
        return;
    }

    time_duration work = 0_minutes;
    switch( op ) {
        case farm_ops::harvest:
            work += 3_minutes * plots_cnt;
            start_mission( miss_id, work, true,
                           _( "begins to harvest the field…" ), false, {}, skill_survival, 1 );
            break;
        case farm_ops::plant: {
            std::vector<item *> seed_inv = _inv.items_with( farm_valid_seed );
            if( seed_inv.empty() ) {
                popup( _( "You have no additional seeds to give your companions…" ) );
                return;
            }
            std::vector<item *> plant_these = give_equipment( seed_inv,
                                              _( "Which seeds do you wish to have planted?" ) );
            size_t seed_cnt = 0;
            for( item *seeds : plant_these ) {
                seed_cnt += seeds->count();
            }
            size_t plots_seeded = std::min( seed_cnt, plots_cnt );
            if( !seed_cnt ) {
                return;
            }
            work += 1_minutes * plots_seeded;
            start_mission( miss_id, work, true,
                           _( "begins planting the field…" ), false, plant_these,
                           skill_survival, 1 );
            break;
        }
        case farm_ops::plow:
            work += 5_minutes * plots_cnt;
            start_mission( miss_id, work, true,
                           _( "begins plowing the field…" ), false, {} );
            break;
        default:
            debugmsg( "Farm operations called with no operation" );
    }
}

// camp faction companion mission recovery functions
npc_ptr basecamp::companion_choose_return( const mission_id &miss_id,
        time_duration min_duration )
{
    return talk_function::companion_choose_return( omt_pos, base_camps::id, miss_id,
            calendar::turn - min_duration );
}

npc_ptr basecamp::companion_crafting_choose_return( const mission_id &miss_id )
{
    comp_list preliminary_npc_list = get_mission_workers( miss_id, true );
    comp_list npc_list;
    std::map<std::string, comp_list> lists = companion_per_recipe_building_type( preliminary_npc_list );
    const std::string bldg = recipe_group::get_building_of_recipe( miss_id.parameters );

    for( const npc_ptr &comp : lists[bldg] ) {
        if( comp->companion_mission_time_ret < calendar::turn ) {
            npc_list.emplace_back( comp );
        }
    }

    return talk_function::companion_choose_return( npc_list );
}


void basecamp::finish_return( npc &comp, const bool fixed_time, const std::string &return_msg,
                              const std::string &skill, int difficulty, const bool cancel )
{
    popup( "%s %s", comp.get_name(), return_msg );
    // this is the time the mission was expected to take, or did take for fixed time missions
    time_duration reserve_time = comp.companion_mission_time_ret - comp.companion_mission_time;
    time_duration mission_time = reserve_time;
    if( !fixed_time ) {
        mission_time = calendar::turn - comp.companion_mission_time;
    }
    if( !cancel ) {
        talk_function::companion_skill_trainer( comp, skill, mission_time, difficulty );
    }

    // companions subtracted food when they started the mission, but didn't mod their hunger for
    // that food.  so add it back in.
    int need_food = time_to_food( mission_time - reserve_time );
    if( camp_food_supply() < need_food ) {
        popup( _( "Your companion seems disappointed that your pantry is empty…" ) );
    }
    int avail_food = std::min( need_food, camp_food_supply() ) + time_to_food( reserve_time );
    // movng all the logic from talk_function::companion return here instead of polluting
    // mission_companion
    comp.reset_companion_mission();
    comp.companion_mission_time = calendar::before_time_starts;
    comp.companion_mission_time_ret = calendar::before_time_starts;
    if( !cancel ) {
        for( size_t i = 0; i < comp.companion_mission_inv.size(); i++ ) {
            for( const auto &it : comp.companion_mission_inv.const_stack( i ) ) {
                if( !it.count_by_charges() || it.charges > 0 ) {
                    place_results( it );
                }
            }
        }
    }
    comp.companion_mission_inv.clear();
    comp.companion_mission_points.clear();
    // npc *may* be active, or not if outside the reality bubble
    g->reload_npcs();
    validate_assignees();

    camp_food_supply( -need_food );
    comp.mod_hunger( -avail_food );
    comp.mod_stored_kcal( avail_food );
    if( has_water() ) {
        comp.set_thirst( 0 );
    }
    comp.set_fatigue( 0 );
    comp.set_sleep_deprivation( 0 );
}

npc_ptr basecamp::mission_return( const mission_id &miss_id, time_duration min_duration,
                                  bool fixed_time, const std::string &return_msg,
                                  const std::string &skill, int difficulty )
{
    npc_ptr comp = companion_choose_return( miss_id, min_duration );
    if( comp != nullptr ) {
        finish_return( *comp, fixed_time, return_msg, skill, difficulty );
    }
    return comp;
}

npc_ptr basecamp::crafting_mission_return( const mission_id &miss_id, const std::string &return_msg,
        const std::string &skill, int difficulty )
{
    npc_ptr comp = companion_crafting_choose_return( miss_id );
    if( comp != nullptr ) {
        finish_return( *comp, false, return_msg, skill, difficulty );
    }
    return comp;
}


npc_ptr basecamp::emergency_recall( const mission_id miss_id )
{
    npc_ptr comp = talk_function::companion_choose_return( omt_pos, base_camps::id, miss_id,
                   calendar::turn - 24_hours, false );
    if( comp != nullptr ) {
        const std::string return_msg = _( "responds to the emergency recall…" );
        finish_return( *comp, false, return_msg, skill_menial.str(), 0, true );
    }
    return comp;

}

bool basecamp::upgrade_return( const mission_id &miss_id )
{
    const point dir = miss_id.dir.value();  //  Will always have a value
    const std::string bldg = miss_id.parameters.empty() ? next_upgrade( dir, 1 ) : miss_id.parameters;
    if( bldg == "null" ) {
        return false;
    }

    auto e = expansions.find( dir );
    if( e == expansions.end() ) {
        return false;
    }
    const tripoint_abs_omt upos = e->second.pos;
    const recipe &making = recipe_id( bldg ).obj();

    time_duration work_days = base_camps::to_workdays( making.batch_duration(
                                  get_player_character() ) );
    npc_ptr comp = companion_choose_return( miss_id, work_days );

    if( comp == nullptr ) {
        return false;
    }

    bool mirror_horizontal;
    bool mirror_vertical;
    int rotation;

    if( !extract_and_check_orientation_flags( making.ident(),
            dir,
            mirror_horizontal,
            mirror_vertical,
            rotation,
            "%s failed to build the %s upgrade",
            comp->disp_name() ) ) {
        return false;
    }

    if( making.get_blueprint().str() == faction_expansion_salt_water_pipe_swamp_N ) {
        return salt_water_pipe_swamp_return( miss_id, work_days );
    } else if( making.get_blueprint().str() == faction_expansion_salt_water_pipe_N ) {
        return salt_water_pipe_return( miss_id, work_days );
    }

    if( !run_mapgen_update_func( making.get_blueprint(), upos, nullptr, true, mirror_horizontal,
                                 mirror_vertical, rotation ) ) {
        popup( _( "%s failed to build the %s upgrade, perhaps there is a vehicle in the way." ),
               comp->disp_name(),
               making.get_blueprint().str() );
        return false;
    }
    update_provides( bldg, e->second );
    update_resources( bldg );

    const std::string msg = _( "returns from upgrading the camp having earned a bit of "
                               "experience…" );
    finish_return( *comp, false, msg, "construction", making.difficulty );

    return true;
}

bool basecamp::menial_return( const mission_id miss_id )
{
    const std::string msg = _( "returns from doing the dirty work to keep the camp running…" );
    npc_ptr comp = mission_return( miss_id,
                                   3_hours, true, msg, skill_menial.str(), 2 );
    if( comp == nullptr ) {
        return false;
    }
    comp->revert_after_activity();
    return true;
}

bool basecamp::gathering_return( const mission_id &miss_id, time_duration min_time )
{
    npc_ptr comp = companion_choose_return( miss_id, min_time );
    if( comp == nullptr ) {
        return false;
    }

    std::string task_description = _( "gathering materials" );
    int danger = 20;
    int favor = 2;
    int threat = 10;
    std::string skill_group = "gathering";
    int skill = 2 * comp->get_skill_level( skill_survival ) + comp->per_cur;
    int checks_per_cycle = 6;
    if( miss_id.id == Camp_Foraging ) {
        task_description = _( "foraging for edible plants" );
        danger = 15;
        checks_per_cycle = 12;
    } else if( miss_id.id == Camp_Trapping ) {
        task_description = _( "trapping small animals" );
        favor = 1;
        danger = 15;
        skill_group = "trapping";
        skill = 2 * comp->get_skill_level( skill_traps ) + comp->per_cur;
        checks_per_cycle = 4;
    } else if( miss_id.id == Camp_Hunting ) {
        task_description = _( "hunting for meat" );
        danger = 10;
        favor = 0;
        skill_group = "hunting";
        skill = 1.5 * comp->get_skill_level( skill_gun ) + comp->per_cur / 2.0;
        threat = 12;
        checks_per_cycle = 2;
    }

    time_duration mission_time = calendar::turn - comp->companion_mission_time;
    if( one_in( danger ) && !survive_random_encounter( *comp, task_description, favor, threat ) ) {
        return false;
    }
    const std::string msg = string_format( _( "returns from %s carrying supplies and has a bit "
                                           "more experience…" ), task_description );
    finish_return( *comp, false, msg, skill_group, 1 );

    item_group_id itemlist( "forest" );
    if( miss_id.id == Camp_Collect_Firewood ) {
        itemlist = Item_spawn_data_gathering_faction_base_camp_firewood;
    } else if( miss_id.id == Camp_Gather_Materials ) {
        itemlist = get_gatherlist();
    } else if( miss_id.id == Camp_Foraging ) {
        switch( season_of_year( calendar::turn ) ) {
            case SPRING:
                itemlist = Item_spawn_data_foraging_faction_camp_spring;
                break;
            case SUMMER:
                itemlist = Item_spawn_data_foraging_faction_camp_summer;
                break;
            case AUTUMN:
                itemlist = Item_spawn_data_foraging_faction_camp_autumn;
                break;
            case WINTER:
                itemlist = Item_spawn_data_foraging_faction_camp_winter;
                break;
            default:
                debugmsg( "Invalid season" );
        }
    }
    if( miss_id.id == Camp_Trapping ||
        miss_id.id == Camp_Hunting ) {
        hunting_results( skill, miss_id, checks_per_cycle * mission_time / min_time, 30 );
    } else {
        search_results( skill, itemlist, checks_per_cycle * mission_time / min_time, 15 );
    }

    return true;
}

void basecamp::fortifications_return( const mission_id miss_id )
{
    npc_ptr comp = companion_choose_return( miss_id, 3_hours );
    if( comp != nullptr ) {
        update_mapgen_id build_n{ faction_wall_level_n_0_string };
        update_mapgen_id build_e{ "faction_wall_level_E_0" };
        update_mapgen_id build_s{ "faction_wall_level_S_0" };
        update_mapgen_id build_w{ "faction_wall_level_W_0" };
        if( miss_id.parameters == faction_wall_level_n_1_string ||
            //  Handling of old format (changed mid 0.F) below
            ( miss_id.parameters.empty() &&
              comp->companion_mission_role_id == faction_wall_level_n_1_string ) ) {
            build_n = update_mapgen_faction_wall_level_N_1;
            build_e = update_mapgen_faction_wall_level_E_1;
            build_s = update_mapgen_faction_wall_level_S_1;
            build_w = update_mapgen_faction_wall_level_W_1;
        }
        update_mapgen_id build_first = build_e;
        update_mapgen_id build_second = build_w;
        bool build_dir_NS = comp->companion_mission_points[0].y() !=
                            comp->companion_mission_points[1].y();
        if( build_dir_NS ) {
            build_first = build_s;
            build_second = build_n;
        }
        //Add fences
        auto build_point = comp->companion_mission_points;
        for( size_t pt = 0; pt < build_point.size(); pt++ ) {
            //First point is always at top or west since they are built in a line and sorted
            if( pt == 0 ) {
                run_mapgen_update_func( build_first, build_point[pt] );
            } else if( pt == build_point.size() - 1 ) {
                run_mapgen_update_func( build_second, build_point[pt] );
            } else {
                run_mapgen_update_func( build_first, build_point[pt] );
                run_mapgen_update_func( build_second, build_point[pt] );
            }
            if( miss_id.parameters == faction_wall_level_n_0_string ||
                //  Handling of old format (changed mid 0.F) below
                ( miss_id.parameters.empty() &&
                  comp->companion_mission_role_id == faction_wall_level_n_0_string ) ) {
                tripoint_abs_omt fort_point = build_point[pt];
                fortifications.push_back( fort_point );
            }
        }
        const std::string msg = _( "returns from constructing fortifications…" );
        finish_return( *comp, true, msg, skill_construction.str(), 2 );
    }
}

static void salt_water_pipe_orientation_adjustment( const point dir, bool &orthogonal,
        bool &mirror_vertical, bool &mirror_horizontal, int &rotation );

void salt_water_pipe_orientation_adjustment( const point dir, bool &orthogonal,
        bool &mirror_vertical, bool &mirror_horizontal, int &rotation )
{
    orthogonal = true;
    mirror_horizontal = false;
    mirror_vertical = false;
    rotation = 0;

    switch( base_camps::all_directions.at( dir ).tab_order ) {
        case base_camps::tab_mode::TAB_MAIN: //  Should not happen. We would have had to define the same point twice.
        case base_camps::tab_mode::TAB_N:    //  This is the reference direction for orthogonal orientations.
            break;

        case base_camps::tab_mode::TAB_NE:
            orthogonal = false;
            break;  //  This is the reference direction for diagonal orientations.

        case base_camps::tab_mode::TAB_E:
            rotation = 1;
            break;

        case base_camps::tab_mode::TAB_SE:
            orthogonal = false;
            rotation = 1;
            break;

        case base_camps::tab_mode::TAB_S:
            mirror_vertical = true;
            break;

        case base_camps::tab_mode::TAB_SW:
            orthogonal = false;
            rotation = 2;
            break;

        case base_camps::tab_mode::TAB_W:
            rotation = 1;
            mirror_vertical = true;
            break;

        case base_camps::tab_mode::TAB_NW:
            orthogonal = false;
            rotation = 3;
            break;
    }
}

bool basecamp::salt_water_pipe_swamp_return( const mission_id &miss_id,
        const time_duration work_days )
{
    const point dir = miss_id.dir.value();  //  Will always have a value
    npc_ptr comp = companion_choose_return( miss_id, work_days );

    if( comp == nullptr ) {
        return false;
    }

    expansion_salt_water_pipe *pipe;

    bool found = false;
    for( expansion_salt_water_pipe *element : salt_water_pipes ) {
        if( element->expansion == dir ) {
            pipe = element;
            found = true;
            break;
        }
    }

    if( !found ) {
        popup( _( "Error: Failed to find the pipe task that was to be constructed" ) );
        return false;
    }

    point connection_dir = pipe->connection_direction;
    const int segment_number = 0;

    point next_construction_direction;

    if( segment_number == pipe->segments.size() - 1 ) {
        next_construction_direction = { -connection_dir.x, -connection_dir.y };
    } else {
        next_construction_direction = { pipe->segments[segment_number + 1].point.x() - pipe->segments[segment_number].point.x(),
                                        pipe->segments[segment_number + 1].point.y() - pipe->segments[segment_number].point.y()
                                      };
    }

    bool orthogonal = true;
    bool mirror_horizontal = false;
    bool mirror_vertical = false;
    int rotation = 0;

    salt_water_pipe_orientation_adjustment( next_construction_direction, orthogonal, mirror_vertical,
                                            mirror_horizontal, rotation );

    if( orthogonal ) {
        const update_mapgen_id id{ faction_expansion_salt_water_pipe_swamp_N };
        run_mapgen_update_func( id, pipe->segments[segment_number].point, nullptr, true, mirror_horizontal,
                                mirror_vertical, rotation );
    } else {
        const update_mapgen_id id{ faction_expansion_salt_water_pipe_swamp_NE };
        run_mapgen_update_func( id, pipe->segments[segment_number].point, nullptr, true, mirror_horizontal,
                                mirror_vertical, rotation );
    }

    pipe->segments[segment_number].finished = true;

    auto e = expansions.find( dir );
    //  Should be safe as the caller has already checked the result. Repeating rather than adding an additional parameter to the function.

    size_t finished_segments = 0;
    for( std::vector<expansion_salt_water_pipe_segment>::iterator::value_type element :
         pipe->segments ) {
        if( element.finished ) {
            finished_segments++;
        }
    }

    //  This is the last segment, so we can now allow the salt water pump to be constructed.
    if( finished_segments == pipe->segments.size() ) {
        const std::string token = salt_water_pipe_string_base + "0" + salt_water_pipe_string_suffix;
        if( e->second.provides.find( token ) == e->second.provides.end() ) {
            e->second.provides[token] = 0;
        }
        e->second.provides[token]++;
    }

    update_provides( miss_id.parameters, e->second );
    update_resources( miss_id.parameters );

    finish_return( *comp, true,
                   _( "returns from construction of the salt water pipe swamp segment…" ), "construction", 2 );

    return true;
}

bool basecamp::salt_water_pipe_return( const mission_id &miss_id,
                                       const time_duration work_days )
{
    const recipe &making = recipe_id( miss_id.parameters ).obj();
    const point dir = miss_id.dir.value();  //  Will always have a value
    npc_ptr comp = companion_choose_return( miss_id, work_days );

    if( comp == nullptr ) {
        return false;
    }

    expansion_salt_water_pipe *pipe;

    bool found = false;
    for( expansion_salt_water_pipe *element : salt_water_pipes ) {
        if( element->expansion == dir ) {
            pipe = element;
            found = true;
            break;
        }
    }

    if( !found ) {
        popup( _( "Error: Failed to find the pipe task that was to be constructed" ) );
        return false;
    }

    point connection_dir = pipe->connection_direction;
    const int segment_number = salt_water_pipe_segment_of( making );

    if( segment_number == -1 ) {
        return false;
    }

    const point previous_construction_direction = { pipe->segments[segment_number - 1].point.x() - pipe->segments[segment_number].point.x(),
                                                    pipe->segments[segment_number - 1].point.y() - pipe->segments[segment_number].point.y()
                                                  };

    point next_construction_direction;

    if( segment_number == static_cast<int>( pipe->segments.size() - 1 ) ) {
        next_construction_direction = { -connection_dir.x, -connection_dir.y };
    } else {
        next_construction_direction = { pipe->segments[segment_number + 1].point.x() - pipe->segments[segment_number].point.x(),
                                        pipe->segments[segment_number + 1].point.y() - pipe->segments[segment_number].point.y()
                                      };
    }

    bool orthogonal = true;
    bool mirror_horizontal = false;
    bool mirror_vertical = false;
    int rotation = 0;

    salt_water_pipe_orientation_adjustment( previous_construction_direction, orthogonal,
                                            mirror_vertical, mirror_horizontal, rotation );

    if( orthogonal ) {
        const update_mapgen_id id{ faction_expansion_salt_water_pipe_N };
        run_mapgen_update_func( id, pipe->segments[segment_number].point, nullptr, true, mirror_horizontal,
                                mirror_vertical, rotation );
    } else {
        const update_mapgen_id id{ faction_expansion_salt_water_pipe_NE };
        run_mapgen_update_func( id, pipe->segments[segment_number].point, nullptr, true, mirror_horizontal,
                                mirror_vertical, rotation );
    }

    salt_water_pipe_orientation_adjustment( next_construction_direction, orthogonal, mirror_vertical,
                                            mirror_horizontal, rotation );

    if( orthogonal ) {
        const update_mapgen_id id{ faction_expansion_salt_water_pipe_N };
        run_mapgen_update_func( id, pipe->segments[segment_number].point, nullptr, true, mirror_horizontal,
                                mirror_vertical, rotation );
    } else {
        const update_mapgen_id id{ faction_expansion_salt_water_pipe_NE };
        run_mapgen_update_func( id, pipe->segments[segment_number].point, nullptr, true, mirror_horizontal,
                                mirror_vertical, rotation );
    }

    pipe->segments[segment_number].finished = true;

    auto e = expansions.find( dir );
    //  Should be safe as the caller has already checked the result. Repeating rather than adding an additional parameter to the function.

    size_t finished_segments = 0;
    for( std::vector<expansion_salt_water_pipe_segment>::iterator::value_type element :
         pipe->segments ) {
        if( element.finished ) {
            finished_segments++;
        }
    }

    //  This is the last segment, so we can now allow the salt water pump to be constructed.
    if( finished_segments == pipe->segments.size() ) {
        const std::string token = salt_water_pipe_string_base + "0" + salt_water_pipe_string_suffix;
        if( e->second.provides.find( token ) == e->second.provides.end() ) {
            e->second.provides[token] = 0;
        }
        e->second.provides[token]++;
    }

    update_provides( miss_id.parameters, e->second );
    update_resources( miss_id.parameters );

    finish_return( *comp, true, _( "returns from construction of a salt water pipe segment…" ),
                   "construction", 2 );

    return true;
}

void basecamp::recruit_return( const mission_id &miss_id, int score )
{
    const std::string msg = _( "returns from searching for recruits with "
                               "a bit more experience…" );
    npc_ptr comp = mission_return( miss_id, 4_days, true, msg, skill_recruiting.str(), 2 );
    if( comp == nullptr ) {
        return;
    }

    npc_ptr recruit;
    //Success of finding an NPC to recruit, based on survival/tracking
    int skill = comp->get_skill_level( skill_survival );
    if( rng( 1, 20 ) + skill > 17 ) {
        recruit = make_shared_fast<npc>();
        recruit->normalize();
        recruit->randomize();
        popup( _( "%s encountered %s…" ), comp->get_name(), recruit->get_name() );
    } else {
        popup( _( "%s didn't find anyone to recruit…" ), comp->get_name() );
        return;
    }
    //Chance of convincing them to come back
    skill = ( 100 * comp->get_skill_level( skill_speech ) + score ) / 100;
    if( rng( 1, 20 ) + skill  > 19 ) {
        popup( _( "%s convinced %s to hear a recruitment offer from you…" ), comp->get_name(),
               recruit->get_name() );
    } else {
        popup( _( "%s wasn't interested in anything %s had to offer…" ), recruit->get_name(),
               comp->get_name() );
        return;
    }
    //Stat window
    int rec_m = 0;
    int appeal = rng( -5, 3 ) + std::min( skill / 3, 3 );
    int food_desire = rng( 0, 5 );
    while( true ) {
        std::string description = _( "NPC Overview:\n\n" );
        description += string_format( _( "Name:  %s\n\n" ), right_justify( recruit->get_name(), 20 ) );
        description += string_format( _( "Strength:        %10d\n" ), recruit->str_max );
        description += string_format( _( "Dexterity:       %10d\n" ), recruit->dex_max );
        description += string_format( _( "Intelligence:    %10d\n" ), recruit->int_max );
        description += string_format( _( "Perception:      %10d\n\n" ), recruit->per_max );
        description += _( "Top 3 Skills:\n" );

        const auto skillslist = Skill::get_skills_sorted_by(
        [&]( const Skill & a, const Skill & b ) {
            const int level_a = recruit->get_skill_level( a.ident() );
            const int level_b = recruit->get_skill_level( b.ident() );
            return localized_compare( std::make_pair( -level_a, a.name() ),
                                      std::make_pair( -level_b, b.name() ) );
        } );

        description += string_format( "%s:          %4d\n", right_justify( skillslist[0]->name(), 12 ),
                                      recruit->get_skill_level( skillslist[0]->ident() ) );
        description += string_format( "%s:          %4d\n", right_justify( skillslist[1]->name(), 12 ),
                                      recruit->get_skill_level( skillslist[1]->ident() ) );
        description += string_format( "%s:          %4d\n\n", right_justify( skillslist[2]->name(), 12 ),
                                      recruit->get_skill_level( skillslist[2]->ident() ) );

        description += _( "Asking for:\n" );
        description += string_format( _( "> Food:     %10d days\n\n" ), food_desire );
        description += string_format( _( "Faction Food:%9d days\n\n" ),
                                      camp_food_supply( 0, true ) );
        description += string_format( _( "Recruit Chance: %10d%%\n\n" ),
                                      std::min( 100 * ( 10 + appeal ) / 20, 100 ) );
        description += _( "Select an option:" );

        std::vector<std::string> rec_options;
        rec_options.emplace_back( _( "Increase Food" ) );
        rec_options.emplace_back( _( "Decrease Food" ) );
        rec_options.emplace_back( _( "Make Offer" ) );
        rec_options.emplace_back( _( "Not Interested" ) );

        rec_m = uilist( description, rec_options );
        if( rec_m < 0 || rec_m == 3 || static_cast<size_t>( rec_m ) >= rec_options.size() ) {
            popup( _( "You decide you aren't interested…" ) );
            return;
        }

        if( rec_m == 0 && food_desire + 1 <= camp_food_supply( 0, true ) ) {
            food_desire++;
            appeal++;
        }
        if( rec_m == 1 ) {
            if( food_desire > 0 ) {
                food_desire--;
                appeal--;
            }
        }
        if( rec_m == 2 ) {
            break;
        }
    }
    // Roll for recruitment
    if( rng( 1, 20 ) + appeal >= 10 ) {
        popup( _( "%s has been convinced to join!" ), recruit->get_name() );
    } else {
        popup( _( "%s wasn't interested…" ), recruit->get_name() );
        // nullptr;
        return;
    }
    // Time durations always subtract from camp food supply
    camp_food_supply( 1_days * food_desire );
    avatar &player_character = get_avatar();
    recruit->spawn_at_precise( player_character.get_location() + point( -4, -4 ) );
    overmap_buffer.insert_npc( recruit );
    recruit->form_opinion( player_character );
    recruit->mission = NPC_MISSION_NULL;
    recruit->add_new_mission( mission::reserve_random( ORIGIN_ANY_NPC,
                              recruit->global_omt_location(),
                              recruit->getID() ) );
    talk_function::follow( *recruit );
    g->load_npcs();
}

void basecamp::combat_mission_return( const mission_id &miss_id )
{
    npc_ptr comp = companion_choose_return( miss_id, 3_hours );
    if( comp != nullptr ) {
        bool patrolling = miss_id.id == Camp_Combat_Patrol;
        comp_list patrol;
        npc_ptr guy = overmap_buffer.find_npc( comp->getID() );
        if( guy ) {
            patrol.push_back( guy );
        }
        for( auto pt : comp->companion_mission_points ) {
            const oter_id &omt_ref = overmap_buffer.ter( pt );
            int swim = comp->get_skill_level( skill_swimming );
            if( is_river( omt_ref ) && swim < 2 ) {
                if( swim == 0 ) {
                    popup( _( "Your companion hit a river and didn't know how to swim…" ) );
                } else {
                    popup( _( "Your companion hit a river and didn't know how to swim well "
                              "enough to cross…" ) );
                }
                break;
            }
            comp->death_drops = false;
            bool outcome = talk_function::companion_om_combat_check( patrol, pt, patrolling );
            comp->death_drops = true;
            if( outcome ) {
                overmap_buffer.reveal( pt, 2 );
            } else if( comp->is_dead() ) {
                popup( _( "%s didn't return from patrol…" ), comp->get_name() );
                comp->place_corpse( pt );
                overmap_buffer.add_note( pt, "DEAD NPC" );
                overmap_buffer.remove_npc( comp->getID() );
                return;
            }
        }
        const std::string msg = _( "returns from patrol…" );
        finish_return( *comp, true, msg, skill_combat.str(), 4 );
    }
}

bool basecamp::survey_return( const mission_id miss_id )
{
    npc_ptr comp = companion_choose_return( miss_id, 3_hours );
    if( comp == nullptr ) {
        return false;
    }

    popup( _( "Select a tile up to %d tiles away." ), 1 );
    const tripoint_abs_omt where( ui::omap::choose_point() );
    if( where == overmap::invalid_tripoint ) {
        return false;
    }

    int dist = rl_dist( where.xy(), omt_pos.xy() );
    if( dist != 1 ) {
        popup( _( "You must select a tile within %d range of the camp" ), 1 );
        return false;
    }
    if( omt_pos.z() != where.z() ) {
        popup( _( "Expansions must be on the same level as the camp" ) );
        return false;
    }
    const point dir = talk_function::om_simple_dir( omt_pos, where );
    if( expansions.find( dir ) != expansions.end() ) {
        popup( _( "You already have an expansion at that location" ) );
        return false;
    }

    const oter_id &omt_ref = overmap_buffer.ter( where );
    const auto &pos_expansions = recipe_group::get_recipes_by_id( "all_faction_base_expansions",
                                 omt_ref.id().c_str() );
    if( pos_expansions.empty() ) {
        popup( _( "You can't build any expansions in a %s." ), omt_ref.id().c_str() );
        return false;
    }

    const recipe_id expansion_type = base_camps::select_camp_option( pos_expansions,
                                     _( "Select an expansion:" ) );

    bool mirror_horizontal;
    bool mirror_vertical;
    int rotation;

    if( !extract_and_check_orientation_flags( expansion_type,
            dir,
            mirror_horizontal,
            mirror_vertical,
            rotation,
            "%s failed to build the %s expansion",
            comp->disp_name() ) ) {
        return false;
    }

    if( !run_mapgen_update_func( update_mapgen_id( expansion_type.str() ), where, nullptr, true,
                                 mirror_horizontal, mirror_vertical, rotation ) ) {
        popup( _( "%s failed to add the %s expansion, perhaps there is a vehicle in the way." ),
               comp->disp_name(),
               expansion_type->blueprint_name() );
        return false;
    }
    overmap_buffer.ter_set( where, oter_id( expansion_type.str() ) );
    add_expansion( expansion_type.str(), where, dir );
    const std::string msg = _( "returns from surveying for the expansion." );
    finish_return( *comp, true, msg, skill_construction.str(), 2 );
    return true;
}

bool basecamp::farm_return( const mission_id &miss_id, const tripoint_abs_omt &omt_tgt )
{
    farm_ops op;
    if( miss_id.id == Camp_Plow ) {
        op = farm_ops::plow;
    } else if( miss_id.id == Camp_Plant ) {
        op = farm_ops::plant;
    } else if( miss_id.id == Camp_Harvest ) {
        op = farm_ops::harvest;
    } else {
        debugmsg( "Farm operations called with no matching operation" );
        return false;
    }

    const std::string msg = _( "returns from working your fields…" );
    npc_ptr comp = companion_choose_return( miss_id, 15_minutes );
    if( comp == nullptr ) {
        return false;
    }

    farm_action( omt_tgt, op, comp );

    Character &player_character = get_player_character();
    //Give any seeds the NPC didn't use back to you.
    for( size_t i = 0; i < comp->companion_mission_inv.size(); i++ ) {
        for( const auto &it : comp->companion_mission_inv.const_stack( i ) ) {
            if( it.charges > 0 ) {
                player_character.i_add( it );
            }
        }
    }
    finish_return( *comp, true, msg, skill_survival.str(), 2 );
    return true;
}

// window manipulation
void talk_function::draw_camp_tabs( const catacurses::window &win,
                                    const base_camps::tab_mode cur_tab,
                                    const std::vector<std::vector<mission_entry>> &entries )
{
    werase( win );
    const int width = getmaxx( win );
    mvwhline( win, point( 0, 2 ), LINE_OXOX, width );

    std::vector<std::string> tabs( base_camps::all_directions.size() );
    for( const auto &direction : base_camps::all_directions ) {
        tabs.at( direction.second.tab_order ) = direction.second.tab_title.translated();
    }
    const int tab_step = 3;
    int tab_space = 1;
    int tab_x = 0;
    for( auto &t : tabs ) {
        bool tab_empty = entries[tab_x + 1].empty();
        draw_subtab( win, tab_space, t, tab_x == cur_tab, false, tab_empty );
        tab_space += tab_step + utf8_width( t );
        tab_x++;
    }
    wnoutrefresh( win );
}

std::string talk_function::name_mission_tabs(
    const tripoint_abs_omt &omt_pos, const std::string &role_id,
    const std::string &cur_title, base_camps::tab_mode cur_tab )
{
    if( role_id != base_camps::id ) {
        return cur_title;
    }
    cata::optional<basecamp *> temp_camp = overmap_buffer.find_camp( omt_pos.xy() );
    if( !temp_camp ) {
        return cur_title;
    }
    basecamp *bcp = *temp_camp;
    for( const auto &direction : base_camps::all_directions ) {
        if( cur_tab == direction.second.tab_order ) {
            return bcp->expansion_tab( direction.first );
        }
    }
    return bcp->expansion_tab( base_camps::base_dir );
}

// recipes and craft support functions
int basecamp::recipe_batch_max( const recipe &making ) const
{
    int max_batch = 0;
    const int max_checks = 9;
    for( size_t batch_size = 1000; batch_size > 0; batch_size /= 10 ) {
        for( int iter = 0; iter < max_checks; iter++ ) {
            time_duration work_days = base_camps::to_workdays( making.batch_duration(
                                          get_player_character(), max_batch + batch_size ) );
            int food_req = time_to_food( work_days );
            bool can_make = making.deduped_requirements().can_make_with_inventory(
                                _inv, making.get_component_filter(), max_batch + batch_size );
            if( can_make && camp_food_supply() > food_req ) {
                max_batch += batch_size;
            } else {
                break;
            }
        }
    }
    return max_batch;
}

void basecamp::search_results( int skill, const item_group_id &group_id, int attempts,
                               int difficulty )
{
    for( int i = 0; i < attempts; i++ ) {
        if( skill > rng( 0, difficulty ) ) {
            item result = item_group::item_from( group_id, calendar::turn );
            if( !result.is_null() ) {
                place_results( result );
            }
        }
    }
}

void basecamp::hunting_results( int skill, const mission_id &miss_id, int attempts, int difficulty )
{
    // no item groups for corpses, so we'll have to improvise
    weighted_int_list<mtype_id> hunting_targets;
    hunting_targets.add( mon_beaver, 10 );
    hunting_targets.add( mon_fox_red, 10 );
    hunting_targets.add( mon_fox_gray, 10 );
    hunting_targets.add( mon_mink, 5 );
    hunting_targets.add( mon_muskrat, 10 );
    hunting_targets.add( mon_otter, 10 );
    hunting_targets.add( mon_duck, 10 );
    hunting_targets.add( mon_cockatrice, 1 );
    if( miss_id.id == Camp_Trapping ) {
        hunting_targets.add( mon_black_rat, 40 );
        hunting_targets.add( mon_chipmunk, 30 );
        hunting_targets.add( mon_groundhog, 30 );
        hunting_targets.add( mon_hare, 20 );
        hunting_targets.add( mon_lemming, 40 );
        hunting_targets.add( mon_opossum, 10 );
        hunting_targets.add( mon_rabbit, 20 );
        hunting_targets.add( mon_squirrel, 20 );
        hunting_targets.add( mon_weasel, 20 );
        hunting_targets.add( mon_chicken, 10 );
        hunting_targets.add( mon_grouse, 10 );
        hunting_targets.add( mon_pheasant, 10 );
        hunting_targets.add( mon_turkey, 20 );
    } else if( miss_id.id == Camp_Hunting ) {
        hunting_targets.add( mon_chicken, 20 );
        // good luck hunting upland game birds without dogs
        hunting_targets.add( mon_grouse, 2 );
        hunting_targets.add( mon_pheasant, 2 );
        hunting_targets.add( mon_turkey, 10 );
        hunting_targets.add( mon_bear, 1 );
        hunting_targets.add( mon_cougar, 5 );
        hunting_targets.add( mon_cow, 1 );
        hunting_targets.add( mon_coyote, 15 );
        hunting_targets.add( mon_deer, 2 );
        hunting_targets.add( mon_moose, 1 );
        hunting_targets.add( mon_pig, 1 );
        hunting_targets.add( mon_wolf, 10 );
    }
    for( int i = 0; i < attempts; i++ ) {
        if( skill > rng( 0, difficulty ) ) {
            const mtype_id *target = hunting_targets.pick();
            item result = item::make_corpse( *target, calendar::turn, "" );
            if( !result.is_null() ) {
                place_results( result );
            }
        }
    }
}

int om_harvest_ter_est( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t, int chance )
{
    return om_harvest_ter( comp, omt_tgt, t, chance, true, false );
}
int om_harvest_ter_break( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t, int chance )
{
    return om_harvest_ter( comp, omt_tgt, t, chance, false, false );
}
int om_harvest_ter( npc &comp, const tripoint_abs_omt &omt_tgt, const ter_id &t, int chance,
                    bool estimate, bool bring_back )
{
    const ter_t &ter_tgt = t.obj();
    tinymap target_bay;
    target_bay.load( project_to<coords::sm>( omt_tgt ), false );
    int harvested = 0;
    int total = 0;
    tripoint mapmin = tripoint( 0, 0, omt_tgt.z() );
    tripoint mapmax = tripoint( 2 * SEEX - 1, 2 * SEEY - 1, omt_tgt.z() );
    for( const tripoint &p : target_bay.points_in_rectangle( mapmin, mapmax ) ) {
        if( target_bay.ter( p ) == t && x_in_y( chance, 100 ) ) {
            total++;
            if( estimate ) {
                continue;
            }
            if( bring_back ) {
                for( const item &itm : item_group::items_from( ter_tgt.bash.drop_group,
                        calendar::turn ) ) {
                    comp.companion_mission_inv.push_back( itm );
                }
                harvested++;
                target_bay.ter_set( p, ter_tgt.bash.ter_set );
            }
        }
    }
    target_bay.save();
    if( bring_back ) {
        return harvested;
    }
    return total;
}

int om_cutdown_trees_est( const tripoint_abs_omt &omt_tgt, int chance )
{
    return om_cutdown_trees( omt_tgt, chance, true, false );
}
int om_cutdown_trees_logs( const tripoint_abs_omt &omt_tgt, int chance )
{
    return om_cutdown_trees( omt_tgt, chance, false, true );
}
int om_cutdown_trees_trunks( const tripoint_abs_omt &omt_tgt, int chance )
{
    return om_cutdown_trees( omt_tgt, chance, false, false );
}
int om_cutdown_trees( const tripoint_abs_omt &omt_tgt, int chance, bool estimate,
                      bool force_cut_trunk )
{
    tinymap target_bay;
    target_bay.load( project_to<coords::sm>( omt_tgt ), false );
    int harvested = 0;
    int total = 0;
    tripoint mapmin = tripoint( 0, 0, omt_tgt.z() );
    tripoint mapmax = tripoint( 2 * SEEX - 1, 2 * SEEY - 1, omt_tgt.z() );
    for( const tripoint &p : target_bay.points_in_rectangle( mapmin, mapmax ) ) {
        if( target_bay.ter( p ).obj().has_flag( ter_furn_flag::TFLAG_TREE ) && rng( 0, 100 ) < chance ) {
            total++;
            if( estimate ) {
                continue;
            }
            // get a random number that is either 1 or -1
            point dir( 3 * ( 2 * rng( 0, 1 ) - 1 ) + rng( -1, 1 ), 3 * rng( -1, 1 ) + rng( -1, 1 ) );
            tripoint to = p + tripoint( dir, omt_tgt.z() );
            std::vector<tripoint> tree = line_to( p, to, rng( 1, 8 ) );
            for( auto &elem : tree ) {
                target_bay.destroy( elem );
                target_bay.ter_set( elem, t_trunk );
            }
            target_bay.ter_set( p, t_dirt );
            harvested++;
        }
    }
    if( estimate ) {
        return total;
    }
    if( !force_cut_trunk ) {
        target_bay.save();
        return harvested;
    }
    // having cut down the trees, cut the trunks into logs
    for( const tripoint &p : target_bay.points_in_rectangle( mapmin, mapmax ) ) {
        if( target_bay.ter( p ) == ter_id( "t_trunk" ) ) {
            target_bay.ter_set( p, t_dirt );
            target_bay.spawn_item( p, itype_log, rng( 2, 3 ), 0, calendar::turn );
            harvested++;
        }
    }
    target_bay.save();
    return harvested;
}

mass_volume om_harvest_itm( const npc_ptr &comp, const tripoint_abs_omt &omt_tgt, int chance,
                            bool take )
{
    tinymap target_bay;
    target_bay.load( project_to<coords::sm>( omt_tgt ), false );
    units::mass harvested_m = 0_gram;
    units::volume harvested_v = 0_ml;
    units::mass total_m = 0_gram;
    units::volume total_v = 0_ml;
    int total_num = 0;
    int harvested_num = 0;
    tripoint mapmin = tripoint( 0, 0, omt_tgt.z() );
    tripoint mapmax = tripoint( 2 * SEEX - 1, 2 * SEEY - 1, omt_tgt.z() );
    for( const tripoint &p : target_bay.points_in_rectangle( mapmin, mapmax ) ) {
        for( const item &i : target_bay.i_at( p ) ) {
            total_m += i.weight( true );
            total_v += i.volume( true );
            total_num += 1;
            if( take && x_in_y( chance, 100 ) ) {
                if( comp ) {
                    comp->companion_mission_inv.push_back( i );
                }
                harvested_m += i.weight( true );
                harvested_v += i.volume( true );
                harvested_num += 1;
            }
        }
        if( take ) {
            target_bay.i_clear( p );
        }
    }
    target_bay.save();
    mass_volume results;
    if( take ) {
        results.wgt = harvested_m;
        results.vol = harvested_v;
        results.count = harvested_num;
    } else {
        results.wgt = total_m;
        results.vol = total_v;
        results.count = total_num;
    }
    return results;
}

tripoint_abs_omt om_target_tile( const tripoint_abs_omt &omt_pos, int min_range, int range,
                                 const std::vector<std::string> &possible_om_types, ot_match_type match_type, bool must_see,
                                 bool popup_notice, const tripoint_abs_omt &source, bool bounce )
{
    bool errors = false;
    if( popup_notice ) {
        popup( _( "Select a location between  %d and  %d tiles away." ), min_range, range );
    }

    std::vector<std::string> bounce_locations = { faction_hide_site_0_string };

    tripoint_abs_omt where;
    om_range_mark( omt_pos, range );
    om_range_mark( omt_pos, min_range, true, "Y;X: MIN RANGE" );
    if( source == tripoint_abs_omt( -999, -999, -999 ) ) {
        where = ui::omap::choose_point();
    } else {
        where = ui::omap::choose_point( source );
    }
    om_range_mark( omt_pos, range, false );
    om_range_mark( omt_pos, min_range, false, "Y;X: MIN RANGE" );

    if( where == overmap::invalid_tripoint ) {
        return tripoint_abs_omt( -999, -999, -999 );
    }
    int dist = rl_dist( where.xy(), omt_pos.xy() );
    if( dist > range || dist < min_range ) {
        popup( _( "You must select a target between %d and %d range from the base.  Range: %d" ),
               min_range, range, dist );
        errors = true;
    }

    tripoint_abs_omt omt_tgt = where;

    const oter_id &omt_ref = overmap_buffer.ter( omt_tgt );

    if( must_see && !overmap_buffer.seen( omt_tgt ) ) {
        errors = true;
        popup( _( "You must be able to see the target that you select." ) );
    }

    if( !errors ) {
        for( const std::string &pos_om : bounce_locations ) {
            if( bounce && omt_ref.id().c_str() == pos_om && range > 5 ) {
                if( query_yn( _( "Do you want to bounce off this location to extend range?" ) ) ) {
                    om_line_mark( omt_pos, omt_tgt );
                    tripoint_abs_omt dest =
                        om_target_tile( omt_tgt, 2, range * .75, possible_om_types, match_type, true, false,
                                        omt_tgt, true );
                    om_line_mark( omt_pos, omt_tgt, false );
                    return dest;
                }
            }
        }

        if( possible_om_types.empty() ) {
            return omt_tgt;
        }

        for( const std::string &pos_om : possible_om_types ) {
            if( is_ot_match( pos_om, omt_ref, match_type ) ) {
                return omt_tgt;
            }
        }
    }

    return om_target_tile( omt_pos, min_range, range, possible_om_types, match_type );
}

void om_range_mark( const tripoint_abs_omt &origin, int range, bool add_notes,
                    const std::string &message )
{
    std::vector<tripoint_abs_omt> note_pts;
    //North Limit
    for( int x = origin.x() - range; x < origin.x() + range + 1; x++ ) {
        note_pts.emplace_back( x, origin.y() - range, origin.z() );
    }
    //South
    for( int x = origin.x() - range; x < origin.x() + range + 1; x++ ) {
        note_pts.emplace_back( x, origin.y() + range, origin.z() );
    }
    //West
    for( int y = origin.y() - range; y < origin.y() + range + 1; y++ ) {
        note_pts.emplace_back( origin.x() - range, y, origin.z() );
    }
    //East
    for( int y = origin.y() - range; y < origin.y() + range + 1; y++ ) {
        note_pts.emplace_back( origin.x() + range, y, origin.z() );
    }

    for( auto pt : note_pts ) {
        if( add_notes ) {
            if( !overmap_buffer.has_note( pt ) ) {
                overmap_buffer.add_note( pt, message );
            }
        } else {
            if( overmap_buffer.has_note( pt ) && overmap_buffer.note( pt ) == message ) {
                overmap_buffer.delete_note( pt );
            }
        }
    }
}

void om_line_mark( const tripoint_abs_omt &origin, const tripoint_abs_omt &dest, bool add_notes,
                   const std::string &message )
{
    std::vector<tripoint_abs_omt> note_pts = line_to( origin, dest );

    for( auto pt : note_pts ) {
        if( add_notes ) {
            if( !overmap_buffer.has_note( pt ) ) {
                overmap_buffer.add_note( pt, message );
            }
        } else {
            if( overmap_buffer.has_note( pt ) && overmap_buffer.note( pt ) == message ) {
                overmap_buffer.delete_note( pt );
            }
        }
    }
}

bool om_set_hide_site( npc &comp, const tripoint_abs_omt &omt_tgt,
                       const std::vector<item *> &itms,
                       const std::vector<item *> &itms_rem )
{
    tinymap target_bay;
    target_bay.load( project_to<coords::sm>( omt_tgt ), false );
    target_bay.ter_set( point( 11, 10 ), t_improvised_shelter );
    for( item *i : itms_rem ) {
        comp.companion_mission_inv.add_item( *i );
        target_bay.i_rem( point( 11, 10 ), i );
    }
    Character &player_character = get_player_character();
    for( const item *i : itms ) {
        target_bay.add_item_or_charges( point( 11, 10 ), *i );
        player_character.use_amount( i->typeId(), 1 );
    }
    target_bay.save();

    overmap_buffer.ter_set( omt_tgt, oter_id( faction_hide_site_0_string ) );

    overmap_buffer.reveal( omt_tgt.xy(), 3, 0 );
    return true;
}

// path and travel time
time_duration companion_travel_time_calc( const tripoint_abs_omt &omt_pos,
        const tripoint_abs_omt &omt_tgt, time_duration work, int trips, int haulage )
{
    std::vector<tripoint_abs_omt> journey = line_to( omt_pos, omt_tgt );
    return companion_travel_time_calc( journey, work, trips, haulage );
}

time_duration companion_travel_time_calc( const std::vector<tripoint_abs_omt> &journey,
        time_duration work, int trips, int haulage )
{
    int one_way = 0;
    for( const tripoint_abs_omt &om : journey ) {
        const oter_id &omt_ref = overmap_buffer.ter( om );
        std::string om_id = omt_ref.id().c_str();
        // Player walks 1 om in roughly 30 seconds
        if( om_id == "field" ) {
            one_way += 30 + 30 * haulage;
        } else if( omt_ref->get_type_id() == oter_type_forest_trail ) {
            one_way += 35 + 30 * haulage;
        } else if( om_id == "forest_thick" ) {
            one_way += 50 + 30 * haulage;
        } else if( om_id == "forest_water" ) {
            one_way += 60 + 30 * haulage;
        } else if( is_river( omt_ref ) ) {
            // hauling stuff over a river is slow, because you have to portage most items
            one_way += 200 + 40 * haulage;
        } else {
            one_way += 40 + 30 * haulage;
        }
    }
    return work + one_way * trips * 1_seconds;
}

int om_carry_weight_to_trips( const units::mass &mass, const units::volume &volume,
                              const units::mass &carry_mass, const units::volume &carry_volume )
{
    int trips_m = 1 + mass / carry_mass;
    int trips_v = 1 + volume / carry_volume;
    // return the number of round trips
    return 2 * std::max( trips_m, trips_v );
}

int om_carry_weight_to_trips( const std::vector<item *> &itms, const npc_ptr &comp )
{
    units::mass total_m = 0_gram;
    units::volume total_v = 0_ml;
    for( const auto &i : itms ) {
        total_m += i->weight( true );
        total_v += i->volume( true );
    }
    units::mass max_m = comp ? comp->weight_capacity() - comp->weight_carried() : 30_kilogram;
    //Assume an additional pack will be carried in addition to normal gear
    units::volume sack_v = item( itype_makeshift_sling ).get_total_capacity();
    units::volume max_v = comp ? comp->free_space() : sack_v;
    max_v += sack_v;
    return om_carry_weight_to_trips( total_m, total_v, max_m, max_v );
}

std::vector<tripoint_abs_omt> om_companion_path( const tripoint_abs_omt &start, int range_start,
        bool bounce )
{
    std::vector<tripoint_abs_omt> scout_points;
    tripoint_abs_omt last = start;
    int range = range_start;
    int def_range = range_start;
    while( range > 3 ) {
        tripoint_abs_omt spt = om_target_tile( last, 0, range, {}, ot_match_type::exact, false, true, last,
                                               false );
        if( spt == tripoint_abs_omt( -999, -999, -999 ) ) {
            scout_points.clear();
            return scout_points;
        }
        if( last == spt ) {
            break;
        }
        std::vector<tripoint_abs_omt> note_pts = line_to( last, spt );
        scout_points.insert( scout_points.end(), note_pts.begin(), note_pts.end() );
        om_line_mark( last, spt );
        range -= rl_dist( spt.xy(), last.xy() );
        last = spt;

        const oter_id &omt_ref = overmap_buffer.ter( last );

        if( bounce && omt_ref.id() == oter_faction_hide_site_0 ) {
            range = def_range * .75;
            def_range = range;
        }
    }
    for( auto pt : scout_points ) {
        om_line_mark( pt, pt, false );
    }
    return scout_points;
}

// camp utility functions
// mission support functions
std::vector<item *> basecamp::give_equipment( std::vector<item *> equipment,
        const std::string &msg )
{
    std::vector<item *> equipment_lost;
    do {
        std::vector<std::string> names;
        names.reserve( equipment.size() );
        for( auto &i : equipment ) {
            names.push_back( i->tname() + " [" + std::to_string( i->charges ) + "]" );
        }

        // Choose item if applicable
        const int i_index = uilist( msg, names );
        if( i_index < 0 || static_cast<size_t>( i_index ) >= equipment.size() ) {
            return equipment_lost;
        }
        equipment_lost.push_back( equipment[i_index] );
        equipment.erase( equipment.begin() + i_index );
    } while( !equipment.empty() );
    return equipment_lost;
}

bool basecamp::validate_sort_points()
{
    auto &mgr = zone_manager::get_manager();
    map &here = get_map();
    if( here.check_vehicle_zones( here.get_abs_sub().z() ) ) {
        mgr.cache_vzones();
    }
    tripoint src_loc = here.getlocal( bb_pos ) + point_north;
    const tripoint_abs_ms abspos = get_player_character().get_location();
    if( !mgr.has_near( zone_type_CAMP_STORAGE, abspos, 60 ) ||
        !mgr.has_near( zone_type_CAMP_FOOD, abspos, 60 ) ) {
        if( query_yn( _( "You do not have sufficient sort zones.  Do you want to add them?" ) ) ) {
            return set_sort_points();
        } else {
            return false;
        }
    } else {
        const std::unordered_set<tripoint_abs_ms> &src_set =
            mgr.get_near( zone_type_CAMP_STORAGE, abspos );
        const std::vector<tripoint_abs_ms> &src_sorted =
            get_sorted_tiles_by_distance( abspos, src_set );
        // Find the nearest unsorted zone to dump objects at
        if( !src_sorted.empty() ) {
            src_loc = here.getlocal( src_sorted.front() );
        }
    }
    set_dumping_spot( here.getglobal( src_loc ) );
    return true;
}

bool basecamp::set_sort_points()
{
    popup( _( "Sorting zones have changed.  Please create some sorting zones.  "
              "You must create a camp food zone, and a camp storage zone." ) );
    g->zones_manager();
    return validate_sort_points();
}

// camp analysis functions
std::vector<std::pair<std::string, tripoint_abs_omt>> talk_function::om_building_region(
            const tripoint_abs_omt &omt_pos, int range, bool purge )
{
    std::vector<std::pair<std::string, tripoint_abs_omt>> om_camp_region;
    for( const tripoint_abs_omt &omt_near_pos : points_in_radius( omt_pos, range ) ) {
        const oter_id &omt_rnear = overmap_buffer.ter( omt_near_pos );
        std::string om_rnear_id = omt_rnear.id().c_str();
        if( !purge || ( om_rnear_id.find( "faction_base_" ) != std::string::npos &&
                        om_rnear_id.find( "faction_base_camp" ) == std::string::npos ) ) {
            om_camp_region.emplace_back( om_rnear_id, omt_near_pos );
        }
    }
    return om_camp_region;
}

point talk_function::om_simple_dir( const tripoint_abs_omt &omt_pos,
                                    const tripoint_abs_omt &omt_tar )
{
    point_rel_omt diff = omt_tar.xy() - omt_pos.xy();
    return { clamp( diff.x(), -1, 1 ), clamp( diff.y(), -1, 1 ) };
}

// mission descriptions
std::string camp_trip_description( const time_duration &total_time,
                                   const time_duration &working_time,
                                   const time_duration &travel_time, int distance, int trips,
                                   int need_food )
{
    std::string entry = "\n";
    //A square is roughly 3 m
    int dist_m = distance * SEEX * 2 * 3;
    if( dist_m > 1000 ) {
        entry += string_format( _( ">Distance:%15.2f (km)\n" ), dist_m / 1000.0 );
        entry += string_format( _( ">One Way: %15d (trips)\n" ), trips );
        entry += string_format( _( ">Covered: %15.2f (km)\n" ), dist_m / 1000.0 * trips );
    } else {
        entry += string_format( _( ">Distance:%15d (m)\n" ), dist_m );
        entry += string_format( _( ">One Way: %15d (trips)\n" ), trips );
        entry += string_format( _( ">Covered: %15d (m)\n" ), dist_m * trips );
    }
    entry += string_format( _( ">Travel:  %s\n" ), right_justify( to_string( travel_time ), 23 ) );
    entry += string_format( _( ">Working: %s\n" ), right_justify( to_string( working_time ), 23 ) );
    entry += "----                   ----\n";
    entry += string_format( _( "Total:    %s\n" ), right_justify( to_string( total_time ), 23 ) );
    entry += string_format( _( "Food:     %15d (kcal)\n\n" ), need_food );
    return entry;
}

std::string basecamp::craft_description( const recipe_id &itm )
{
    const recipe &making = itm.obj();

    std::vector<std::string> component_print_buffer;
    int pane = FULL_SCREEN_WIDTH;
    const requirement_data &req = making.simple_requirements();
    auto tools = req.get_folded_tools_list( pane, c_white, _inv, 1 );
    auto comps = req.get_folded_components_list( pane, c_white, _inv,
                 making.get_component_filter(), 1 );

    component_print_buffer.insert( component_print_buffer.end(), tools.begin(), tools.end() );
    component_print_buffer.insert( component_print_buffer.end(), comps.begin(), comps.end() );

    std::string comp;
    for( auto &elem : component_print_buffer ) {
        str_append( comp, elem, "\n" );
    }
    comp = string_format( _( "Skill used: %s\nDifficulty: %d\n%s\nTime: %s\n" ),
                          making.skill_used.obj().name(), making.difficulty, comp,
                          to_string( base_camps::to_workdays( making.batch_duration( get_player_character() ) ) ) );
    return comp;
}

int basecamp::recruit_evaluation( int &sbase, int &sexpansions, int &sfaction, int &sbonus ) const
{
    auto e = expansions.find( base_camps::base_dir );
    if( e == expansions.end() ) {
        sbase = 0;
        sexpansions = 0;
        sfaction = 0;
        sbonus = 0;
        return 0;
    }
    sbase = e->second.cur_level * 5;
    sexpansions = expansions.size() * 2;

    //How could we ever starve?
    //More than 5 farms at recruiting base
    int farm = 0;
    for( const point &dir : directions ) {
        if( has_provides( "farming", dir ) ) {
            farm++;
        }
    }
    sfaction = std::min( camp_food_supply() / 10000, 10 );
    sfaction += std::min( camp_discipline() / 10, 5 );
    sfaction += std::min( camp_morale() / 10, 5 );

    //Secret or Hidden Bonus
    //Please avoid openly discussing so that there is some mystery to the system
    sbonus = 0;
    if( farm >= 5 ) {
        sbonus += 10;
    }
    //More machine than man
    //Bionics count > 10, respect > 75
    if( get_player_character().get_bionics().size() > 10 && camp_discipline() > 75 ) {
        sbonus += 10;
    }
    //Survival of the fittest
    if( g->get_kill_tracker().npc_kill_count() > 10 ) {
        sbonus += 10;
    }
    return sbase + sexpansions + sfaction + sbonus;
}
int basecamp::recruit_evaluation() const
{
    int sbase;
    int sexpansions;
    int sfaction;
    int sbonus;
    return recruit_evaluation( sbase, sexpansions, sfaction, sbonus );
}

std::string basecamp::recruit_description( int npc_count )
{
    int sbase;
    int sexpansions;
    int sfaction;
    int sbonus;
    int total = recruit_evaluation( sbase, sexpansions, sfaction, sbonus );
    std::string desc = string_format( _( "Notes:\n"
                                         "Recruiting additional followers is very dangerous and "
                                         "expensive.  The outcome is heavily dependent on the "
                                         "skill of the  companion you send and the appeal of "
                                         "your base.\n\n"
                                         "Skill used: social\n"
                                         "Difficulty: 2\n"
                                         "Base Score:                   +%3d%%\n"
                                         "> Expansion Bonus:            +%3d%%\n"
                                         "> Faction Bonus:              +%3d%%\n"
                                         "> Special Bonus:              +%3d%%\n\n"
                                         "Total: Skill                  +%3d%%\n\n"
                                         "Risk: High\n"
                                         "Time: 4 Days\n"
                                         "Positions: %d/1\n" ), sbase, sexpansions, sfaction,
                                      sbonus, total, npc_count );
    return desc;
}

std::string basecamp::gathering_description( const std::string &bldg )
{
    item_group_id itemlist;
    if( item_group::group_is_defined( item_group_id( "gathering_" + bldg ) ) ) {
        itemlist = item_group_id( "gathering_" + bldg );
    } else {
        itemlist = Item_spawn_data_forest;
    }
    std::string output;

    // Functions like the debug item group tester but only rolls 6 times so the player
    // doesn't have perfect knowledge
    std::map<std::string, int> itemnames;
    for( size_t a = 0; a < 6; a++ ) {
        const auto items = item_group::items_from( itemlist, calendar::turn );
        for( const item &it : items ) {
            itemnames[it.display_name()]++;
        }
    }
    // Invert the map to get sorting!
    std::multimap<int, std::string> itemnames2;
    for( const auto &e : itemnames ) {
        itemnames2.insert( std::pair<int, std::string>( e.second, e.first ) );
    }
    for( const auto &e : itemnames2 ) {
        str_append( output, "> ", e.second, "\n" );
    }
    return output;
}

std::string basecamp::farm_description( const tripoint_abs_omt &farm_pos, size_t &plots_count,
                                        farm_ops operation )
{
    std::pair<size_t, std::string> farm_data = farm_action( farm_pos, operation );
    std::string entry;
    plots_count = farm_data.first;
    switch( operation ) {
        case farm_ops::harvest:
            entry += _( "Harvestable: " ) + std::to_string( plots_count ) + "\n" + farm_data.second;
            break;
        case farm_ops::plant:
            entry += _( "Ready for Planting: " ) + std::to_string( plots_count ) + "\n";
            break;
        case farm_ops::plow:
            entry += _( "Needs Plowing: " ) + std::to_string( plots_count ) + "\n";
            break;
        default:
            debugmsg( "Farm operations called with no operation" );
            break;
    }
    return entry;
}

// food supply
int camp_food_supply( int change, bool return_days )
{
    faction *yours = get_player_character().get_faction();
    yours->food_supply += change;
    if( yours->food_supply < 0 ) {
        yours->likes_u += yours->food_supply / 1250;
        yours->respects_u += yours->food_supply / 625;
        yours->trusts_u += yours->food_supply / 625;
        yours->food_supply = 0;
    }
    if( return_days ) {
        return yours->food_supply / 2500;
    }

    return yours->food_supply;
}

int camp_food_supply( time_duration work )
{
    return camp_food_supply( -time_to_food( work ) );
}

int time_to_food( time_duration work )
{
    return 2500 * to_hours<int>( work ) / 24;
}

// mission support
bool basecamp::distribute_food()
{
    if( !validate_sort_points() ) {
        popup( _( "You do not have a camp food zone.  Aborting…" ) );
        return false;
    }

    map &here = get_map();
    auto &mgr = zone_manager::get_manager();
    if( here.check_vehicle_zones( here.get_abs_sub().z() ) ) {
        mgr.cache_vzones();
    }
    const tripoint_abs_ms &abspos = get_dumping_spot();
    const std::unordered_set<tripoint_abs_ms> &z_food =
        mgr.get_near( zone_type_CAMP_FOOD, abspos, 60 );

    double quick_rot = 0.6 + ( has_provides( "pantry" ) ? 0.1 : 0 );
    double slow_rot = 0.8 + ( has_provides( "pantry" ) ? 0.05 : 0 );
    int total = 0;

    const auto rot_multip = [&]( const item & it, item * const container ) {
        if( !it.goes_bad() ) {
            return 1.;
        }
        float spoil_mod = 1.0f;
        if( container ) {
            if( item_pocket *const pocket = container->contained_where( it ) ) {
                spoil_mod = pocket->spoil_multiplier();
            }
        }
        // Container seals and prevents any spoilage.
        if( spoil_mod == 0 ) {
            return 1.;
        }
        // @TODO: this does not handle fridges or things like root cellar, but maybe it shouldn't.
        const time_duration rots_in = ( it.get_shelf_life() - it.get_rot() ) / spoil_mod;
        if( rots_in >= 5_days ) {
            return 1.;
        } else if( rots_in >= 2_days ) {
            return slow_rot;
        } else {
            return quick_rot;
        }
    };
    const auto consume_non_recursive = [&]( item & it, item * const container ) {
        if( !it.is_comestible() ) {
            return false;
        }
        // Stuff like butchery refuse and other disgusting stuff
        if( it.get_comestible_fun() < -6 ) {
            return false;
        }
        if( it.has_flag( flag_INEDIBLE ) ) {
            return false;
        }
        if( it.rotten() ) {
            return false;
        }
        const int kcal = it.get_comestible()->default_nutrition.kcal() * it.count() * rot_multip( it,
                         container );
        if( kcal <= 0 ) {
            // can happen if calories is low and rot is high.
            return false;
        }
        total += kcal;
        return true;
    };

    // Returns whether the item should be removed from the map.
    const auto consume = [&]( item & it, item * const container ) {
        if( it.is_food_container() ) {
            std::vector<item *> to_remove;
            it.visit_items( [&]( item * content, item * parent ) {
                if( consume_non_recursive( *content, parent ) ) {
                    to_remove.push_back( content );
                    return VisitResponse::SKIP;
                }
                return VisitResponse::NEXT;
            } );
            if( to_remove.empty() ) {
                return false;
            }
            for( item *const food : to_remove ) {
                it.remove_item( *food );
            }
            it.on_contents_changed();
            return false;
        }
        return consume_non_recursive( it, container );
    };
    for( const tripoint_abs_ms &p_food_stock_abs : z_food ) {
        // @FIXME: this will not handle zones in vehicle
        const tripoint p_food_stock = here.getlocal( p_food_stock_abs );
        map_stack items = here.i_at( p_food_stock );
        for( auto iter = items.begin(); iter != items.end(); ) {
            if( consume( *iter, nullptr ) ) {
                iter = items.erase( iter );
            } else {
                ++iter;
            }
        }
    }

    if( total <= 0 ) {
        popup( _( "No suitable items are located at the drop points…" ) );
        return false;
    }

    popup( _( "You distribute %d kcal worth of food to your companions." ), total );
    camp_food_supply( total );
    return true;
}

// morale
int camp_discipline( int change )
{
    faction *yours = get_player_character().get_faction();
    yours->respects_u += change;
    return yours->respects_u;
}

int camp_morale( int change )
{
    faction *yours = get_player_character().get_faction();
    yours->likes_u += change;
    return yours->likes_u;
}

void basecamp::place_results( const item &result )
{
    if( by_radio ) {
        tinymap target_bay;
        target_bay.load( project_to<coords::sm>( omt_pos ), false );
        const tripoint &new_spot = target_bay.getlocal( get_dumping_spot() );
        target_bay.add_item_or_charges( new_spot, result, true );
        apply_camp_ownership( new_spot, 10 );
        target_bay.save();
    } else {
        map &here = get_map();
        auto &mgr = zone_manager::get_manager();
        if( here.check_vehicle_zones( here.get_abs_sub().z() ) ) {
            mgr.cache_vzones();
        }
        Character &player_character = get_player_character();
        const tripoint_abs_ms abspos = player_character.get_location();
        if( mgr.has_near( zone_type_CAMP_STORAGE, abspos ) ) {
            const std::unordered_set<tripoint_abs_ms> &src_set =
                mgr.get_near( zone_type_CAMP_STORAGE, abspos );
            const std::vector<tripoint_abs_ms> &src_sorted =
                get_sorted_tiles_by_distance( abspos, src_set );
            // Find the nearest unsorted zone to dump objects at
            for( const tripoint_abs_ms &src : src_sorted ) {
                const tripoint &src_loc = here.getlocal( src );
                here.add_item_or_charges( src_loc, result, true );
                apply_camp_ownership( src_loc, 10 );
                break;
            }
            //or dump them at players feet
        } else {
            here.add_item_or_charges( player_character.pos(), result, true );
            apply_camp_ownership( player_character.pos(), 0 );
        }
    }
}

void apply_camp_ownership( const tripoint &camp_pos, int radius )
{
    map &here = get_map();
    for( const tripoint &p : here.points_in_rectangle( camp_pos + point( -radius, -radius ),
            camp_pos + point( radius, radius ) ) ) {
        map_stack items = here.i_at( p.xy() );
        for( item &elem : items ) {
            elem.set_owner( get_player_character() );
        }
    }
}

// combat and danger
// this entire system is stupid
bool survive_random_encounter( npc &comp, std::string &situation, int favor, int threat )
{
    popup( _( "While %s, a silent specter approaches %s…" ), situation, comp.get_name() );
    int skill_1 = comp.get_skill_level( skill_survival );
    int skill_2 = comp.get_skill_level( skill_speech );
    if( skill_1 + favor > rng( 0, 10 ) ) {
        popup( _( "%s notices the antlered horror and slips away before it gets too close." ),
               comp.get_name() );
        talk_function::companion_skill_trainer( comp, "gathering", 10_minutes, 10 - favor );
    } else if( skill_2 + favor > rng( 0, 10 ) ) {
        popup( _( "Another survivor approaches %s asking for directions." ), comp.get_name() );
        popup( _( "Fearful that he may be an agent of some hostile faction, "
                  "%s doesn't mention the camp." ), comp.get_name() );
        popup( _( "The two part on friendly terms and the survivor isn't seen again." ) );
        talk_function::companion_skill_trainer( comp, skill_recruiting, 10_minutes, 10 - favor );
    } else {
        popup( _( "%s didn't detect the ambush until it was too late!" ), comp.get_name() );
        int skill = comp.get_skill_level( skill_melee ) +
                    0.5 * comp.get_skill_level( skill_survival ) +
                    comp.get_skill_level( skill_bashing ) +
                    comp.get_skill_level( skill_cutting ) +
                    comp.get_skill_level( skill_stabbing ) +
                    comp.get_skill_level( skill_unarmed ) + comp.get_skill_level( skill_dodge );
        int monsters = rng( 0, threat );
        if( skill * rng( 8, 12 ) > ( monsters * rng( 8, 12 ) ) ) {
            if( one_in( 2 ) ) {
                popup( _( "The bull moose charged %s from the tree line…" ), comp.get_name() );
                popup( _( "Despite being caught off guard %s was able to run away until the "
                          "moose gave up pursuit." ), comp.get_name() );
            } else {
                popup( _( "The jabberwock grabbed %s by the arm from behind and "
                          "began to scream." ), comp.get_name() );
                popup( _( "Terrified, %s spun around and delivered a massive kick "
                          "to the creature's torso…" ), comp.get_name() );
                popup( _( "Collapsing into a pile of gore, %s walked away unscathed…" ),
                       comp.get_name() );
                popup( _( "(Sounds like bullshit, you wonder what really happened.)" ) );
            }
            talk_function::companion_skill_trainer( comp, skill_combat, 10_minutes, 10 - favor );
        } else {
            if( one_in( 2 ) ) {
                popup( _( "%s turned to find the hideous black eyes of a giant wasp "
                          "staring back from only a few feet away…" ), comp.get_name() );
                popup( _( "The screams were terrifying, there was nothing anyone could do." ) );
            } else {
                popup( _( "Pieces of %s were found strewn across a few bushes." ), comp.get_name() );
                popup( _( "(You wonder if your companions are fit to work on their own…)" ) );
            }
            overmap_buffer.remove_npc( comp.getID() );
            return false;
        }
    }
    return true;
}
