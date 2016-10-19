#include "item.h"
#include "material.h"
#include "item_search.h"
#include "cata_utility.h"

std::pair<std::string, std::string> get_both( const std::string &a );
std::function<bool( const item & )>
item_filter_from_string(  std::string filter )
{
    if( filter.empty() ){
        // Variable without name prevents unused parameter warning
        return [](const item&){
            return true;
        };
    }
    size_t colon;
    char flag = '\0';
    if( ( colon = filter.find( ":" ) ) != std::string::npos ) {

        if( colon >= 1 ) {
            flag = filter[colon - 1];
            filter = filter.substr( colon + 1 );
        }
    }
    switch( flag ) {
        case 'c'://category
            return [filter]( const item & i ) {
                return lcmatch( i.get_category().name, filter );
            };
            break;
        case 'm'://material
            return [filter]( const item & i ) {
                return lcmatch( i.get_base_material().name(), filter );
            };
            break;
        case 'e'://either
            return [filter]( const item & i ) {
                auto pair = get_both( filter );
                return item_filter_from_string( pair.first )( i )
                       || item_filter_from_string( pair.second )( i );
            };
            break;
        case 'b'://both
            return [filter]( const item & i ) {
                auto pair = get_both( filter );
                return item_filter_from_string( pair.first )( i )
                       && item_filter_from_string( pair.second )( i );
            };
            break;
        default://by name
            return [filter]( const item & a ) {
                return lcmatch( a.tname(), filter );
            };
            break;
    }
}
std::pair<std::string, std::string> get_both( const std::string &a )
{
    size_t f_begin = a.find( '{' ),
           f_end = a.find( '}' ),
           s_begin = a.find( '{', f_end ),
           s_end = a.find( '}', s_begin );
    return std::make_pair( a.substr( f_begin + 1, f_end - f_begin - 1 ),
                           a.substr( s_begin + 1, s_end - s_begin - 1 ) );
}
