#include <string>
#include <vector>

#include "cata_catch.h"
#include "cata_utility.h"
#include "mongroup.h"
#include "options.h"
#include "options_helpers.h"

static const mtype_id mon_test_CBM( "mon_test_CBM" );
static const mtype_id mon_test_bovine( "mon_test_bovine" );
static const mtype_id mon_test_non_shearable( "mon_test_non_shearable" );
static const mtype_id mon_test_shearable( "mon_test_shearable" );
static const mtype_id mon_test_speed_desc_base( "mon_test_speed_desc_base" );
static const mtype_id mon_test_speed_desc_base_immobile( "mon_test_speed_desc_base_immobile" );

static const int max_iters = 10000;

static void spawn_x_monsters( int x, const mongroup_id &grp, const std::vector<mtype_id> yesspawn, const std::vector<mtype_id> &nospawn )
{
    std::set<mtype_id> rand_gets;
    std::set<mtype_id> rand_results;
    calendar::turn = time_point( 1 );
    for( int i = 0; i < x; i++ ) {
        rand_gets.emplace( MonsterGroupManager::GetRandomMonsterFromGroup( grp ) );
        rand_results.emplace( MonsterGroupManager::GetResultFromGroup( grp ).name );
    }
    for( const mtype_id &m : nospawn ) {
        CHECK( rand_gets.find( m ) == rand_gets.end() );
        CHECK( rand_results.find( m ) == rand_results.end() );
    }
    for( const mtype_id &m : yesspawn ) {
        CHECK( rand_gets.find( m ) != rand_gets.end() );
        CHECK( rand_results.find( m ) != rand_results.end() );
    }
}

TEST_CASE( "Event-based monster spawns do not spawn outside event", "[monster][mongroup]" )
{
    override_option ev_spawn_opt( "EVENT_SPAWNS", "monsters" );
    REQUIRE( get_option<std::string>( "EVENT_SPAWNS" ) == "monsters" );

    GIVEN( "a monster group with entries from different events" ) {
        mongroup_id test_group( "test_event_mongroup" );
        REQUIRE( test_group->monsters.size() == 6 );

        WHEN( "current holiday == none" ) {
            holiday cur_event = get_holiday_from_time( 1614574800L, true );
            THEN( "only spawn non-event monsters" ) {
                std::vector<mtype_id> mons = MonsterGroupManager::GetMonstersFromGroup( test_group, true );
                CHECK( mons.size() == 1 );
                CHECK( mons[0] == mon_test_speed_desc_base_immobile );

                const std::vector<mtype_id> ylist = { mon_test_speed_desc_base_immobile };
                const std::vector<mtype_id> nlist = { mon_test_speed_desc_base, mon_test_shearable, mon_test_non_shearable, mon_test_bovine, mon_test_CBM };
                spawn_x_monsters( max_iters, test_group, ylist, nlist );
            }
        }

        WHEN( "current holiday == christmas" ) {
            holiday cur_event = get_holiday_from_time( 1640408400L, true );
            THEN( "only spawn christmas and non-event monsters" ) {
                std::vector<mtype_id> mons = MonsterGroupManager::GetMonstersFromGroup( test_group, true );
                CHECK( mons.size() == 4 );

                const std::vector<mtype_id> ylist = { mon_test_speed_desc_base_immobile, mon_test_non_shearable, mon_test_shearable, mon_test_bovine };
                const std::vector<mtype_id> nlist = { mon_test_speed_desc_base, mon_test_CBM };
                spawn_x_monsters( max_iters, test_group, ylist, nlist );
            }
        }

        WHEN( "current holiday == halloween" ) {
            holiday cur_event = get_holiday_from_time( 1635652800L, true );
            THEN( "only spawn halloween and non-event monsters" ) {
                std::vector<mtype_id> mons = MonsterGroupManager::GetMonstersFromGroup( test_group, true );
                CHECK( mons.size() == 3 );

                const std::vector<mtype_id> ylist = { mon_test_speed_desc_base_immobile, mon_test_CBM, mon_test_speed_desc_base };
                const std::vector<mtype_id> nlist = { mon_test_shearable, mon_test_non_shearable, mon_test_bovine };
                spawn_x_monsters( max_iters, test_group, ylist, nlist );
            }
        }
    }
}