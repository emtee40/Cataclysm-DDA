#pragma once
#ifndef CATA_SRC_NPCTALK_H
#define CATA_SRC_NPCTALK_H

#include "type_id.h"
#include "character.h"

class item;
class json_talk_topic;
class npc;
class time_duration;

namespace talk_function
{

struct teach_domain {
    skill_id skill = skill_id();
    matype_id style = matype_id();
    spell_id spell = spell_id();
    proficiency_id prof = proficiency_id();
};

void nothing( npc & );
void assign_mission( npc & );
void mission_success( npc & );
void mission_failure( npc & );
void clear_mission( npc & );
void mission_reward( npc & );
void mission_favor( npc & );
void give_equipment( npc & );
void give_equipment_allowance( npc &p, int allowance );
void give_aid( npc & );
void give_all_aid( npc & );
void buy_horse( npc & );
void buy_cow( npc & );
void buy_chicken( npc & );
void bionic_install( npc & );
void bionic_remove( npc & );
void dismount( npc & );
void find_mount( npc & );

void barber_beard( npc & );
void barber_hair( npc & );
void buy_haircut( npc & );
void buy_shave( npc & );
void morale_chat( npc & );
void morale_chat_activity( npc & );
void buy_10_logs( npc & );
void buy_100_logs( npc & );
void start_trade( npc & );
void sort_loot( npc & );
void do_construction( npc & );
void do_mining( npc & );
void do_mopping( npc & );
void do_read( npc & );
void do_chop_plank( npc & );
void do_vehicle_deconstruct( npc & );
void do_vehicle_repair( npc & );
void do_chop_trees( npc & );
void do_fishing( npc & );
void do_farming( npc & );
void do_butcher( npc & );
void revert_activity( npc & );
void goto_location( npc & );
void assign_base( npc & );
void assign_guard( npc & );
void assign_camp( npc & );
void abandon_camp( npc & );
void stop_guard( npc & );
void end_conversation( npc & );
void insult_combat( npc & );
void reveal_stats( npc & );
void drop_items_in_place( npc &p );
void follow( npc & );                // p becomes a member of your_followers
void follow_only( npc & );           // p starts following you
void deny_follow( npc & );           // p gets "asked_to_follow"
void deny_lead( npc & );             // p gets "asked_to_lead"
void deny_equipment( npc & );        // p gets "asked_for_item"
void deny_train( npc & );            // p gets "asked_to_train"
void deny_personal_info( npc & );    // p gets "asked_personal_info"
void hostile( npc & );               // p turns hostile to u
void flee( npc & );
void leave( npc & );                 // p becomes indifferent
void stop_following( npc & );
void stranger_neutral( npc & );      // p is now neutral towards you

bool drop_stolen_item( item &cur_item, npc &p );

void start_mugging( npc & );
void player_leaving( npc & );

void remove_stolen_status( npc & );

void drop_weapon( npc & );
void player_weapon_away( npc & );
void player_weapon_drop( npc & );
void drop_stolen_item( npc & );

void lead_to_safety( npc & );
void start_training( npc & );
void start_training_npc( npc & );
void start_training_seminar( npc &p );
void start_training_gen( Character &teacher, std::vector<Character *> &students, teach_domain &d );

void wake_up( npc & );
void copy_npc_rules( npc &p );
void set_npc_pickup( npc &p );
void npc_die( npc &p );
void npc_thankful( npc &p );
void clear_overrides( npc &p );
void do_disassembly( npc &p );
} // namespace talk_function

time_duration calc_skill_training_time( const npc &p, const skill_id &skill );
time_duration calc_skill_training_time_char( const Character &teacher, const Character &student,
        const skill_id &skill );
int calc_skill_training_cost( const npc &p, const skill_id &skill );
int calc_skill_training_cost_char( const Character &teacher, const Character &student,
                                   const skill_id &skill );
time_duration calc_proficiency_training_time( const proficiency_id &proficiency );
time_duration calc_proficiency_training_time( const Character &teacher, const Character &student,
        const proficiency_id &proficiency );
int calc_proficiency_training_cost( const npc &p, const proficiency_id &proficiency );
int calc_proficiency_training_cost( const Character &teacher, const Character &student,
                                    const proficiency_id &proficiency );
time_duration calc_ma_style_training_time( const npc &p, const matype_id &id );
time_duration calc_ma_style_training_time( const Character &teacher, const Character &student,
        const matype_id &id );
int calc_ma_style_training_cost( const npc &p, const matype_id &id );
int calc_ma_style_training_cost( const Character &teacher, const Character &student,
                                 const matype_id &id );
time_duration calc_spell_training_time( const Character &teacher, const Character &student,
                                        const spell_id &id );
int calc_spell_training_cost_gen( const bool knows, int difficulty, int level );
int calc_spell_training_cost( const Character &teacher, const Character &student,
                              const spell_id &id );

const json_talk_topic *get_talk_topic( const std::string &id );

template<class T>
std::vector<int> npcs_select_menu( const std::vector<T *> &npc_list,
                                   const std::string &prompt,
                                   std::function<bool( const T * )> exclude_func )
{
    std::vector<int> picked;
    if( npc_list.empty() ) {
        return picked;
    }
    const int npc_count = npc_list.size();
    int last_index = 0;
    do {
        uilist nmenu;
        nmenu.text = prompt;
        for( int i = 0; i < npc_count; i++ ) {
            std::string entry;
            if( std::find( picked.begin(), picked.end(), i ) != picked.end() ) {
                entry = "* ";
            }
            bool enable = exclude_func == nullptr || !exclude_func( npc_list[i] );
            if( const npc *np = dynamic_cast<const npc *>( npc_list[i] ) ) {
                entry += np->name_and_activity();
            } else {
                entry += npc_list[i]->disp_name( false, true );
            }
            nmenu.addentry( i, enable, MENU_AUTOASSIGN, entry );
        }
        nmenu.addentry( npc_count, true, MENU_AUTOASSIGN, _( "Finish selection" ) );
        nmenu.selected = nmenu.fselected = last_index;
        nmenu.query();
        if( nmenu.ret < 0 ) {
            return std::vector<int>();
        } else if( nmenu.ret >= npc_count ) {
            break;
        }
        std::vector<int>::iterator exists = std::find( picked.begin(), picked.end(), nmenu.ret );
        if( exists != picked.end() ) {
            picked.erase( exists );
        } else {
            picked.push_back( nmenu.ret );
        }
        last_index = nmenu.fselected;
    } while( true );
    return picked;
}

#endif // CATA_SRC_NPCTALK_H
