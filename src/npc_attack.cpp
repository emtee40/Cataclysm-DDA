#include "npc_attack.h"

#include "character.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "messages.h"
#include "npc.h"
#include "point.h"
#include "projectile.h"
#include "ranged.h"

namespace npc_attack_constants
{
const std::map<Creature::Attitude, int> attitude_multiplier = {
    { Creature::Attitude::FRIENDLY, -10 },
    { Creature::Attitude::HOSTILE, 3 },
    { Creature::Attitude::NEUTRAL, -1 }
};
// if you are attacking your target, multiply potential by this number
const float target_modifier = 1.5f;
// if you kill the creature, multiply the potential by this number
const float kill_modifier = 1.5f;
// the amount of penalty if the npc has to change what it's wielding
// update this number and comment when that is no longer a flat -15 moves
const int base_time_penalty = 3;
// we want this out of our hands, pronto.
// give a large buff to the attack value so it prioritizes this
const int base_throw_now = 10'000;
} // namespace npc_attack_constants

// TODO: make a better, more generic "check if this projectile is blocked" function
// TODO: put this in a namespace for reuse
static bool has_obstruction( const tripoint &from, const tripoint &to, bool check_ally = false )
{
    std::vector<tripoint> line = line_to( from, to );
    // @to is what we want to hit. we don't need to check for obstruction there.
    line.pop_back();
    const map &here = get_map();
    for( const tripoint &line_point : line ) {
        if( check_ally && g->critter_at( line_point ) ) {
            return true;
        } else if( here.impassable( line_point ) ) {
            return true;
        }
    }
    return false;
}

static bool can_move( const npc &source )
{
    return !source.in_vehicle && source.rules.engagement != combat_engagement::NO_MOVE;
}

static bool can_move_melee( const npc &source )
{
    return can_move( source ) && source.rules.engagement != combat_engagement::FREE_FIRE;
}

bool npc_attack_rating::operator>( const npc_attack_rating &rhs ) const
{
    if( !rhs._value ) {
        return true;
    }
    if( !_value ) {
        return false;
    }
    return *_value > *rhs._value;
}

bool npc_attack_rating::operator>( const int rhs ) const
{
    if( !_value ) {
        return false;
    }
    return *_value > rhs;
}

bool npc_attack_rating::operator<( const int rhs ) const
{
    if( !_value ) {
        return true;
    }
    return *_value < rhs;
}

npc_attack_rating npc_attack_rating::operator-=( const int rhs )
{
    if( _value ) {
        *_value -= rhs;
    }
    return *this;
}

void npc_attack_melee::use( npc &source, const tripoint &location ) const
{
    if( !source.is_wielding( weapon ) ) {
        if( !source.wield( weapon ) ) {
            debugmsg( "ERROR: npc tried to equip a weapon it couldn't wield" );
        }
        return;
    }
    Creature *critter = g->critter_at( location );
    if( !critter ) {
        debugmsg( "ERROR: npc tried to attack null critter" );
        return;
    }
    if( !source.is_adjacent( critter, true ) ) {
        if( rl_dist( source.pos(), location ) <= weapon.reach_range( source ) ) {
            add_msg_debug( debugmode::debug_filter::DF_NPC, "%s is attempting a reach attack",
                           source.disp_name() );
            // TODO: Avoid friendly fire
            source.reach_attack( location );
        } else {
            source.update_path( location );
            if( source.path.size() > 1 ) {
                if( can_move_melee( source ) ) {
                    source.move_to_next();
                } else {
                    source.look_for_player( get_player_character() );
                }
            } else if( source.path.size() == 1 ) {
                if( critter != nullptr ) {
                    if( source.can_use_offensive_cbm() ) {
                        source.activate_bionic_by_id( bionic_id( "bio_hydraulics" ) );
                    }
                    add_msg_debug( debugmode::debug_filter::DF_NPC, "%s is attempting a melee attack",
                                   source.disp_name() );
                    source.melee_attack( *critter, true );
                }
            } else {
                source.look_for_player( get_player_character() );
            }
        }
    } else {
        add_msg_debug( debugmode::debug_filter::DF_NPC, "%s is attempting a melee attack",
                       source.disp_name() );
        source.melee_attack( *critter, true );
    }
}

tripoint_range<tripoint> npc_attack_melee::targetable_points( const npc &source ) const
{
    return get_map().points_in_radius( source.pos(), 8 );
}

npc_attack_rating npc_attack_melee::evaluate( const npc &source,
        const Creature *target ) const
{
    npc_attack_rating effectiveness( cata::nullopt, source.pos() );
    if( !can_use( source ) ) {
        return effectiveness;
    }
    const int time_penalty = base_time_penalty( source );
    for( const tripoint &targetable_point : targetable_points( source ) ) {
        if( Creature *critter = g->critter_at( targetable_point ) ) {
            if( source.attitude_to( *critter ) != Creature::Attitude::HOSTILE ) {
                // no point in swinging a sword at a friendly!
                continue;
            }
            npc_attack_rating effectiveness_at_point = evaluate_critter( source, target, critter );
            effectiveness_at_point -= time_penalty;
            if( effectiveness_at_point > effectiveness ) {
                effectiveness = effectiveness_at_point;
            }
        }
    }
    return effectiveness;
}

std::vector<npc_attack_rating> npc_attack_melee::all_evaluations( const npc &source,
        const Creature *target ) const
{
    std::vector<npc_attack_rating> effectiveness;
    if( !can_use( source ) ) {
        return effectiveness;
    }
    const int time_penalty = base_time_penalty( source );
    for( const tripoint &targetable_point : targetable_points( source ) ) {
        if( Creature *critter = g->critter_at( targetable_point ) ) {
            if( source.attitude_to( *critter ) != Creature::Attitude::HOSTILE ) {
                // no point in swinging a sword at a friendly!
                continue;
            }
            npc_attack_rating effectiveness_at_point = evaluate_critter( source, target, critter );
            effectiveness_at_point -= time_penalty;
            effectiveness.push_back( effectiveness_at_point );
        }
    }
    return effectiveness;
}

bool npc_attack_melee::can_use( const npc &source ) const
{
    // can't attack with something you can't wield
    return source.can_wield( weapon ).success();
}

int npc_attack_melee::base_time_penalty( const npc &source ) const
{
    return source.is_wielding( weapon ) ? 0 : npc_attack_constants::base_time_penalty;
}

npc_attack_rating npc_attack_melee::evaluate_critter( const npc &source,
        const Creature *target, Creature *critter ) const
{
    if( !critter ) {
        return npc_attack_rating{};
    }
    const int distance_to_me = rl_dist( source.pos(), critter->pos() );
    const double damage{ weapon.effective_dps( source, *critter ) };
    const Creature::Attitude att = source.attitude_to( *critter );
    double potential = damage * npc_attack_constants::attitude_multiplier.at( att ) -
                       ( distance_to_me - 1 );

    const int reach_range{ weapon.reach_range( source ) };
    const int distance = rl_dist( source.pos(), critter->pos() );
    int range_penalty = std::max( 0, distance - reach_range );
    potential *= range_penalty ? 0.7f : 1.5f;

    if( damage >= critter->get_hp() ) {
        potential *= npc_attack_constants::kill_modifier;
    }
    if( target && target->pos() == critter->pos() ) {
        potential *= npc_attack_constants::target_modifier;
    }

    return npc_attack_rating( std::round( potential ), critter->pos() );
}

void npc_attack_gun::use( npc &source, const tripoint &location ) const
{
    if( !source.is_wielding( gun ) ) {
        if( !source.wield( gun ) ) {
            debugmsg( "ERROR: npc tried to equip a weapon it couldn't wield" );
        }
        return;
    }

    if( !gun.ammo_sufficient() ) {
        source.do_reload( gun );
        add_msg_debug( debugmode::debug_filter::DF_NPC, "%s is reloading %s", source.disp_name(),
                       gun.display_name() );
        return;
    }

    if( has_obstruction( source.pos(), location, false ) ||
        ( source.rules.has_flag( ally_rule::avoid_friendly_fire ) &&
          !source.wont_hit_friend( location, gun, false ) ) ) {
        if( can_move( source ) ) {
            source.avoid_friendly_fire();
        } else {
            source.move_pause();
        }
        return;
    }

    const int dist = rl_dist( source.pos(), location );

    // Only aim if we aren't in risk of being hit
    // TODO: Get distance to closest enemy
    if( dist > 1 && source.aim_per_move( gun, source.recoil ) > 0 &&
        /*source.confident_shoot_range(gun, source.get_most_accurate_sight(gun) ) >= dist*/
        source.confident_gun_mode_range( gunmode, source.recoil ) < dist ) {
        add_msg_debug( debugmode::debug_filter::DF_NPC, "%s is aiming", source.disp_name() );
        source.aim();
    } else {
        source.fire_gun( location );
        add_msg_debug( debugmode::debug_filter::DF_NPC, "%s fires %s", source.disp_name(),
                       gun.display_name() );
    }
}

bool npc_attack_gun::can_use( const npc &source ) const
{
    // can't attack with something you can't wield
    return source.can_wield( *gunmode ).success();
}

int npc_attack_gun::base_time_penalty( const npc &source ) const
{
    const item &weapon = *gunmode;
    int time_penalty = 0;
    if( source.is_wielding( weapon ) ) {
        time_penalty += npc_attack_constants::base_time_penalty;
    }
    // we want the need to reload a gun cumulative with needing to wield the gun
    if( !weapon.ammo_sufficient() ) {
        time_penalty += npc_attack_constants::base_time_penalty;
    }
    int recoil_penalty = 0;
    if( source.is_wielding( weapon ) ) {
        recoil_penalty = source.recoil;
    } else {
        recoil_penalty = MAX_RECOIL;
    }
    recoil_penalty /= 100;
    return time_penalty + recoil_penalty;
}

tripoint_range<tripoint> npc_attack_gun::targetable_points( const npc &source ) const
{
    const item &weapon = *gunmode;
    return get_map().points_in_radius( source.pos(), weapon.gun_range() );
}

npc_attack_rating npc_attack_gun::evaluate(
    const npc &source, const Creature *target ) const
{
    npc_attack_rating effectiveness( cata::nullopt, source.pos() );
    if( !can_use( source ) ) {
        return effectiveness;
    }
    const int time_penalty = base_time_penalty( source );
    for( const tripoint &targetable_point : targetable_points( source ) ) {
        if( g->critter_at( targetable_point ) ) {
            npc_attack_rating effectiveness_at_point = evaluate_tripoint( source, target,
                    targetable_point );
            effectiveness_at_point -= time_penalty;
            if( effectiveness_at_point > effectiveness ) {
                effectiveness = effectiveness_at_point;
            }
        }
    }
    return effectiveness;
}

std::vector<npc_attack_rating> npc_attack_gun::all_evaluations( const npc &source,
        const Creature *target ) const
{
    std::vector<npc_attack_rating> effectiveness;
    if( !can_use( source ) ) {
        return effectiveness;
    }
    const int time_penalty = base_time_penalty( source );
    for( const tripoint &targetable_point : targetable_points( source ) ) {
        if( g->critter_at( targetable_point ) ) {
            npc_attack_rating effectiveness_at_point = evaluate_tripoint( source, target,
                    targetable_point );
            effectiveness_at_point -= time_penalty;
            effectiveness.push_back( effectiveness_at_point );
        }
    }
    return effectiveness;
}

npc_attack_rating npc_attack_gun::evaluate_tripoint(
    const npc &source, const Creature *target, const tripoint &location ) const
{
    const item &gun = *gunmode.target;
    const int damage = gun.gun_damage().total_damage() * gunmode.qty;
    if( has_obstruction( source.pos(), location ) ) {
        return npc_attack_rating( cata::nullopt, location );
    }

    const Creature *critter = g->critter_at( location );
    if( !critter ) {
        // TODO: AOE ammo effects
        return npc_attack_rating( cata::nullopt, location );
    }
    const int distance_to_me = rl_dist( location, source.pos() );
    const Creature::Attitude att = source.attitude_to( *critter );

    if( att != Creature::Attitude::HOSTILE ) {
        // No point in throwing stuff at neutral/friendly targets
        return npc_attack_rating( cata::nullopt, location );
    }

    bool avoids_friendly_fire = source.rules.has_flag( ally_rule::avoid_friendly_fire );

    double potential = damage;

    // Make attacks that involve moving to find clear LOS slightly less likely
    if( has_obstruction( source.pos(), location, avoids_friendly_fire ) ) {
        potential *= 0.9f;
    } else if( avoids_friendly_fire && !source.wont_hit_friend( location, gun, false ) ) {
        potential *= 0.95f;
    }

    // Prefer targets not directly next to you
    potential *= distance_to_me > 2 ? 1.15f : 0.85f;

    if( damage >= critter->get_hp() ) {
        potential *= npc_attack_constants::kill_modifier;
    }
    if( target && target->pos() == critter->pos() ) {
        potential *= npc_attack_constants::target_modifier;
    }
    return npc_attack_rating( std::round( potential ), location );
}

void npc_attack_activate_item::use( npc &source, const tripoint &/*location*/ ) const
{
    if( !source.wield( activatable_item ) ) {
        debugmsg( "%s can't wield %s it tried to activate", source.disp_name(),
                  activatable_item.display_name() );
    }
    source.activate_item( activatable_item );
}

bool npc_attack_activate_item::can_use( const npc &source ) const
{
    if( !activatable_item.has_flag( flag_NPC_ACTIVATE ) ) {
        //TODO: make this more complete. only does BOMB type items
        return false;
    }
    const bool can_use_grenades = !source.is_hallucination() && ( !source.is_player_ally() ||
                                  source.rules.has_flag( ally_rule::use_grenades ) );
    return can_use_grenades;
}

npc_attack_rating npc_attack_activate_item::evaluate(
    const npc &source, const Creature * /*target*/ ) const
{
    if( !can_use( source ) ) {
        return npc_attack_rating( cata::nullopt, source.pos() );
    }
    // until we have better logic for grenades it's better to keep this as a last resort...
    const int emergency = source.emergency() ? 1 : 0;
    return npc_attack_rating( emergency, source.pos() );
}

std::vector<npc_attack_rating> npc_attack_activate_item::all_evaluations( const npc &source,
        const Creature * /*target*/ ) const
{
    std::vector<npc_attack_rating> effectiveness;
    if( !can_use( source ) ) {
        return effectiveness;
    }
    // until we have better logic for grenades it's better to keep this as a last resort...
    const int emergency = source.emergency() ? 1 : 0;
    effectiveness.emplace_back( npc_attack_rating( emergency, source.pos() ) );
    return effectiveness;
}

void npc_attack_throw::use( npc &source, const tripoint &location ) const
{
    if( !source.is_wielding( source.weapon ) ) {
        if( !source.wield( source.weapon ) ) {
            debugmsg( "ERROR: npc tried to equip a weapon it couldn't wield" );
        }
        return;
    }

    if( has_obstruction( source.pos(), location, false ) ||
        ( source.rules.has_flag( ally_rule::avoid_friendly_fire ) &&
          !source.wont_hit_friend( location, thrown_item, false ) ) ) {
        if( can_move( source ) ) {
            source.avoid_friendly_fire();
        } else {
            source.move_pause();
        }
        return;
    }

    add_msg_debug( debugmode::debug_filter::DF_NPC, "%s throws the %s", source.disp_name(),
                   source.weapon.display_name() );
    item thrown( source.weapon );
    if( source.weapon.count_by_charges() && source.weapon.charges > 1 ) {
        source.weapon.mod_charges( -1 );
        thrown.charges = 1;
    } else {
        source.remove_weapon();
    }
    source.throw_item( location, thrown );
}

bool npc_attack_throw::can_use( const npc &source ) const
{
    item single_item( thrown_item );
    if( single_item.count_by_charges() ) {
        single_item.charges = 1;
    }
    if( thrown_item.has_flag( flag_NPC_THROW_NOW ) || thrown_item.has_flag( flag_NPC_THROWN ) ) {
        //Always allow throwing items that are flagged as throw now or npc_thrown
        return true;
    }

    bool throwable = source.throw_range( single_item ) > 0 && !source.is_worn( thrown_item ) &&
                     !thrown_item.has_flag( flag_NPC_ACTIVATE );
    throwable = throwable && !thrown_item.is_gun() && !thrown_item.is_armor() &&
                !thrown_item.is_comestible() && !thrown_item.is_magazine() && !thrown_item.is_tool() &&
                !thrown_item.is_unarmed_weapon();
    // TODO: Better choose what should be thrown
    return throwable;
}

int npc_attack_throw::base_penalty( const npc &source ) const
{
    item single_item( thrown_item );
    if( single_item.count_by_charges() ) {
        single_item.charges = 1;
    }
    const int time_penalty = source.is_wielding( single_item ) ? 0 :
                             npc_attack_constants::base_time_penalty;

    return time_penalty;
}

tripoint_range<tripoint> npc_attack_throw::targetable_points( const npc &source ) const
{
    item single_item( thrown_item );
    if( single_item.count_by_charges() ) {
        single_item.charges = 1;
    }
    const int range = source.throw_range( single_item );
    return get_map().points_in_radius( source.pos(), range );
}

npc_attack_rating npc_attack_throw::evaluate(
    const npc &source, const Creature *target ) const
{
    npc_attack_rating effectiveness( cata::nullopt, source.pos() );
    if( !can_use( source ) ) {
        // please don't throw your pants...
        return effectiveness;
    }
    const int penalty = base_penalty( source );
    // TODO: Should this be a field to cache the result?
    bool avoids_friendly_fire = source.rules.has_flag( ally_rule::avoid_friendly_fire );
    for( const tripoint &potential : targetable_points( source ) ) {

        // hot potato! HOT POTATO!
        // Calculated for all targetable points, not just those with targets
        if( thrown_item.has_flag( flag_NPC_THROW_NOW ) ) {
            // TODO: Take into account distance to allies too
            const int distance_to_me = rl_dist( potential, source.pos() );
            int result = npc_attack_constants::base_throw_now + distance_to_me;
            if( !has_obstruction( source.pos(), potential, avoids_friendly_fire ) ) {
                // More likely to pick a target tile that isn't obstructed
                result += 100;
            }
            return npc_attack_rating( result, potential );
        }

        if( Creature *critter = g->critter_at( potential ) ) {
            if( source.attitude_to( *critter ) != Creature::Attitude::HOSTILE ) {
                // no point in friendly fire!
                continue;
            }
            npc_attack_rating effectiveness_at_point = evaluate_tripoint( source, target,
                    potential );
            effectiveness_at_point -= penalty;
            if( effectiveness_at_point > effectiveness ) {
                effectiveness = effectiveness_at_point;
            }
        }
    }
    return effectiveness;
}

std::vector<npc_attack_rating> npc_attack_throw::all_evaluations( const npc &source,
        const Creature *target ) const
{
    std::vector<npc_attack_rating> effectiveness;
    if( !can_use( source ) ) {
        // please don't throw your pants...
        return effectiveness;
    }
    const int penalty = base_penalty( source );
    for( const tripoint &potential : targetable_points( source ) ) {
        if( Creature *critter = g->critter_at( potential ) ) {
            if( source.attitude_to( *critter ) != Creature::Attitude::HOSTILE ) {
                // no point in friendly fire!
                continue;
            }
            npc_attack_rating effectiveness_at_point = evaluate_tripoint( source, target,
                    potential );
            effectiveness_at_point -= penalty;
            effectiveness.push_back( effectiveness_at_point );
        }
    }
    return effectiveness;
}

npc_attack_rating npc_attack_throw::evaluate_tripoint(
    const npc &source, const Creature *target, const tripoint &location ) const
{
    if( has_obstruction( source.pos(), location ) ) {
        return npc_attack_rating( cata::nullopt, location );
    }
    item single_item( thrown_item );
    if( single_item.count_by_charges() ) {
        single_item.charges = 1;
    }

    Creature::Attitude att = Creature::Attitude::NEUTRAL;
    const Creature *critter = g->critter_at( location );
    if( critter ) {
        att = source.attitude_to( *critter );
    }

    if( att != Creature::Attitude::HOSTILE ) {
        // No point in throwing stuff at neutral/friendly targets
        return npc_attack_rating( cata::nullopt, location );
    }

    if( source.rules.has_flag( ally_rule::avoid_friendly_fire ) &&
        !source.wont_hit_friend( location, thrown_item, false ) ) {
        // Avoid friendy fire
        return npc_attack_rating( cata::nullopt, location );
    }

    const float throw_mult = throw_cost( source, single_item ) * source.speed_rating() / 100.0f;
    const int damage = source.thrown_item_total_damage_raw( single_item );
    float dps = damage / throw_mult;
    const int distance_to_me = rl_dist( location, source.pos() );
    const int distance_penalty = std::max( std::min( distance_to_me,
                                           source.closest_enemy_to_friendly_distance() ) - 3, 0 );
    const float suitable_item_mult = thrown_item.has_flag( flag_NPC_THROWN ) ? 0.2f : -0.15f;
    const float distance_mult = -distance_penalty / 10;

    double potential = dps * ( 1.0f + distance_mult + suitable_item_mult );
    if( critter && damage >= critter->get_hp() ) {
        potential *= npc_attack_constants::kill_modifier;
    }
    if( !target || !critter ) {
        // not great to throw here but if we have a grenade...
        potential = -100;
        // ... we'd rather throw it farther away from ourselves.
        potential += distance_to_me;
    } else if( target->pos() == critter->pos() ) {
        potential *= npc_attack_constants::target_modifier;
    }
    return npc_attack_rating( std::round( potential ), location );
}
