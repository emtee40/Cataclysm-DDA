#ifndef CATA_SRC_ADVUILIST_HELPERS_H
#define CATA_SRC_ADVUILIST_HELPERS_H

#include <cstddef>    // for size_t
#include <functional> // for function
#include <string>     // for string, allocator
#include <utility>    // for pair
#include <vector>     // for vector

#include "advuilist.h"         // for advuilist
#include "advuilist_sourced.h" // for advuilist_sourced
#include "item_location.h"     // for item_location
#include "transaction_ui.h"    // for transaction_ui
#include "units_fwd.h"         // for mass, volume

class Character;
class item;
class map_cursor;
class vehicle_cursor;
class map_stack;
class vehicle_stack;
struct tripoint;
namespace catacurses
{
class window;
} // namespace catacurses

namespace advuilist_helpers
{
/// entry type for advuilist based on item_location
struct iloc_entry {
    // entries are stacks of items
    using stack_t = std::vector<item_location>;
    stack_t stack;
};

using iloc_stack_t = std::vector<iloc_entry>;
using aim_container_t = std::vector<iloc_entry>;
using aim_advuilist_t = advuilist<aim_container_t, iloc_entry>;
using aim_advuilist_sourced_t = advuilist_sourced<aim_container_t, iloc_entry>;
using aim_transaction_ui_t = transaction_ui<aim_container_t, iloc_entry>;
using aim_stats_t = std::pair<units::mass, units::volume>;

constexpr auto const aim_nsources = 18;
constexpr point const aimlayout( 6, 3 );

constexpr auto const SOURCE_ALL = "Surrounding area";
constexpr auto const SOURCE_ALL_i = 'A';
constexpr auto const SOURCE_CENTER = "Directly below you";
constexpr auto const SOURCE_CENTER_i = '5';
constexpr auto const SOURCE_CONT = "Container";
constexpr auto const SOURCE_CONT_i = 'C';
constexpr auto const SOURCE_DRAGGED = "Grabbed Vehicle";
constexpr auto const SOURCE_DRAGGED_i = 'D';
constexpr auto const SOURCE_E = "East";
constexpr auto const SOURCE_E_i = '6';
constexpr auto const SOURCE_INV = "Inventory";
constexpr auto const SOURCE_INV_i = 'I';
constexpr auto const SOURCE_N = "North";
constexpr auto const SOURCE_N_i = '8';
constexpr auto const SOURCE_NE = "North East";
constexpr auto const SOURCE_NE_i = '9';
constexpr auto const SOURCE_NW = "North West";
constexpr auto const SOURCE_NW_i = '7';
constexpr auto const SOURCE_S = "South";
constexpr auto const SOURCE_S_i = '2';
constexpr auto const SOURCE_SE = "South East";
constexpr auto const SOURCE_SE_i = '3';
constexpr auto const SOURCE_SW = "South West";
constexpr auto const SOURCE_SW_i = '1';
constexpr auto const SOURCE_W = "West";
constexpr auto const SOURCE_W_i = '4';
constexpr auto const SOURCE_WORN = "Worn Items";
constexpr auto const SOURCE_WORN_i = 'W';
constexpr auto const SOURCE_VEHICLE = "Vehicle";
constexpr auto const SOURCE_VEHICLE_i = 'V';

using pane_mutex_t = std::vector<bool>;
void reset_mutex( aim_transaction_ui_t *ui, pane_mutex_t *mutex );

using filoc_t = std::function<item_location( item * )>;
item_location iloc_map_cursor( map_cursor const &cursor, item *it );
item_location iloc_tripoint( tripoint const &loc, item *it );
item_location iloc_character( Character *guy, item *it );
item_location iloc_vehicle( vehicle_cursor const &cursor, item *it );
template <class Iterable>
iloc_stack_t get_stacks( Iterable items, filoc_t const &iloc_helper );

std::size_t iloc_entry_counter( iloc_entry const &it );
std::string iloc_entry_count( iloc_entry const &it );
std::string iloc_entry_weight( iloc_entry const &it );
std::string iloc_entry_volume( iloc_entry const &it );
std::string iloc_entry_name( iloc_entry const &it );

bool iloc_entry_count_sorter( iloc_entry const &l, iloc_entry const &r );
bool iloc_entry_weight_sorter( iloc_entry const &l, iloc_entry const &r );
bool iloc_entry_volume_sorter( iloc_entry const &l, iloc_entry const &r );
bool iloc_entry_damage_sorter( iloc_entry const &l, iloc_entry const &r );
bool iloc_entry_spoilage_sorter( iloc_entry const &l, iloc_entry const &r );
bool iloc_entry_price_sorter( iloc_entry const &l, iloc_entry const &r );
bool iloc_entry_name_sorter( iloc_entry const &l, iloc_entry const &r );

bool iloc_entry_gsort( iloc_entry const &l, iloc_entry const &r );
std::string iloc_entry_glabel( iloc_entry const &it );

bool iloc_entry_filter( iloc_entry const &it, std::string const &filter );

void iloc_entry_stats( aim_stats_t *stats, bool first, advuilist_helpers::iloc_entry const &it );
void iloc_entry_stats_printer( aim_stats_t *stats, catacurses::window *w );

void iloc_entry_examine( catacurses::window *w, iloc_entry const &it );

aim_container_t source_ground_all( Character *guy, int radius );
aim_container_t source_ground( tripoint const &loc );
aim_container_t source_vehicle( tripoint const &loc );
aim_container_t source_char_inv( Character *guy );
aim_container_t source_char_worn( Character *guy );

bool source_vehicle_avail( tripoint const &loc );

void setup_for_aim( aim_advuilist_t *myadvuilist, aim_stats_t *stats );
void add_aim_sources( aim_advuilist_sourced_t *myadvuilist, pane_mutex_t const *mutex );
void aim_add_return_activity();
void aim_transfer( aim_transaction_ui_t *ui, aim_transaction_ui_t::select_t select );
void aim_ctxthandler( aim_transaction_ui_t *ui, std::string const &action, pane_mutex_t *mutex );

// for map::i_at()
extern template iloc_stack_t get_stacks<>( map_stack items, filoc_t const &iloc_helper );
// for vehicle::get_items()
extern template iloc_stack_t get_stacks<>( vehicle_stack items, filoc_t const &iloc_helper );

} // namespace advuilist_helpers

#endif
