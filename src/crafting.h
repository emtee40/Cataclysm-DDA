#pragma once
#ifndef CRAFTING_H
#define CRAFTING_H

#include <list>

class Character;
class item;
class player;
class recipe;

enum class craft_flags : int {
    none = 0,
    start_only = 1, // Only require 5% (plus remainder) of tool charges
};

inline constexpr craft_flags operator&( craft_flags l, craft_flags r )
{
    return static_cast<craft_flags>( static_cast<unsigned>( l ) & static_cast<unsigned>( r ) );
}

// removes any (removable) ammo from the item and stores it in the
// players inventory.
void remove_ammo( item &dis_item, player &p );
// same as above but for each item in the list
void remove_ammo( std::list<item> &dis_items, player &p );

const recipe *select_crafting_recipe( int &batch_size );
void drop_or_handle( const item &newit, Character &p );

void drop_or_handle( const item &newit, Character &p );

#endif
