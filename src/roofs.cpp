#include "roofs.h"
#include "map.h"
#include "game.h"
#include "mapdata.h"
#include <array>
#include <algorithm>

constexpr size_t padded_map_size = ( SEEX + 2 ) * MAPSIZE * ( SEEY + 2 ) * MAPSIZE;

enum class dir_enum : size_t {
    west = 0,
    north,
    east,
    south,
    num_dirs
};

dir_enum opposite_dir( dir_enum dir )
{
    switch( dir ) {
        case dir_enum::west:
            return dir_enum::east;
        case dir_enum::north:
            return dir_enum::south;
        case dir_enum::east:
            return dir_enum::west;
        case dir_enum::south:
            return dir_enum::north;
        case dir_enum::num_dirs:
            return dir_enum::num_dirs;
    }
    return dir_enum::num_dirs;
}

dir_enum convert_directions( direction dir )
{
    switch( dir ) {
        case NORTH:
            return dir_enum::north;
        case EAST:
            return dir_enum::east;
        case SOUTH:
            return dir_enum::south;
        case WEST:
            return dir_enum::west;
        default:
            break;
    }
    return dir_enum::num_dirs;
}

struct cached_tile {
    bool dirty = true;
    ter_id ter = t_null;
    bool ter_dirty = true;
    std::array<int, ( size_t )dir_enum::num_dirs> support = {{ }};
    std::array<bool, ( size_t )dir_enum::num_dirs> dir_support_dirty = {{ true, true, true, true }};
    int final_support_val = -1;
};

struct support_cache {
    std::array<cached_tile, padded_map_size> support;
    support_cache() {
        support.fill( cached_tile() );
    }
    inline bool inbounds( const tripoint &p ) const {
        return p.x >= 0 && p.y >= 0 && p.x < SEEX * MAPSIZE && p.y < SEEY * MAPSIZE;
    }
    inline size_t w() const {
        return SEEX * MAPSIZE;
    }
    inline size_t h() const {
        return SEEY * MAPSIZE;
    }
    inline size_t y_stride() const {
        // 2 is padding
        return w() + 2;
    }
    inline size_t get_index( size_t x, size_t y ) {
        return y_stride() * ( y + 1 ) + ( x + 1 );
    }
    inline size_t get_index( const tripoint &p ) {
        return get_index( p.x, p.y );
    }
    inline cached_tile &get( const tripoint &p ) {
        return get( p.x, p.y );
    }
    inline cached_tile &get( size_t x, size_t y ) {
        return support[ get_index( x, y ) ];
    }
    inline cached_tile &get_clean( const tripoint &p, const map &m ) {
        cached_tile &t = get( p );
        if( !t.ter_dirty ) {
            return t;
        }
        t.ter = m.ter( p );
        t.ter_dirty = false;
        return t;
    }
};

static const std::array<tripoint, ( size_t )dir_enum::num_dirs> dir_offsets = {{
        tripoint( -1, 0, 0 ),
        tripoint( 0, -1, 0 ),
        tripoint( 1, 0, 0 ),
        tripoint( 0, 1, 0 )
    }
};

roofs::roofs( game &gm, map &mp ) : g( gm ), m( mp )
{
    cache.reset( new support_cache() );
}

roofs::~roofs()
{
}

int roofs::get_support( const tripoint &origin )
{
    if( !cache->inbounds( origin ) ) {
        return -1;
    }

    if( cache->get( origin ).dirty ) {
        calc_support( origin );
    }

    return cache->get( origin ).final_support_val;
}

int roofs::get_support( const tripoint &origin, direction from_direction )
{
    // The direction enum covers all directions, our small enum only 4
    dir_enum dir = convert_directions( from_direction );
    if( dir == dir_enum::num_dirs ) {
        return -1;
    }

    if( !cache->inbounds( origin ) ) {
        return -1;
    }

    if( cache->get( origin ).dirty ) {
        calc_support( origin );
    }

    return cache->get( origin ).support[( size_t )dir ];
}

// For every direction:
// Go in that direction to find tile that supports us (if any)
// Go back from that tile to us
//
// Every tile has limited carrying capacity
// Support has directions (orthogonal)
// Tiles that can't collapse also support from all directions
// Tiles that can collapse only support if they are supported from given direction
roofs &roofs::calc_support( const tripoint &origin )
{
    support_cache &ch = *cache;
    if( !ch.inbounds( origin ) ) {
        // Debugmsg?
        return *this;
    }

    if( !ch.get( origin ).dirty ) {
        return *this;
    }

    auto &origin_tile = ch.get_clean( origin, m );
    const auto &origin_ter = origin_tile.ter.obj();
    if( !origin_ter.has_flag( TFLAG_COLLAPSES ) ) {
        origin_tile.final_support_val = origin_ter.max_support;
        origin_tile.dirty = false;
        return *this;
    }
    for( size_t i = 0; i < ( size_t )dir_enum::num_dirs; i++ ) {
        if( !origin_tile.dir_support_dirty[ i ] ) {
            continue;
        }

        const tripoint &doff = dir_offsets[ i ];
        tripoint cur_pos = origin;
        // Weight of all roofs up from this one to first proper support
        int cur_weight = ch.get_clean( cur_pos, m ).ter.obj().weight;
        // The least-supporting roof in the chain
        // If weight exceeds that, there is no way this roof is supported
        int weakest_link = INT_MAX;
        // This is what we want to find here
        int cur_support = 0;
        // First we find the supporting tile in given direction
        while( ch.inbounds( cur_pos ) ) {
            cur_pos += doff;
            cached_tile &t = ch.get_clean( cur_pos, m );
            const auto &ter_here = t.ter.obj();
            int support_here = ter_here.max_support;
            if( support_here <= 0 ) {
                break;
            }
            if( ter_here.has_flag( TFLAG_COLLAPSES ) ) {
                // Collapsing tiles can only support from a direction if they are supported from it
                cur_weight += ter_here.weight;
                weakest_link = std::min( weakest_link, support_here );
            } else if( ter_here.has_flag( TFLAG_SUPPORTS_ROOF ) ) {
                // Currently every true support can only transfer as much support as it provides
                cur_support = support_here;
                break;
            } else {
                break;
            }
            // The weight of line of tiles is greater than this tile's capacity
            // This means that even if it was supported by
            // a nintendium-Dragonforce alloy column on the other side, it still can't lift this side
            if( cur_weight > weakest_link ) {
                break;
            }
        }

        // Then we pass it back until it reaches us again
        const tripoint dopp = -doff;
        while( ch.inbounds( cur_pos ) && cur_pos != origin ) {
            cur_pos += dopp;
            cached_tile &t = ch.get_clean( cur_pos, m );
            const auto &ter_here = t.ter.obj();
            if( ter_here.has_flag( TFLAG_COLLAPSES ) ) {
                cur_support = std::max( 0, cur_support - ter_here.weight );
            } else if( ter_here.has_flag( TFLAG_SUPPORTS_ROOF ) ) {
                // This shouldn't happen, but it's easy to handle
                cur_support = ter_here.max_support;
            }

            t.support[ i ] = cur_support;
            t.dir_support_dirty[ i ] = false;
        }
    }

    // Now that all directions have support calculated, pick the... second best support as final value
    // Second best == worse of the 2 best values == at least 2 supports provide this much value
    auto supp_arr_copy = origin_tile.support;
    std::sort( supp_arr_copy.begin(), supp_arr_copy.end() );
    origin_tile.final_support_val = supp_arr_copy[ supp_arr_copy.size() - 2 ];
    origin_tile.dirty = false;
    return *this;
}

roofs &roofs::replace_tiles( const std::map<tripoint, ter_id> &replacement )
{
    std::set<size_t> indices_x;
    std::set<size_t> indices_y;

    support_cache &ch = *cache;
    for( const std::pair<tripoint, ter_id> &pr : replacement ) {
        if( !ch.inbounds( pr.first ) ) {
            continue;
        }

        cached_tile &t = ch.get( pr.first );
        t.ter_dirty = false;
        t.ter = pr.second;
        indices_x.insert( pr.first.x );
        indices_y.insert( pr.first.y );
    }

    // For simplicity, just dirty whole lines of coords
    // This is slow and ugly, but better than a giant bulk of code that would do it better
    // The performance isn't required here since it's just few hundreds of tiles at most
    for( size_t x : indices_x ) {
        for( size_t y = 0; y < ch.h(); y++ ) {
            ch.get( x, y ).dirty = true;
            ch.get( x, y ).dir_support_dirty.fill( true );
        }
    }
    for( size_t y : indices_y ) {
        for( size_t x = 0; x < ch.w(); x++ ) {
            ch.get( x, y ).dirty = true;
            ch.get( x, y ).dir_support_dirty.fill( true );
        }
    }

    return *this;
}