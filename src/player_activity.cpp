#include "player_activity.h"

#include <algorithm>

#include "activity_handlers.h"
#include "activity_type.h"
#include "player.h"
#include "sounds.h"

static const std::map< activity_id, std::string> activity_names = {
    { activity_id( "ACT_BURROW" ), translate_marker( "Burrowing" ) },
    { activity_id( "ACT_CRAFT" ), translate_marker( "Crafting" ) },
    { activity_id( "ACT_FILL_LIQUID" ), translate_marker( "Filling with liquid" ) },
    { activity_id( "ACT_PICKAXE" ), translate_marker( "Mining" ) },
    { activity_id( "ACT_DROP" ), translate_marker( "Dropping" ) },
    { activity_id( "ACT_STASH" ), translate_marker( "Stashing" ) },
    { activity_id( "ACT_PULP" ), translate_marker( "Pulping" ) },
    { activity_id( "ACT_GAME" ), translate_marker( "Gaming" ) },
    { activity_id( "ACT_START_FIRE" ), translate_marker( "Starting fire" ) },
    { activity_id( "ACT_VIBE" ), translate_marker( "Recreating" ) },
    { activity_id( "ACT_HAND_CRANK" ), translate_marker( "Handcrank charging" ) },
    { activity_id( "ACT_OXYTORCH" ), translate_marker( "Welding" ) },
    { activity_id( "ACT_AIM" ), translate_marker( "Aiming" ) },
    { activity_id( "ACT_PICKUP" ), translate_marker( "Picking up" ) },
    { activity_id( "ACT_WEAR" ), translate_marker( "Wearing" ) },
    { activity_id( "ACT_MULTIPLE_FISH" ), translate_marker( "Fishing" ) },
    { activity_id( "ACT_MULTIPLE_CONSTRUCTION" ), translate_marker( "Constructing" ) },
    { activity_id( "ACT_MULTIPLE_BUTCHER" ), translate_marker( "Butchering" ) },
    { activity_id( "ACT_MULTIPLE_FARM" ), translate_marker( "Farming" ) },
    { activity_id( "ACT_FETCH_REQUIRED" ), translate_marker( "Fetching item" ) },
    { activity_id( "ACT_BUILD" ), translate_marker( "Building" ) },
    { activity_id( "ACT_EAT_MENU" ), translate_marker( "Eating" ) },
    { activity_id( "ACT_VEHICLE_DECONSTRUCTION" ), translate_marker( "Deconstructing" ) },
    { activity_id( "ACT_VEHICLE_REPAIR" ), translate_marker( "Repairing" ) },
    { activity_id( "ACT_MULTIPLE_CHOP_TREES" ), translate_marker( "Chopping tree" ) },
    { activity_id( "ACT_CONSUME_FOOD_MENU" ), translate_marker( "Consuming food" ) },
    { activity_id( "ACT_CONSUME_DRINK_MENU" ), translate_marker( "Consuming drinks" ) },
    { activity_id( "ACT_CONSUME_MEDS_MENU" ), translate_marker( "Consuming meds" ) },
    { activity_id( "ACT_MOVE_ITEMS" ), translate_marker( "Moving items" ) },
    { activity_id( "ACT_MOVE_LOOT" ), translate_marker( "Moving loot" ) },
    { activity_id( "ACT_ARMOR_LAYERS" ), translate_marker( "Layering Armor" ) },
    { activity_id( "ACT_ATM" ), translate_marker( "Handling ATM" ) },
    { activity_id( "ACT_CRACKING" ), translate_marker( "Cracking safe" ) },
    { activity_id( "ACT_FISH" ), translate_marker( "Fishing" ) },
    { activity_id( "ACT_REPAIR_ITEM" ), translate_marker( "Repairing item" ) },
    { activity_id( "ACT_BUTCHER" ), translate_marker( "Butchering" ) },
    { activity_id( "ACT_BUTCHER_FULL" ), translate_marker( "Fully butchering" ) },
    { activity_id( "ACT_TRAVELLING" ), translate_marker( "Traveling" ) },
    { activity_id( "ACT_AUTODRIVE" ), translate_marker( "Driving" ) },
    { activity_id( "ACT_CHURN" ), translate_marker( "Churning" ) },
    { activity_id( "ACT_FIELD_DRESS" ), translate_marker( "Fielddressing" ) },
    { activity_id( "ACT_SKIN" ), translate_marker( "Skinning" ) },
    { activity_id( "ACT_QUARTER" ), translate_marker( "Quartering" ) },
    { activity_id( "ACT_DISMEMBER" ), translate_marker( "Dismembering" ) },
    { activity_id( "ACT_DISSECT" ), translate_marker( "Dissecting" ) },
    { activity_id( "ACT_HACKSAW" ), translate_marker( "Hacksawing" ) },
    { activity_id( "ACT_CHOP_TREE" ), translate_marker( "Chopping tree" ) },
    { activity_id( "ACT_CHOP_LOGS" ), translate_marker( "Chopping log" ) },
    { activity_id( "ACT_TIDY_UP" ), translate_marker( "Tidying up" ) },
    { activity_id( "ACT_CHOP_PLANKS" ), translate_marker( "Chopping plank" ) },
    { activity_id( "ACT_JACKHAMMER" ), translate_marker( "Jackhammering" ) },
    { activity_id( "ACT_FIND_MOUNT" ), translate_marker( "Finding mount" ) },
    { activity_id( "ACT_DIG" ), translate_marker( "Digging" ) },
    { activity_id( "ACT_DIG_CHANNEL" ), translate_marker( "Digging channel" ) },
    { activity_id( "ACT_FILL_PIT" ), translate_marker( "Filling pit" ) },
    { activity_id( "ACT_MULTIPLE_CHOP_PLANKS" ), translate_marker( "Chopping plank" ) },
    { activity_id( "ACT_TILL_PLOT" ), translate_marker( "Tilling plot" ) },
    { activity_id( "ACT_HARVEST_PLOT" ), translate_marker( "Harvesting plot" ) },
    { activity_id( "ACT_PLANT_PLOT" ), translate_marker( "Planting plot" ) },
    { activity_id( "ACT_FERTILIZE_PLOT" ), translate_marker( "Fertilizing plot" ) },
    { activity_id( "ACT_TRY_SLEEP" ), translate_marker( "Trying to sleep" ) },
    { activity_id( "ACT_OPERATION" ), translate_marker( "Operating" ) },
    { activity_id( "ACT_ROBOT_CONTROL" ), translate_marker( "Controlling robot" ) },
    { activity_id( "ACT_TREE_COMMUNION" ), translate_marker( "Tree communion" ) },
    { activity_id( "ACT_STUDY_SPELL" ), translate_marker( "Studying spell" )},
    { activity_id( "ACT_READ" ), translate_marker( "Reading" )},
    { activity_id( "ACT_WAIT_STAMINA" ), translate_marker( "Catching breath" ) }
};

player_activity::player_activity() : type( activity_id::NULL_ID() ) { }

player_activity::player_activity( activity_id t, int turns, int Index, int pos,
                                  const std::string &name_in ) :
    type( t ), moves_total( turns ), moves_left( turns ),
    index( Index ),
    position( pos ), name( name_in ),
    placement( tripoint_min ), auto_resume( false )
{
}

void player_activity::set_to_null()
{
    type = activity_id::NULL_ID();
    sfx::end_activity_sounds(); // kill activity sounds when activity is nullified
}

bool player_activity::rooted() const
{
    return type->rooted();
}

std::string player_activity::get_stop_phrase() const
{
    return type->stop_phrase();
}

const translation &player_activity::get_verb() const
{
    return type->verb();
}

int player_activity::get_value( size_t index, int def ) const
{
    return index < values.size() ? values[index] : def;
}

bool player_activity::is_suspendable() const
{
    return type->suspendable();
}

bool player_activity::is_multi_type() const
{
    return type->multi_activity();
}

std::string player_activity::get_str_value( size_t index, const std::string &def ) const
{
    return index < str_values.size() ? str_values[index] : def;
}

cata::optional<std::string> player_activity::get_progress_message() const
{
    const auto iter = activity_names.find( type );
    if( iter == activity_names.end() ) {
        return cata::optional<std::string>();
    }

    std::string extra_info = "";
    if( type == activity_id( "ACT_CRAFT" ) ) {
        if( const item *craft = targets.front().get_item() ) {
            extra_info = craft->tname();
        }
    } else if( moves_total > 0 ) {
        const int percentage = ( ( moves_total - moves_left ) * 100 ) / moves_total;

        if( type == activity_id( "ACT_BURROW" ) ||
            type == activity_id( "ACT_HACKSAW" ) ||
            type == activity_id( "ACT_JACKHAMMER" ) ||
            type == activity_id( "ACT_PICKAXE" ) ||
            type == activity_id( "ACT_DISASSEMBLE" ) ||
            type == activity_id( "ACT_FILL_PIT" ) ||
            type == activity_id( "ACT_DIG" ) ||
            type == activity_id( "ACT_DIG_CHANNEL" ) ||
            type == activity_id( "ACT_CHOP_TREE" ) ||
            type == activity_id( "ACT_CHOP_LOGS" ) ||
            type == activity_id( "ACT_CHOP_PLANKS" )
          ) {
            extra_info = string_format( "%d%%", percentage );
        }
    }

    return string_format( "%s%s", _( iter->second ), extra_info.empty() ? "…" : ": " + extra_info );
}

void player_activity::do_turn( player &p )
{
    // Should happen before activity or it may fail du to 0 moves
    if( *this && type->will_refuel_fires() ) {
        try_fuel_fire( *this, p );
    }

    if( type->based_on() == based_on_type::TIME ) {
        moves_left -= 100;
    } else if( type->based_on() == based_on_type::SPEED ) {
        if( p.moves <= moves_left ) {
            moves_left -= p.moves;
            p.moves = 0;
        } else {
            p.moves -= moves_left;
            moves_left = 0;
        }
    }
    int previous_stamina = p.stamina;
    // This might finish the activity (set it to null)
    type->call_do_turn( this, &p );

    // Activities should never excessively drain stamina.
    if( p.stamina < previous_stamina && p.stamina < p.get_stamina_max() / 3 ) {
        if( one_in( 50 ) ) {
            p.add_msg_if_player( _( "You pause for a moment to catch your breath." ) );
        }
        auto_resume = true;
        player_activity new_act( activity_id( "ACT_WAIT_STAMINA" ), to_moves<int>( 1_minutes ) );
        new_act.values.push_back( 200 + p.get_stamina_max() / 3 );
        p.assign_activity( new_act );
        return;
    }
    if( *this && type->rooted() ) {
        p.rooted();
        p.pause();
    }

    if( *this && moves_left <= 0 ) {
        // Note: For some activities "finish" is a misnomer; that's why we explicitly check if the
        // type is ACT_NULL below.
        if( !type->call_finish( this, &p ) ) {
            // "Finish" is never a misnomer for any activity without a finish function
            set_to_null();
        }
    }
    if( !*this ) {
        // Make sure data of previous activity is cleared
        p.activity = player_activity();
        p.resume_backlog_activity();

        // If whatever activity we were doing forced us to pick something up to
        // handle it, drop any overflow that may have caused
        p.drop_invalid_inventory();
    }
}

template <typename T>
bool containers_equal( const T &left, const T &right )
{
    if( left.size() != right.size() ) {
        return false;
    }

    return std::equal( left.begin(), left.end(), right.begin() );
}

bool player_activity::can_resume_with( const player_activity &other, const Character & ) const
{
    // Should be used for relative positions
    // And to forbid resuming now-invalid crafting

    // TODO: Once activity_handler_actors exist, the less ugly method of using a
    // pure virtual can_resume_with should be used

    if( !*this || !other || type->no_resume() ) {
        return false;
    }

    if( id() == activity_id( "ACT_CLEAR_RUBBLE" ) ) {
        if( other.coords.empty() || other.coords[0] != coords[0] ) {
            return false;
        }
    } else if( id() == activity_id( "ACT_READ" ) ) {
        // Return false if any NPCs joined or left the study session
        // the vector {1, 2} != {2, 1}, so we'll have to check manually
        if( values.size() != other.values.size() ) {
            return false;
        }
        for( int foo : other.values ) {
            if( std::find( values.begin(), values.end(), foo ) == values.end() ) {
                return false;
            }
        }
        if( targets.empty() || other.targets.empty() || targets[0] != other.targets[0] ) {
            return false;
        }
    } else if( id() == activity_id( "ACT_DIG" ) || id() == activity_id( "ACT_DIG_CHANNEL" ) ) {
        // We must be digging in the same location.
        if( placement != other.placement ) {
            return false;
        }

        // And all our parameters must be the same.
        if( !std::equal( values.begin(), values.end(), other.values.begin() ) ) {
            return false;
        }

        if( !std::equal( str_values.begin(), str_values.end(), other.str_values.begin() ) ) {
            return false;
        }

        if( !std::equal( coords.begin(), coords.end(), other.coords.begin() ) ) {
            return false;
        }
    }

    return !auto_resume && id() == other.id() && index == other.index &&
           position == other.position && name == other.name && targets == other.targets;
}

bool player_activity::is_distraction_ignored( distraction_type type ) const
{
    return ignored_distractions.find( type ) != ignored_distractions.end();
}

void player_activity::ignore_distraction( distraction_type type )
{
    ignored_distractions.emplace( type );
}

void player_activity::allow_distractions()
{
    ignored_distractions.clear();
}

void player_activity::inherit_distractions( const player_activity &other )
{
    for( auto &type : other.ignored_distractions ) {
        ignore_distraction( type );
    }
}
