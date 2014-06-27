#include "game.h"
#include "output.h"
#include "map.h"
#include <map>
#include <set>
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <math.h>
#include <vector>
#include <iterator>
#include "catacharset.h"
#include "translations.h"
#include "uistate.h"
#include "helper.h"
#include "item_factory.h"
#include "auto_pickup.h"
#include "messages.h"

#include "advanced_inv.h"

#define ADVINVOFS 7
// abstract of selected origin which can be inventory, or  map tile / vehicle storage / aggregate

// should probably move to an adv_inv_pane class
enum advanced_inv_sortby {
    SORTBY_NONE = 1, SORTBY_NAME, SORTBY_WEIGHT, SORTBY_VOLUME, SORTBY_CHARGES, SORTBY_CATEGORY, NUM_SORTBY
};

const point areaIndicatorRoot(5, 2);
const point linearIndicatorRoot(13, 2);

bool advanced_inventory::isDirectionalDragged(int area1, int area2)
{

    if(!(area1 == isdrag || area2 == isdrag)) {
        return false;
    }
    // one of the areas is drag square
    advanced_inv_area other = (area1 == isdrag ? squares[area2] : squares[area1]);

    // the player is not grabbing anything.
    if(p->grab_point.x == 0 && p->grab_point.y == 0) {
        return false;
    }
    if(other.offx == p->grab_point.x && other.offy == p->grab_point.y) {
        return true;
    }
    return false;
}

int getsquare(int c, int &off_x, int &off_y, std::string &areastring, advanced_inv_area *squares)
{
    int ret = - 1;
    if (!( c >= 0 && c <= 11 )) {
        return ret;
    }
    ret = c;
    off_x = squares[ret].offx;
    off_y = squares[ret].offy;
    areastring = squares[ret].name;
    return ret;
}

int getsquare(char c , int &off_x, int &off_y, std::string &areastring, advanced_inv_area *squares)
{
    int ret = - 1;
    switch(c) {
    case '0':
    case 'I':
        ret = 0;
        break;
    case '1':
    case 'B':
        ret = 1;
        break;
    case '2':
    case 'J':
        ret = 2;
        break;
    case '3':
    case 'N':
        ret = 3;
        break;
    case '4':
    case 'H':
        ret = 4;
        break;
    case '5':
    case 'G':
        ret = 5;
        break;
    case '6':
    case 'L':
        ret = 6;
        break;
    case '7':
    case 'Y':
        ret = 7;
        break;
    case '8':
    case 'K':
        ret = 8;
        break;
    case '9':
    case 'U':
        ret = 9;
        break;
    case 'a':
        ret = 10;
        break;
    case 'D':
        ret = 11;
        break;
    default :
        return -1;
    }
    return getsquare(ret, off_x, off_y, areastring, squares);
}

void advanced_inventory::print_items(advanced_inventory_pane &pane, bool active)
{
    std::vector<advanced_inv_listitem> &items = pane.items;
    WINDOW *window = pane.window;
    int page = pane.page;
    unsigned selected_index = pane.index;
    bool isinventory = ( pane.area == 0 );
    bool isall = ( pane.area == 10 );
    bool compact = (TERMX <= 100);
    size_t itemsPerPage = getmaxy( window ) - ADVINVOFS; // fixme

    int columns = getmaxx( window );
    std::string spaces(columns - 4, ' ');

    nc_color norm = active ? c_white : c_dkgray;

    //print inventory's current and total weight + volume
    if(isinventory) {
        //right align
        int hrightcol = columns -
                        helper::to_string_int(g->u.convert_weight(g->u.weight_carried())).length() - 3 - //"xxx.y/"
                        helper::to_string_int(g->u.convert_weight(g->u.weight_capacity())).length() - 3 - //"xxx.y_"
                        helper::to_string_int(g->u.volume_carried()).length() - 1 - //"xxx/"
                        helper::to_string_int(g->u.volume_capacity() - 2).length() - 1;//"xxx|"
        nc_color color = c_ltgreen;//red color if overload
        if (g->u.weight_carried() > g->u.weight_capacity()) {
            color = c_red;
        }
        mvwprintz( window, 4, hrightcol, color, "%.1f", g->u.convert_weight(g->u.weight_carried()) );
        wprintz(window, c_ltgray, "/%.1f ", g->u.convert_weight(g->u.weight_capacity()) );
        if (g->u.volume_carried() > g->u.volume_capacity() - 2) {
            color = c_red;
        } else {
            color = c_ltgreen;
        }
        wprintz(window, color, "%d", g->u.volume_carried() );
        wprintz(window, c_ltgray, "/%d ", g->u.volume_capacity() - 2 );
    } else { //print square's current and total weight + volume
        std::string head;
        if (isall) {
            head = string_format("%3.1f %3d",
                                 g->u.convert_weight(squares[pane.area].weight),
                                 squares[pane.area].volume);
        } else {
            int maxvolume;
            if (squares[pane.area].veh != NULL && squares[pane.area].vstor >= 0) {
                maxvolume = squares[pane.area].veh->max_volume(squares[pane.area].vstor);
            } else {
                maxvolume = g->m.max_volume(squares[pane.area].x, squares[pane.area].y);
            }
            head = string_format("%3.1f %3d/%3d",
                                 g->u.convert_weight(squares[pane.area].weight),
                                 squares[pane.area].volume, maxvolume);
        }
        mvwprintz( window, 4, columns - 1 - head.length(), norm, "%s", head.c_str());
    }

    //print header row and determine max item name length
    const int lastcol = columns - 2; // Last printable column
    const size_t name_startpos = ( compact ? 1: 4 );
    const size_t src_startpos = lastcol - 17;
    const size_t amt_startpos = lastcol - 14;
    const size_t weight_startpos = lastcol - 9;
    const size_t vol_startpos = lastcol - 3;
    int max_name_length = amt_startpos - name_startpos - 1; // Default name length

    //~ Items list header. Table fields length without spaces: amt - 4, weight - 5, vol - 4.
    const int table_hdr_len1 = utf8_width(_("amt weight vol")); // Header length type 1
    //~ Items list header. Table fields length without spaces: src - 2, amt - 4, weight - 5, vol - 4.
    const int table_hdr_len2 = utf8_width(_("src amt weight vol")); // Header length type 2

    mvwprintz( window, 5, ( compact ? 1 : 4 ), c_ltgray, _("Name (charges)") );
    if (isall && !compact) {
        mvwprintz( window, 5, lastcol - table_hdr_len2 + 1, c_ltgray, _("src amt weight vol") );
        max_name_length = src_startpos - name_startpos - 1; // 1 for space
    } else {
            mvwprintz( window, 5, lastcol - table_hdr_len1 + 1, c_ltgray, _("amt weight vol") );
    }

    for(unsigned i = page * itemsPerPage , x = 0 ; i < items.size() && x < itemsPerPage ; i++ , x++) {
        if ( items[i].volume == -8 ) { // I'm a header!
            mvwprintz(window, 6 + x, ( columns - items[i].name.size() - 6 ) / 2, c_cyan, "[%s]",
                      items[i].name.c_str() );
            continue;
        }
        nc_color thiscolor = active ? items[i].it->color(&g->u) : norm;
        nc_color thiscolordark = c_dkgray;
        nc_color print_color;

        if(active && selected_index == x) {
            thiscolor = (inCategoryMode &&
                         panes[src].sortby == SORTBY_CATEGORY) ? c_white_red : hilite(thiscolor);
            thiscolordark = hilite(thiscolordark);
            if ( compact ) {
                mvwprintz(window, 6 + x, 1, thiscolor, "  %s", spaces.c_str());
            } else {
                mvwprintz(window, 6 + x, 1, thiscolor, ">>%s", spaces.c_str());
            }
        }

        //print item name
        std::string it_name = utf8_truncate(items[i].it->display_name(), max_name_length);
        mvwprintz(window, 6 + x, ( compact ? 1 : 4 ), thiscolor, "%s", it_name.c_str() );

        //print src column
        if ( isall && !compact) {
            mvwprintz(window, 6 + x, src_startpos, thiscolor, "%s",
                      _(squares[items[i].area].shortname.c_str()));
        }

        //print "amount" column
        int it_amt = items[i].stacks;
        if( it_amt > 1 ) {
            print_color = thiscolor;
            if (it_amt > 9999) {
                it_amt = 9999;
                print_color = (active && selected_index == x) ? hilite(c_red) : c_red;
            }
            mvwprintz(window, 6 + x, amt_startpos, print_color, "%4d", it_amt);
        }

        //print weight column
        double it_weight = g->u.convert_weight(items[i].weight);
        size_t w_precision;
        print_color = (it_weight > 0) ? thiscolor : thiscolordark;

        if (it_weight >= 1000.0) {
            if (it_weight >= 10000.0) {
                print_color = (active && selected_index == x) ? hilite(c_red) : c_red;
                it_weight = 9999.0;
            }
            w_precision = 0;
        } else if (it_weight >= 100.0) {
            w_precision = 1;
        } else {
            w_precision = 2;
        }
        mvwprintz(window, 6 + x, weight_startpos, print_color, "%5.*f", w_precision, it_weight);

        //print volume column
        int it_vol = items[i].volume;
        print_color = (it_vol > 0) ? thiscolor : thiscolordark;
        if (it_vol > 9999) {
            it_vol = 9999;
            print_color = (active && selected_index == x) ? hilite(c_red) : c_red;
        }
        mvwprintz(window, 6 + x, vol_startpos, print_color, "%4d", it_vol );

        if(active && items[i].autopickup == true) {
            mvwprintz(window, 6 + x, 1, magenta_background(items[i].it->color(&g->u)), "%s",
                      (compact ? items[i].it->tname().substr(0, 1) : ">").c_str());
        }
    }
}

struct advanced_inv_sort_case_insensitive_less : public std::binary_function< char, char, bool > {
    bool operator () (char x, char y) const {
        return toupper( static_cast< unsigned char >(x)) < toupper( static_cast< unsigned char >(y));
    }
};

struct advanced_inv_sorter {
    int sortby;
    advanced_inv_sorter(int sort) {
        sortby = sort;
    };
    bool operator()(const advanced_inv_listitem &d1, const advanced_inv_listitem &d2) {
        if ( sortby != SORTBY_NAME ) {
            switch(sortby) {
            case SORTBY_WEIGHT: {
                if ( d1.weight != d2.weight ) {
                    return d1.weight > d2.weight;
                }
                break;
            }
            case SORTBY_VOLUME: {
                if ( d1.volume != d2.volume ) {
                    return d1.volume > d2.volume;
                }
                break;
            }
            case SORTBY_CHARGES: {
                if ( d1.it->charges != d2.it->charges ) {
                    return d1.it->charges > d2.it->charges;
                }
                break;
            }
            case SORTBY_CATEGORY: {
                if ( d1.cat != d2.cat ) {
                    return *d1.cat < *d2.cat;
                } else if ( d1.volume == -8 ) {
                    return true;
                } else if ( d2.volume == -8 ) {
                    return false;
                }
                break;
            }
            default:
                return d1.idx > d2.idx;
                break;
            };
        }
        // secondary sort by name
        std::string n1;
        std::string n2;
        if (d1.name_without_prefix ==
            d2.name_without_prefix) { //if names without prefix equal, compare full name
            n1 = d1.name;
            n2 = d2.name;
        } else {//else compare name without prefix
            n1 = d1.name_without_prefix;
            n2 = d2.name_without_prefix;
        }
        return std::lexicographical_compare( n1.begin(), n1.end(),
                                             n2.begin(), n2.end(), advanced_inv_sort_case_insensitive_less() );
    };
};

void advanced_inv_menu_square(advanced_inv_area *squares, uimenu *menu )
{
    int ofs = -25 - 4;
    int sel = menu->selected + 1;
    for ( int i = 1; i < 10; i++ ) {
        char key = (char)(i + 48);
        char bracket[3] = "[]";
        if ( squares[i].vstor >= 0 ) {
            strcpy(bracket, "<>");
        }
        bool canputitems = ( squares[i].canputitems && menu->entries[i - 1].enabled ? true : false);
        nc_color bcolor = ( canputitems ? ( sel == i ? h_cyan : c_cyan ) : c_dkgray );
        nc_color kcolor = ( canputitems ? ( sel == i ? h_ltgreen : c_ltgreen ) : c_dkgray );
        mvwprintz(menu->window, squares[i].hscreenx + 5, squares[i].hscreeny + ofs, bcolor, "%c", bracket[0]);
        wprintz(menu->window, kcolor, "%c", key);
        wprintz(menu->window, bcolor, "%c", bracket[1]);
    }
}

void advanced_inv_print_header(advanced_inv_area* squares, advanced_inventory_pane &pane, int sel=-1 )
{
    WINDOW *window = pane.window;
    int area = pane.area;
    int wwidth = getmaxx(window);
    int ofs = wwidth - 25 - 2 - 14;
    for ( int i = 0; i < 12; i++ ) {
        char key = ( i == 0 ? 'I' : ( i == 10 ? 'A' : ( i == 11 ? 'D' : (char)(i + 48) ) ) );
        char bracket[3] = "[]";
        if ( squares[i].vstor >= 0 ) {
            strcpy(bracket, "<>");
        }
        nc_color bcolor = ( squares[i].canputitems ? ( area == i || ( area == 10 &&
                            i != 0 ) ? c_cyan : c_ltgray ) : c_red );
        nc_color kcolor = ( squares[i].canputitems ? ( area == i ? c_ltgreen :
                            ( i == sel ? c_cyan : c_ltgray ) ) : c_red );
        mvwprintz(window, squares[i].hscreenx, squares[i].hscreeny + ofs, bcolor, "%c", bracket[0]);
        wprintz(window, kcolor, "%c", key);
        wprintz(window, bcolor, "%c", bracket[1]);
    }
}

void advanced_inv_update_area( advanced_inv_area &area )
{
    int i = area.id;
    const player &u = g->u;
    area.x = g->u.posx + area.offx;
    area.y = g->u.posy + area.offy;
    area.size = 0;
    area.veh = NULL;
    area.vstor = -1;
    area.desc = "";
    if( i > 0 && i < 10 ) {
        int vp = 0;
        area.veh = g->m.veh_at( u.posx + area.offx, u.posy + area.offy, vp );
        if ( area.veh ) {
            area.vstor = area.veh->part_with_feature(vp, "CARGO", false);
        }
        if ( area.vstor >= 0 ) {
            area.desc = area.veh->name;
            area.canputitems = true;
            area.size = area.veh->parts[area.vstor].items.size();
            area.max_size = MAX_ITEM_IN_VEHICLE_STORAGE;
            area.max_volume = area.veh->max_volume(area.vstor);
        } else {
            area.canputitems = g->m.can_put_items(u.posx + area.offx, u.posy + area.offy);
            area.size = g->m.i_at(u.posx + area.offx, u.posy + area.offy).size();
            area.max_size = MAX_ITEM_IN_SQUARE;
            area.max_volume = g->m.max_volume(u.posx + area.offx, u.posy + area.offy);
            if (g->m.graffiti_at(u.posx + area.offx, u.posy + area.offy).contents) {
                area.desc = g->m.graffiti_at(u.posx + area.offx, u.posy + area.offy).contents->c_str();
            }
        }
    } else if ( i == 0 ) {
        area.size = u.inv.size();
        area.canputitems = true;
    } else if (i == 11 ) {
        int vp = 0;
        area.veh = g->m.veh_at( u.posx + u.grab_point.x, u.posy + u.grab_point.y, vp);
        if( area.veh ) {
            area.vstor = area.veh->part_with_feature(vp, "CARGO", false);
        }
        if( area.vstor >= 0 ) {
            area.desc = area.veh->name;
            area.canputitems = true;
            area.size = area.veh->parts[area.vstor].items.size();
            area.max_size = MAX_ITEM_IN_VEHICLE_STORAGE;
            area.max_volume = area.veh->max_volume(area.vstor);
        } else {
            area.canputitems = false;
            area.desc = _("No dragged vehicle");
        }
    } else {
        area.desc = _("All 9 squares");
        area.canputitems = true;
    }
    area.volume = 0; // must update in main function
    area.weight = 0; // must update in main function
}

std::string center_text(const char *str, int width)
{
    std::string spaces;
    int numSpaces = width - strlen(str);
    for (int i = 0; i < numSpaces / 2; i++) {
        spaces += " ";
    }
    return spaces + std::string(str);
}

void advanced_inventory::init(player *pp)
{
    this->p = pp;

    advanced_inv_area initsquares[12] = {
        {0, 2, 25, 0, 0, 0, 0, _("Inventory"), "IN", false, NULL, -1, 0, "", 0, 0, 0, 0 },
        {1, 3, 30, -1, 1, 0, 0, _("South West"), "SW", false, NULL, -1, 0, "", 0, 0, 0, 0 },
        {2, 3, 33, 0, 1, 0, 0, _("South"), "S", false, NULL, -1, 0, "", 0, 0, 0, 0 },
        {3, 3, 36, 1, 1, 0, 0, _("South East"), "SE", false, NULL, -1, 0, "", 0, 0, 0, 0 },
        {4, 2, 30, -1, 0, 0, 0, _("West"), "W", false, NULL, -1, 0, "", 0, 0, 0, 0 },
        {5, 2, 33, 0, 0, 0, 0, _("Directly below you"), "DN", false, NULL, -1, 0, "", 0, 0, 0, 0 },
        {6, 2, 36, 1, 0, 0, 0, _("East"), "E", false, NULL, -1, 0, "", 0, 0, 0, 0 },
        {7, 1, 30, -1, -1, 0, 0, _("North West"), "NW", false, NULL, -1, 0, "", 0, 0, 0, 0 },
        {8, 1, 33, 0, -1, 0, 0, _("North"), "N", false, NULL, -1, 0, "", 0, 0, 0, 0 },
        {9, 1, 36, 1, -1, 0, 0, _("North East"), "NE", false, NULL, -1, 0, "", 0, 0, 0, 0 },
        {10, 3, 25, 0, 0, 0, 0, _("Surrounding area"), "AL", false, NULL, -1, 0, "", 0, 0, 0, 0 },
        {11, 1, 25, 0, 0, 0, 0, _("Grabbed Vehicle"), "GR", false, NULL, -1, 0, "", 0, 0, 0, 0 }
    };
    for ( int i = 0; i < 12; i++ ) {
        squares[i] = initsquares[i];
        advanced_inv_update_area(squares[i]);
    }

    panes[left].pos = 0;
    panes[left].area = 10;
    panes[right].pos = 1;
    panes[right].area = isinventory;

    panes[left].sortby = uistate.adv_inv_leftsort;
    panes[right].sortby = uistate.adv_inv_rightsort;
    panes[left].area = uistate.adv_inv_leftarea;
    panes[right].area = uistate.adv_inv_rightarea;
    bool moved = ( uistate.adv_inv_last_coords.x != p->posx ||
                   uistate.adv_inv_last_coords.y != p->posy );
    if ( !moved || panes[left].area == isinventory ) {
        panes[left].index = uistate.adv_inv_leftindex;
        panes[left].page = uistate.adv_inv_leftpage;
    }
    if ( !moved || panes[right].area == isinventory ) {
        panes[right].index = uistate.adv_inv_rightindex;
        panes[right].page = uistate.adv_inv_rightpage;
    }

    panes[left].filter = uistate.adv_inv_leftfilter;
    panes[right].filter = uistate.adv_inv_rightfilter;


    checkshowmsg = false;
    showmsg = false;

    itemsPerPage = 10;
    w_height = (TERMY < min_w_height + head_height) ? min_w_height : TERMY - head_height;
    w_width = (TERMX < min_w_width) ? min_w_width : (TERMX > max_w_width) ? max_w_width : (int)TERMX;

    headstart = 0; //(TERMY>w_height)?(TERMY-w_height)/2:0;
    colstart = (TERMX > w_width) ? (TERMX - w_width) / 2 : 0;

    // todo: awaiting ui::menu // last_tmpdest=-1;
    exit = false;
    redraw = true;
    recalc = true;
    lastCh = 0;

    src = left; // the active screen , 0 for left , 1 for right.
    dest = right;
    examineScroll = false;
    filter_edit = false;
}

bool cached_lcmatch(const std::string &str, const std::string &findstr,
                    std::map<std::string, bool> &filtercache)
{
    if ( filtercache.find(str) == filtercache.end() ) {
        std::string ret = "";
        ret.reserve( str.size() );
        transform( str.begin(), str.end(), std::back_inserter(ret), tolower );
        bool ismatch = ( ret.find( findstr ) != std::string::npos );
        filtercache[ str ] = ismatch;
        return ismatch;
    } else {
        return filtercache[ str ];
    }
}

void advanced_inventory::recalc_pane(int i)
{
    panes[i].recalc = false;
    bool filtering = ( !panes[i].filter.empty() );
    player &u = *p;
    map &m = g->m;
    int idest = (i == left ? right : left);
    panes[i].items.clear();
    std::set<std::string> has_category;
    panes[i].numcats = 0;
    int avolume = 0;
    int aweight = 0;

    if(panes[i].area == isinventory) {
        const invslice &stacks = u.inv.slice();
        for (unsigned x = 0; x < stacks.size(); ++x ) {
            item &an_item = stacks[x]->front();
            advanced_inv_listitem it;
            it.name = an_item.tname();
            it.name_without_prefix = an_item.tname( false );
            if ( filtering && ! cached_lcmatch(it.name, panes[i].filter, panes[i].filtercache ) ) {
                continue;
            }
            it.idx = x;
            int size = u.inv.const_stack(x).size();
            if ( size < 1 ) {
                size = 1;
            }
            it.autopickup = hasPickupRule(it.name);
            it.stacks = size;
            it.weight = an_item.weight() * size;
            it.volume = an_item.volume() * size;
            it.cat = &(an_item.get_category());
            it.it = &an_item;
            it.area = panes[i].area;
            if( has_category.count(it.cat->id) == 0 ) {
                has_category.insert(it.cat->id);
                panes[i].numcats++;
                if(panes[i].sortby == SORTBY_CATEGORY) {
                    advanced_inv_listitem itc;
                    itc.idx = -8;
                    itc.stacks = -8;
                    itc.weight = -8;
                    itc.volume = -8;
                    itc.cat = it.cat;
                    itc.name = it.cat->name;
                    itc.area = panes[i].area;
                    panes[i].items.push_back(itc);
                }
            }
            avolume += it.volume;
            aweight += it.weight;
            panes[i].items.push_back(it);
        }
    } else {
        int s1 = panes[i].area;
        int s2 = panes[i].area;
        if ( panes[i].area == isall ) {
            s1 = 1;
            s2 = 9;
        }
        for (int s = s1; s <= s2; s++) {
            int savolume = 0;
            int saweight = 0;
            advanced_inv_update_area(squares[s]);

            if ( panes[idest].area != s && squares[s].canputitems &&
                 !isDirectionalDragged(s, panes[idest].area)) {

                const itemslice &stacks = squares[s].vstor >= 0 ?
                                           m.i_stacked(squares[s].veh->parts[squares[s].vstor].items) :
                                           m.i_stacked(m.i_at(squares[s].x , squares[s].y ));

                //loop through lists of item stacks
                for (unsigned x = 0; x < stacks.size(); ++x) {
                    item *an_item = stacks[x].front();
                    advanced_inv_listitem it;
                    int stackSize = stacks[x].size() < 1 ? 1 : stacks[x].size();

                    it.idx = x;
                    it.name = an_item->tname();
                    it.name_without_prefix = an_item->tname( false );
                    if ( filtering && ! cached_lcmatch(it.name, panes[i].filter, panes[i].filtercache ) ) {
                        continue;
                    }

                    it.autopickup = hasPickupRule(it.name);
                    it.stacks = stackSize;
                    it.weight = an_item->weight() * stackSize;
                    it.volume = an_item->volume() * stackSize;
                    it.cat = &(an_item->get_category());
                    it.it = an_item;
                    it.area = s;
                    if( has_category.count(it.cat->id) == 0 ) {
                        has_category.insert(it.cat->id);
                        panes[i].numcats++;
                        if(panes[i].sortby == SORTBY_CATEGORY) {
                            advanced_inv_listitem itc;
                            itc.idx = -8;
                            itc.stacks = -8;
                            itc.weight = -8;
                            itc.volume = -8;
                            itc.cat = it.cat;
                            itc.name = it.cat->name;
                            itc.area = s;
                            panes[i].items.push_back(itc);
                        }
                    }

                    savolume += it.volume;
                    saweight += it.weight;
                    panes[i].items.push_back(it);

                } // for( size_t x = 0; x < items.size(); ++x )

            } // if( panes[idest].area != s && squares[s].canputitems )
            avolume += savolume;
            aweight += saweight;
        } // for(int s = s1; s <= s2; s++)

    } // if(panes[i].area ?? isinventory)

    advanced_inv_update_area(squares[panes[i].area]);

    squares[panes[i].area].volume = avolume;
    squares[panes[i].area].weight = aweight;

    panes[i].veh = squares[panes[i].area].veh; // <--v-- todo deprecate
    panes[i].vstor = squares[panes[i].area].vstor;
    panes[i].size = panes[i].items.size();

    // sort the stuff
    switch(panes[i].sortby) {
    case SORTBY_NONE:
        if ( i != isinventory ) {
            std::stable_sort( panes[i].items.begin(), panes[i].items.end(), advanced_inv_sorter(SORTBY_NONE) );
        }
        break;
    default:
        std::stable_sort( panes[i].items.begin(), panes[i].items.end(),
                          advanced_inv_sorter( panes[i].sortby ) );
        break;
    }


}

void advanced_inventory::redraw_pane( int i )
{
    std::string sortnames[8] = { "-none-", _("none"), _("name"), _("weight"), _("volume"),
                                 _("charges"), _("category"), "-"
                               };
    // calculate the offset.
    getsquare(panes[i].area, panes[i].offx, panes[i].offy, panes[i].area_string, squares);

    if( recalc || panes[i].recalc == true ) {
        recalc_pane(i);
    }
    panes[i].redraw = false;
    // paginate (not sure why)
    panes[i].max_page = (int)ceil(panes[i].size / (itemsPerPage +
                                  0.0)); //(int)ceil(panes[i].size/20.0);

    if (panes[i].max_page == 0) {
        // No results forces page 0.
        panes[i].page = 0;
    } else if (panes[i].page >= panes[i].max_page) {
        // Clamp to max_page.
        panes[i].page = panes[i].max_page - 1;
    }

    // Determine max index.
    if (panes[i].max_page == 0 || panes[i].page == (-1 + panes[i].max_page)) {
        // We are on the last page.
        if (0 == (panes[i].size % itemsPerPage)) {
            // Last page was exactly full, use maximum..
            panes[i].max_index = itemsPerPage;
        } else {
            // Last page was not full, use remainder.
            panes[i].max_index = panes[i].size % itemsPerPage;
        }
    } else {
        // We aren't on the last page, so the last item is always maximum.
        panes[i].max_index = itemsPerPage;
    }

    // Last chance to force the index in range.
    if (panes[i].index >= panes[i].max_index && panes[i].max_index > 0) {
        panes[i].index = panes[i].max_index - 1;
    }
    if( panes[i].sortby == SORTBY_CATEGORY && !panes[i].items.empty() ) {
        unsigned lpos = panes[i].index + (panes[i].page * itemsPerPage);
        if ( lpos < panes[i].items.size() && panes[i].items[lpos].volume == -8 ) {
            // Force the selection off the category labels, but don't run off the page.
            panes[i].index += ( panes[i].index + 1 >= itemsPerPage ? -1 : 1 );
        }
    }
    // draw the stuff
    werase(panes[i].window);

    print_items( panes[i], (src == i) );

    int sel = -1;
    if ( panes[i].size > 0 && panes[i].size > panes[i].index) {
        sel = panes[i].items[panes[i].index].area;
    }

    advanced_inv_print_header(squares, panes[i], sel );
    // todo move --v to --^
    mvwprintz(panes[i].window, 1, 2, src == i ? c_cyan : c_ltgray, "%s",
              panes[i].area_string.c_str());
    mvwprintz(panes[i].window, 2, 2, src == i ? c_green : c_dkgray , "%s",
              squares[panes[i].area].desc.c_str() );

    if ( i == src ) {
        if(panes[src].max_page > 1 ) {
            mvwprintz(panes[src].window, 4, 2, c_ltblue, _("[<] page %d of %d [>]"),
                      panes[src].page + 1, panes[src].max_page);
        }
    }
    ////////////
    if ( src == i ) {
        wattron(panes[i].window, c_cyan);
    }
    draw_border(panes[i].window);
    mvwprintw(panes[i].window, 0, 3, _("< [s]ort: %s >"),
              sortnames[ ( panes[i].sortby <= 6 ? panes[i].sortby : 0 ) ].c_str() );
    int max = MAX_ITEM_IN_SQUARE;
    if ( panes[i].area == isall ) {
        max *= 9;
    }
    int fmtw = 7 + ( panes[i].size > 99 ? 3 : panes[i].size > 9 ? 2 : 1 ) +
               ( max > 99 ? 3 : max > 9 ? 2 : 1 );
    mvwprintw(panes[i].window, 0 , (w_width / 2) - fmtw, "< %d/%d >", panes[i].size, max );
    const char *fprefix = _("[F]ilter");
    const char *fsuffix = _("[R]eset");
    if ( ! filter_edit ) {
        if ( !panes[i].filter.empty() ) {
            mvwprintw(panes[i].window, getmaxy(panes[i].window) - 1, 2, "< %s: %s >", fprefix,
                      panes[i].filter.c_str() );
        } else {
            mvwprintw(panes[i].window, getmaxy(panes[i].window) - 1, 2, "< %s >", fprefix );
        }
    }
    if ( src == i ) {
        wattroff(panes[i].window, c_white);
    }
    if ( ! filter_edit && !panes[i].filter.empty() ) {
        mvwprintz(panes[i].window, getmaxy(panes[i].window) - 1, 6 + strlen(fprefix), c_white, "%s",
                  panes[i].filter.c_str() );
        mvwprintz(panes[i].window, getmaxy(panes[i].window) - 1,
                  getmaxx(panes[i].window) - strlen(fsuffix) - 2, c_white, "%s", fsuffix);
    }

}

bool advanced_inventory::move_all_items()
{
    player &u = *p;
    map &m = g->m;

    bool filtering = ( !panes[src].filter.empty() );

    // If the active screen has no item.
    if( panes[src].size == 0 ) {
        return false;
    }

    //update the dest variable
    dest = (src == left ? right : left);
    int destarea = panes[dest].area;
    if ( panes[dest].area == isall ) {
        popup(_("You have to choose a destination area."));
        return false;
    }

    if ( panes[src].area == isall) {
        popup(_("You have to choose a source area."));
        return false;
    }

    // is this panel the player inventory?
    if (panes[src].area == isinventory) {
        // Handle moving from inventory
        if(query_yn(_("Really move everything from your inventory?"))) {

            int part = panes[dest].vstor;
            vehicle *veh = panes[dest].veh;
            int d_x = u.posx + panes[dest].offx;
            int d_y = u.posy + panes[dest].offy;
            // Ok, we're go to (try) and move everything from the player inventory.
            // First, we'll want to iterate backwards
            for (int ip = u.inv.size() - 1; ip >= 0; /* noop */ ) {
                // Get the stack at index ip.
                const std::list<item> &stack = u.inv.const_stack(ip);
                // Get the first item in that stack.
                const item *it = &stack.front();

                // if we're filtering, check if this item is in the filter. If it isn't, continue
                if ( filtering && ! cached_lcmatch(it->tname(), panes[src].filter,
                                                   panes[src].filtercache ) ) {
                    --ip;
                    continue;
                }

                // max items in the destination area
                int max_items = (squares[destarea].max_size - squares[destarea].size);
                // get the free volume in the destination area
                int free_volume = 1000 * ( panes[dest].vstor >= 0 ?
                                           veh->free_volume(part) : m.free_volume( d_x, d_y ));

                long amount = 1; // the amount to move from the stack
                int volume = it->precise_unit_volume(); // exact volume

                // we'll want to get the maximum amount depending on charges or stack size
                if (stack.size() > 1) {
                    amount = stack.size();
                } else if (it->count_by_charges()) {
                    amount = it->charges;
                }

                if (volume > 0 && volume * amount > free_volume) {
                    // how many items can we fit?
                    int volmax = int( free_volume / volume );
                    // can't fit this itme, let's check another
                    if (volmax == 0) {
                        add_msg(m_info, _("Unable to move item, the destination is too full."));
                        --ip;
                        continue;
                    }

                    // we'll want to move as many as possible
                    if (stack.size() > 1) {
                        max_items = ( volmax < max_items ? volmax : max_items);
                    } else if ( it->count_by_charges()) {
                        max_items = volmax;
                    }
                } else if ( it->count_by_charges()) {
                    // not over the volume maximum, so just use as much as possible
                    max_items = amount;
                }

                // no items? no move.
                if (max_items == 0) {
                    add_msg(m_info, _("Unable to move item, the destination is too full."));
                    --ip;
                    continue;
                }


                if (stack.size() > 1) {     // we have a stacked item
                    if ( amount != 0 && amount <= long( stack.size() )) {
                        long all_items = long(stack.size());
                        amount = amount > max_items ? max_items : amount;
                        std::list<item> moving_items = u.inv.reduce_stack(ip, amount); // reduce our inventory by amount of item at ip
                        bool chargeback = false; // in case we need to give back items.
                        int moved = 0;
                        // loop over the items we're trying to move, add them one by one to the destination
                        for (std::list<item>::iterator iter = moving_items.begin(); iter != moving_items.end(); ++iter) {
                            if (chargeback == true) {
                                u.i_add(*iter); // we give back the rest of the item
                            } else {
                                // in theory, none of the below should evaluate to false, or we've done something weird calculating above, i think
                                if (panes[dest].vstor >= 0) {
                                    if (veh->add_item(part, *iter) == false) {
                                        u.i_add(*iter);
                                        add_msg(m_info, _("Destination full. %d / %d moved. Please report a bug if items have vanished."), moved, amount);
                                        chargeback = true;
                                    }
                                } else {
                                    if (m.add_item_or_charges(d_x, d_y, *iter, 0) == false) {
                                        u.i_add(*iter);
                                        add_msg(m_info, _("Destination full. %d / %d moved. Please report a bug if items have vanished."), moved, amount);
                                        chargeback = true;
                                    }
                                }
                            }
                            moved++;
                        }

                        if (moved != 0) {
                            u.moves -= 100;
                        }
                        // only move the iterator if we didn't move everything from the stack
                        if (amount != all_items || chargeback == true) {
                            --ip;
                            continue;
                        }
                    }
                } else if (it->count_by_charges()) {
                    amount = amount > max_items ? max_items : amount;
                    if (amount != 0 && amount <= it->charges) {
                        item moving_item = u.inv.reduce_charges(ip, amount);
                        if (panes[dest].vstor >= 0) {
                            if (veh->add_item(part, moving_item) == false) {
                                u.i_add(moving_item);
                                add_msg(m_info, _("Destination full. Please report a bug if items have vanished."));
                            }
                        } else {
                            if (m.add_item_or_charges(d_x, d_y, moving_item, 0) == false) {
                                u.i_add(moving_item);
                                add_msg(m_info, _("Destination full. Please report a bug if items have vanished."));
                            }
                        }

                        u.moves -= 100;
                    }
                } else {
                    bool chargeback = false;
                    item moving_item = u.inv.remove_item(ip);
                    if (panes[dest].vstor >= 0) {
                        if (veh->add_item(part, moving_item) == false) {
                            u.i_add(moving_item);
                            add_msg(m_info, _("Destination full. Please report a bug if items have vanished."));
                            chargeback = true;
                        }
                    } else {
                        if (m.add_item_or_charges(d_x, d_y, moving_item) == false) {
                            u.i_add(moving_item);
                            add_msg(m_info, _("Destination full. Please report a bug if items have vanished."));
                            chargeback = true;
                        }
                    }
                    if (chargeback == false) {
                        u.moves -= 100;
                    }
                }

                //aaaaaaand iterate
                --ip;
            }

        } else {
            return false;
        }

    // Otherwise, we have a normal square to work with
    } else {

        int p_x = u.posx + panes[src].offx;
        int p_y = u.posy + panes[src].offy;
        int part = panes[src].vstor;
        vehicle *veh = panes[src].veh;
        // by default, we want to iterate the items at a location
        std::vector<item> *items_to_iterate = &m.i_at(p_x, p_y);

        // but if it's a vehicle, we'll want the items in the vehicle
        if (panes[src].vstor >= 0) {
            items_to_iterate = &veh->parts[part].items;
        }

        for (std::vector<item>::iterator it = items_to_iterate->begin();
             it != items_to_iterate->end(); /* noop */) {
            // if we're filtering, check if this item is in the filter. If it isn't, continue
            if ( filtering && ! cached_lcmatch(it->tname(), panes[src].filter, panes[src].filtercache ) ) {
                ++it;
                continue;
            }

            // Don't even try.
            if (it->made_of(LIQUID)) {
                ++it;
                continue;
            } else {
                // if the item has charges, how many should we move?
                long trycharges = -1;
                // Picking up to inventory?
                if (destarea == isinventory) {
                    if (!u.can_pickup(true)) {
                        return true;
                    }
                    if(squares[destarea].size >= MAX_ITEM_IN_SQUARE) {
                        add_msg(m_info, _("You are carrying too many items."));
                        return true;
                    }
                    // Ok, let's see. What is the volume and weight?

                    int tryvolume = it->volume();   // this is the volume we're going to check
                    int tryweight = it->weight();   // this is the weight we're going to check
                    int amount = 1;                 // this is the amount of items we're moving
                    // does this item have charges, and do we count by that?
                    if (it->count_by_charges() && it->charges > 1) {
                        amount = it->charges;
                        int unitvolume = it->precise_unit_volume(); // get the exact volume per unit
                        int unitweight = ( tryweight * 1000 ) / it->charges; // and the unit weight

                        int max_vol = (u.volume_capacity() - u.volume_carried()) * 1000; // how much more can we carry (volume)
                        int max_weight = (( u.weight_capacity() * 4 ) - u.weight_carried()) * 1000; // how much more can we carry (weight)
                        int max = amount; // the max is the maximum we can pick up

                        // we'll check and see how many items we can pick up in total, if the volume is above the max_vol
                        if ( unitvolume > 0 && unitvolume * amount > max_vol ) {
                            max = int( max_vol / unitvolume );
                        }
                        // we'll check and see how many items we can pick up in total, if the weight is above the max_weight
                        if ( unitweight > 0 && unitweight * amount > max_weight ) {
                            max = int( max_weight / unitweight );
                        }

                        // Can we pick this up at all?
                        if (max != 0) {
                            if ( amount != it->charges ) {
                                tryvolume = ( unitvolume * amount ) / 1000;
                                tryweight = ( unitweight * amount ) / 1000;
                                trycharges = amount;
                            }
                            if ( trycharges == 0 ) {
                                add_msg(m_info, _("Unable to pick up %s."), it->tname().c_str());
                                ++it;
                                continue;
                            }
                        } else {
                            add_msg(m_info, _("Unable to pick up %s."), it->tname().c_str());
                            ++it;
                            continue;
                        }
                    }

                    // We've already checked if we're trying to pick up a stack
                    if(!u.can_pickVolume(tryvolume)) {
                        add_msg(m_info, _("There's no room in your inventory for %s."), it->tname().c_str());
                        ++it;
                        continue;
                    } else if (!u.can_pickWeight(tryweight, false)) {
                        add_msg(m_info, _("%s is too heavy."), it->tname().c_str());
                        ++it;
                        continue;
                    }
                }
                // We can move it!
                item new_item = (*it);

                // So, if there's are charges, we'll set the new item to this.
                if ( trycharges > 0 ) {
                    new_item.charges = trycharges;
                }

                // if it's an inventory, we'll have to let time pass, and move the item
                // we also know that we can pick it up, we've calculated that above!
                if(destarea == isinventory) {
                    u.inv.assign_empty_invlet(new_item);
                    u.i_add(new_item);
                    u.moves -= 100;

                // if it is a vehicle storage, try to move it there. If not, let's just continue
                } else if (squares[destarea].vstor >= 0) {
                    if( squares[destarea].veh->add_item( squares[destarea].vstor, new_item ) == false) {
                        add_msg(m_info, _("Unable to move item, the destination is too full."));
                        ++it;
                        continue;
                    }

                // if it's a normal square, try to move it there. If not, just continue
                } else {
                    if ( m.add_item_or_charges(squares[destarea].x, squares[destarea].y, new_item, 0 ) == false ) {
                        add_msg(m_info, _("Unable to move item, the destination is too full."));
                        ++it;
                        continue;
                    }
                }

                // OK! Item is moved. Now deduct charges, or remove the item completely
                if ( trycharges > 0 ) {
                    it->charges -= trycharges;
                    ++it;
                    continue;
                }

                it = items_to_iterate->erase(it);
            }
        }
    }
    /*for (std::vector<advanced_inv_listitem>::iterator ait = panes[src].items.begin(); ait != panes[src].items.end(); ++ait)

    {
        int item_pos = panes[src].size > 0 ? ait->idx : 0;
        add_msg("Item %s", ait->it->tname().c_str());


    }
*/

    return true; // passed, so let's continue

}

void advanced_inventory::display(player *pp)
{
    init(pp);

    player &u = *p;
    map &m = g->m;

    u.inv.sort();
    u.inv.restack((&g->u));

    std::string sortnames[8] = { "-none-", _("none"), _("name"), _("weight"), _("volume"),
                                 _("charges"), _("category"), "-"
                               };

    WINDOW *head = newwin(head_height, w_width, headstart, colstart);
    WINDOW *left_window = newwin(w_height, w_width / 2, headstart + head_height, colstart);
    WINDOW *right_window = newwin(w_height, w_width / 2, headstart + head_height, colstart + w_width / 2);

    itemsPerPage = getmaxy(left_window) - ADVINVOFS;

    panes[left].window = left_window;
    panes[right].window = right_window;

    inCategoryMode = false;

    std::vector<int> category_index_start;
    category_index_start.reserve(NUM_SORTBY);

    while(!exit) {
        dest = (src == left ? right : left);
        // recalc and redraw
        if ( recalc ) {
            redraw = true;
        }
        for (int i = 0; i < 2; i++) {
            if ( panes[i].recalc ) {
                panes[i].redraw = true;
            }
        }

        if ( recalc ) {
            redraw = true; // global recalc = global redraw
        }

        // any redraw = redraw everything except opposite
        if(redraw || panes[0].redraw || panes[1].redraw ) {
            for (int i = 0; i < 2; i++) {
                if ( redraw || panes[i].redraw ) {
                    redraw_pane( i );
                    wrefresh(panes[i].window);
                }
            }
            recalc = false;
            if (redraw) {
                werase(head);
                draw_border(head);
                if (checkshowmsg && Messages::has_undisplayed_messages()){
                    showmsg = true;
                }

                if( showmsg ) {
                    Messages::display_messages(head, 2, 1, w_width - 1, 4);
                }
                if ( ! showmsg ) {
                    mvwprintz(head, 0, w_width - utf8_width(_("< [?] show log >")) - 1,
                              c_white, _("< [?] show log >"));
                    mvwprintz(head, 1, 2, c_white,
                              _("hjkl or arrow keys to move cursor, [m]ove item between panes ([M]: all)"));
                    mvwprintz(head, 2, 2, c_white,
                              _("1-9 to select square for active tab, 0 for inventory, D for dragged item,")); // 1-9 or GHJKLYUBNID
                    mvwprintz(head, 3, 2, c_white,
                              _("[e]xamine, [s]ort, toggle auto[p]ickup, [,] to move all items, [q]uit."));
                    if (panes[src].sortby == SORTBY_CATEGORY) {
                        nc_color highlight_color = inCategoryMode ? c_white_red : h_ltgray;
                        mvwprintz(head, 3, 3 + utf8_width(_("[e]xamine, [s]ort, toggle auto[p]ickup, [,] to move all items, [q]uit.")),
                                  highlight_color, _("[space] toggles selection modes."));
                    }
                } else {
                    mvwprintz(head, 0, w_width - utf8_width(_("< [?] show help >")) - 1,
                              c_white, _("< [?] show help >"));
                }
                wrefresh(head);
            }
            redraw = false;
        }

        int list_pos = panes[src].index + (panes[src].page * itemsPerPage);
        int item_pos = panes[src].size > list_pos ? panes[src].items[list_pos].idx : 0;

        int changex = -1;
        int changey = 0;
        bool donothing = false;

        int c = lastCh ? lastCh : getch();
        lastCh = 0;
        int changeSquare;

        if(c == 'i') {
            c = (char)'0';
        }

        if(c == 'A') {
            c = (char)'a';
        }

        if(c == 'd') {
            c = (char)'D';
        }

        changeSquare = getsquare((char)c, panes[src].offx, panes[src].offy,
                                 panes[src].area_string, squares);

        category_index_start.clear();

        // Finds the index of the first item in each category.
        for (unsigned current_item_index = 0; current_item_index < panes[src].items.size();
             ++current_item_index) {
            // Found a category header.
            if (panes[src].items[current_item_index].volume == -8) {
                category_index_start.push_back(current_item_index + 1);
            }
        }

        if (' ' == c) {
            inCategoryMode = !inCategoryMode;
            redraw = true; // We redraw to force the color change of the highlighted line and header text.
        } else if(changeSquare != -1) {
            // do nthing
            if(panes[left].area == changeSquare || panes[right].area == changeSquare ||
               isDirectionalDragged(panes[left].area, changeSquare) ||
               isDirectionalDragged(panes[right].area, changeSquare)) {
                // store the old values temporarily
                int lArea = panes[left].area;
                int lPage = panes[left].page;
                int lIndex = panes[left].index;

                // Switch left and right pane.
                panes[left].area = panes[right].area;
                panes[left].page = panes[right].page;
                panes[left].index = panes[right].index;
                panes[right].area = lArea;
                panes[right].page = lPage;
                panes[right].index = lIndex;

            } else if(squares[changeSquare].canputitems) {
                panes[src].area = changeSquare;
                panes[src].page = 0;
                panes[src].index = 0;
            } else {
                popup(_("You can't put items there"));
            }
            recalc = true;
        } else if('m' == c || 'M' == c || '\n' == c ) {
            // If the active screen has no item.
            if( panes[src].size == 0 ) {
                continue;
            } else if ( item_pos == -8 ) {
                continue; // category header
            }
            bool moveall = ('M' == c || '\n' == c );
            int destarea = panes[dest].area;
            if ( panes[dest].area == isall ) {
                bool valid = false;
                uimenu m; /* using new uimenu class */
                m.text = _("Select destination");
                m.pad_left = 9; /* free space for advanced_inv_menu_square */

                for(int i = 1; i < 10; i++) {
                    std::string prefix = string_format("%2d/%d",
                                                       squares[i].size, MAX_ITEM_IN_SQUARE);
                    if (squares[i].size >= MAX_ITEM_IN_SQUARE) {
                        prefix += _(" (FULL)");
                    }
                    m.entries.push_back( uimenu_entry( /* std::vector<uimenu_entry> */
                                             i, /* return value */
                                             (squares[i].canputitems && i != panes[src].area), /* enabled */
                                             i + 48, /* hotkey */
                                             prefix + " " + squares[i].name + " " +
                                             ( squares[i].vstor >= 0 ? squares[i].veh->name : "" ) /* entry text */
                                         ) );
                }

                m.selected = uistate.adv_inv_last_popup_dest - 1; // selected keyed to uimenu.entries, which starts at 0;
                m.show(); // generate and show window.
                while ( m.ret == UIMENU_INVALID && m.keypress != 'q' && m.keypress != KEY_ESCAPE ) {
                    advanced_inv_menu_square(squares, &m ); // render a fancy ascii grid at the left of the menu
                    m.query(false); // query, but don't loop
                }
                if ( m.ret >= 0 && m.ret <= 9 ) { // is it a square?
                    if ( m.ret == panes[src].area ) { // should never happen, but sanity checks keep developers sane.
                        popup(_("Can't move stuff to the same place."));
                    } else if ( ! squares[m.ret].canputitems ) { // this was also disabled in it's uimenu_entry
                        popup(_("Invalid. Like the menu said."));
                    } else {
                        destarea = m.ret;
                        valid = true;
                        uistate.adv_inv_last_popup_dest = m.ret;
                    }
                }
                if ( ! valid ) {
                    continue;
                }
            }
            if (!squares[destarea].canputitems) {
                popup(_("You can't put items there"));
                redraw = true;
                continue;
            }
            // from inventory
            if(panes[src].area == isinventory) {
                int max = (squares[destarea].max_size - squares[destarea].size);
                int free_volume = 1000 * ( squares[ destarea ].vstor >= 0 ?
                                           squares[ destarea ].veh->free_volume( squares[ destarea ].vstor ) :
                                           m.free_volume ( squares[ destarea ].x, squares[ destarea ].y ) );
                const std::list<item> &stack = u.inv.const_stack(item_pos);
                const item *it = &stack.front();

                long amount = 1;
                int volume = it->precise_unit_volume();
                bool askamount = false;
                if ( stack.size() > 1) {
                    amount = stack.size();
                    askamount = true;
                } else if ( it->count_by_charges() ) {
                    amount = it->charges;
                    askamount = true;
                }

                if ( volume > 0 && volume * amount > free_volume ) {
                    int volmax = int( free_volume / volume );
                    if ( volmax == 0 ) {
                        popup(_("Destination area is full. Remove some items first."));
                        continue;
                    }
                    if ( stack.size() > 1) {
                        max = ( volmax < max ? volmax : max );
                    } else if ( it->count_by_charges() ) {
                        max = volmax;
                    }
                } else if ( it->count_by_charges() ) {
                    max = amount;
                }
                if ( max == 0 ) {
                    popup(_("Destination area has too many items. Remove some first."));
                    continue;
                }
                if ( askamount && ( amount > max || !moveall ) ) {
                    std::string popupmsg = _("How many do you want to move? (0 to cancel)");
                    if(amount > max) {
                        popupmsg = string_format(_("Destination can only hold %d! Move how many? (0 to cancel) "), max);
                    }
                    // fixme / todo make popup take numbers only (m = accept, q = cancel)
                    amount = helper::to_int(string_input_popup( popupmsg, 20,
                                            helper::to_string_int(( amount > max ? max : amount )),
                                            "", "", -1, true)); //input only digits
                }
                recalc = true;
                if(stack.size() > 1) { // if the item is stacked
                    if ( amount != 0 && amount <= long( stack.size() ) ) {
                        amount = amount > max ? max : amount;
                        std::list<item> moving_items = u.inv.reduce_stack(item_pos, amount);
                        bool chargeback = false;
                        int moved = 0;
                        for (std::list<item>::iterator iter = moving_items.begin();
                             iter != moving_items.end(); ++iter) {
                            if ( chargeback == true ) {
                                u.i_add(*iter);
                            } else {
                                if(squares[destarea].vstor >= 0) {
                                    if(squares[destarea].veh->add_item(squares[destarea].vstor, *iter) == false) {
                                        // testme
                                        u.i_add(*iter);
                                        popup(_("Destination full. %d / %d moved. Please report a bug if items have vanished."), moved,
                                              amount);
                                        chargeback = true;
                                    }
                                } else {
                                    if(m.add_item_or_charges(squares[destarea].x, squares[destarea].y, *iter, 0) == false) {
                                        // testme
                                        u.i_add(*iter);
                                        popup(_("Destination full. %d / %d moved. Please report a bug if items have vanished."), moved,
                                              amount);
                                        chargeback = true;
                                    }
                                }
                                moved++;
                            }
                        }
                        if ( moved != 0 ) {
                            u.moves -= 100;
                        }
                    }
                } else if(it->count_by_charges()) {
                    if(amount != 0 && amount <= it->charges ) {
                        item moving_item = u.inv.reduce_charges(item_pos, amount);
                        if (squares[destarea].vstor >= 0) {
                            if(squares[destarea].veh->add_item(squares[destarea].vstor, moving_item) == false) {
                                // fixme add item back
                                u.i_add(moving_item);
                                popup(_("Destination full. Please report a bug if items have vanished."));
                                continue;
                            }
                        } else {
                            if ( m.add_item_or_charges(squares[destarea].x, squares[destarea].y, moving_item, 0) == false ) {
                                // fixme add item back
                                u.i_add(moving_item);
                                popup(_("Destination full. Please report a bug if items have vanished."));
                                continue;
                            }
                        }
                        u.moves -= 100;
                    }
                } else {
                    item moving_item = u.inv.remove_item(item_pos);
                    if(squares[destarea].vstor >= 0) {
                        if(squares[destarea].veh->add_item(squares[destarea].vstor, moving_item) == false) {
                            // fixme add item back (test)
                            u.i_add(moving_item);
                            popup(_("Destination full. Please report a bug if items have vanished."));
                            continue;
                        }
                    } else {
                        if(m.add_item_or_charges(squares[destarea].x, squares[destarea].y, moving_item) == false) {
                            // fixme add item back (test)
                            u.i_add(moving_item);
                            popup(_("Destination full. Please report a bug if items have vanished."));
                            continue;
                        }
                    }
                    u.moves -= 100;
                }
                // from map / vstor
            } else {
                int s;
                if(panes[src].area == isall) {
                    s = panes[src].items[list_pos].area;
                    // todo: phase out these vars? ---v // temp_fudge pending tests/cleanup
                    panes[src].offx = squares[s].offx;
                    panes[src].offy = squares[s].offy;
                    panes[src].vstor = squares[s].vstor;
                    panes[src].veh = squares[s].veh;
                    recalc = true;
                } else {
                    s = panes[src].area;
                }
                if ( s == destarea ) {
                    popup(_("Source area is the same as destination (%s)."), squares[destarea].name.c_str());
                    continue;
                }
                item *it = panes[src].items[list_pos].it;

                if ( it->made_of(LIQUID) ) {
                    popup(_("You can't pick up a liquid."));
                    continue;
                } else {// from veh/map
                    long trycharges = -1;
                    if ( destarea == isinventory ) { // if destination is inventory
                        if (!u.can_pickup(true)) {
                            if (!showmsg) {
                                redraw = showmsg = true;
                            }
                            continue;
                        }
                        if(squares[destarea].size >= MAX_ITEM_IN_SQUARE) {
                            popup(_("Too many items."));
                            continue;
                        }
                        int tryvolume = it->volume();
                        int tryweight = it->weight();
                        int amount = 1;
                        if ( it->count_by_charges() && it->charges > 1 ) {
                            amount = it->charges;
                            int unitvolume = it->precise_unit_volume();
                            int unitweight = ( tryweight * 1000 ) / it->charges;
                            int max_vol = u.volume_capacity() - u.volume_carried();
                            int max_weight = ( u.weight_capacity() * 4 ) - u.weight_carried();
                            max_vol *= 1000;
                            max_weight *= 1000;
                            int max = amount;
                            if ( unitvolume > 0 && unitvolume * amount > max_vol ) {
                                max = int( max_vol / unitvolume );
                            }
                            if ( unitweight > 0 && unitweight * amount > max_weight ) {
                                max = int( max_weight / unitweight );
                            }
                            // popup("uvol: %d amt: %d mvol: %d mamt: %d", unitvolume, amount, max_vol, max);
                            if ( max != 0 ) {
                                std::string popupmsg = _("How many do you want to move? (0 to cancel)");
                                if(amount > max) {
                                    popupmsg = string_format(_("Destination can only hold %d! Move how many? (0 to cancel) "), max);
                                    moveall = false;
                                }
                                // fixme / todo make popup take numbers only (m = accept, q = cancel)
                                if ( !moveall ) {
                                    amount = helper::to_int(
                                                 string_input_popup( popupmsg, 20,
                                                                     helper::to_string_int(
                                                                         ( amount > max ? max : amount )
                                                                     ), "", "", -1, true//input only digits
                                                                   )
                                             );
                                    if ( amount > max ) {
                                        amount = max;
                                    }
                                } else {
                                    amount = max;
                                }
                                if ( amount != it->charges ) {
                                    tryvolume = ( unitvolume * amount ) / 1000;
                                    tryweight = ( unitweight * amount ) / 1000;
                                    trycharges = amount;
                                }
                                if ( trycharges == 0 ) {
                                    continue;
                                }
                            } else {
                                continue;
                            }
                        }
                        // ...not even going to think about checking for stack
                        // at this time...
                        if(!u.can_pickVolume(tryvolume)) {
                            popup(_("There's no room in your inventory."));
                            continue;
                        } else if (!u.can_pickWeight(tryweight, false)) {
                            popup(_("This is too heavy!"));
                            continue;
                        }
                    }
                    recalc = true;

                    item new_item = (*it);

                    if ( trycharges > 0 ) {
                        new_item.charges = trycharges;
                    }
                    if(destarea == isinventory) {
                        u.inv.assign_empty_invlet(new_item);
                        u.i_add(new_item);
                        u.moves -= 100;
                    } else if (squares[destarea].vstor >= 0) {
                        if( squares[destarea].veh->add_item( squares[destarea].vstor, new_item ) == false) {
                            popup(_("Destination area is full. Remove some items first"));
                            continue;
                        }
                    } else {
                        if ( m.add_item_or_charges(squares[destarea].x, squares[destarea].y, new_item, 0 ) == false ) {
                            popup(_("Destination area is full. Remove some items first"));
                            continue;
                        }
                    }
                    if ( trycharges > 0 ) {
                        it->charges -= trycharges;
                    } else {
                        if (panes[src].vstor >= 0) {
                            panes[src].veh->remove_item (panes[src].vstor, it);
                        } else {
                            m.i_rem(u.posx + panes[src].offx, u.posy + panes[src].offy, it);
                        }
                    }
                }
            }
        } else if (',' == c) {
            if (move_all_items() && OPTIONS["CLOSE_ADV_INV"] == true) {
                exit = true;
            }
            recalc = true;
            redraw = true;
        } else if ('?' == c) {
            showmsg = (!showmsg);
            checkshowmsg = false;
            redraw = true;
        } else if ('s' == c) {
            redraw = true;
            uimenu sm; /* using new uimenu class */
            sm.text = _("Sort by... ");
            sm.entries.push_back(uimenu_entry(SORTBY_NONE, true, 'u', _("Unsorted (recently added first)") ));
            sm.entries.push_back(uimenu_entry(SORTBY_NAME, true, 'n', sortnames[SORTBY_NAME]));
            sm.entries.push_back(uimenu_entry(SORTBY_WEIGHT, true, 'w', sortnames[SORTBY_WEIGHT]));
            sm.entries.push_back(uimenu_entry(SORTBY_VOLUME, true, 'v', sortnames[SORTBY_VOLUME]));
            sm.entries.push_back(uimenu_entry(SORTBY_CHARGES, true, 'x', sortnames[SORTBY_CHARGES]));
            sm.entries.push_back(uimenu_entry(SORTBY_CATEGORY, true, 'c', sortnames[SORTBY_CATEGORY]));
            sm.selected = panes[src].sortby - 1; /* pre-select current sort. uimenu.selected is entries[index] (starting at 0), not return value */
            sm.query(); /* calculate key and window variables, generate window, and loop until we get a valid answer */
            if (sm.ret < 1) {
                continue; /* didn't get a valid answer =[ */
            }
            panes[src].sortby = sm.ret;

            if ( src == left ) {
                uistate.adv_inv_leftsort = sm.ret;
            } else {
                uistate.adv_inv_rightsort = sm.ret;
            }
            recalc = true;
        } else if( 'f' == c || '.' == c || '/' == c) {
            long key = 0;
            int spos = -1;
            std::string filter = panes[src].filter;
            filter_edit = true;

            do {
                mvwprintz(panes[src].window, getmaxy(panes[src].window) - 1, 2, c_cyan, "< ");
                mvwprintz(panes[src].window, getmaxy(panes[src].window) - 1, (w_width / 2) - 3, c_cyan, " >");
                filter = string_input_win(panes[src].window, panes[src].filter, 256, 4,
                                          w_height - 1, (w_width / 2) - 4, false, key, spos, "",
                                          4, getmaxy(panes[src].window) - 1);
                if ( filter != panes[src].filter ) {
                    panes[src].filtercache.clear();
                    panes[src].filter = filter;
                    recalc_pane(src);
                    redraw_pane(src);
                }
            } while(key != '\n' && key != KEY_ESCAPE);
            filter_edit = false;
            redraw = true;
        } else if('r' == c) {
            panes[src].filter = "";
            recalc_pane(src);
            redraw_pane(src);
            redraw = true;
        } else if('p' == c) {
            if(panes[src].size == 0) {
                continue;
            } else if ( item_pos == -8 ) {
                continue; // category header
            }
            if ( panes[src].items[list_pos].autopickup == true ) {
                removePickupRule(panes[src].items[list_pos].name);
                panes[src].items[list_pos].autopickup = false;
            } else {
                addPickupRule(panes[src].items[list_pos].name);
                panes[src].items[list_pos].autopickup = true;
            }
            redraw = true;
        } else if('e' == c) {
            if(panes[src].size == 0) {
                continue;
            } else if ( item_pos == -8 ) {
                continue; // category header
            }
            item *it = panes[src].items[list_pos].it;
            int ret = 0;
            if(panes[src].area == isinventory ) {
                ret = g->inventory_item_menu( item_pos, colstart + ( src == left ? w_width / 2 : 0 ),
                                              w_width / 2, (src == right ? 0 : -1) );
                // if player has started an activaity, leave the screen and process it
                if (!g->u.has_activity(ACT_NULL)) {
                    exit = true;
                }
                // Might have changed at stack (activated an item)
                g->u.inv.restack(&g->u);
                recalc = true;
                checkshowmsg = true;
            } else {
                std::vector<iteminfo> vThisItem, vDummy;
                it->info(true, &vThisItem);
                int rightWidth = w_width / 2;
                vThisItem.push_back(iteminfo(_("DESCRIPTION"), "\n"));
                vThisItem.push_back(iteminfo(_("DESCRIPTION"),
                                             center_text(_("[up / page up] previous"),
                                                     rightWidth - 4)));
                vThisItem.push_back(iteminfo(_("DESCRIPTION"),
                                             center_text(_("[down / page down] next"),
                                                     rightWidth - 4)));
                ret = draw_item_info(colstart + ( src == left ? w_width / 2 : 0 ),
                                     rightWidth, 0, 0, it->tname(), vThisItem, vDummy );
            }
            if ( ret == KEY_NPAGE || ret == KEY_DOWN ) {
                changey += 1;
                lastCh = 'e';
            } else if ( ret == KEY_PPAGE || ret == KEY_UP ) {
                changey += -1;
                lastCh = 'e';
            } else {
                lastCh = 0;
                redraw = true;
            };
        } else if( 'q' == c || KEY_ESCAPE == c) {
            exit = true;
        } else if('>' == c || KEY_NPAGE == c) {
            if ( inCategoryMode ) {
                changey = 1;
            } else {
                panes[src].page++;
                if( panes[src].page >= panes[src].max_page ) {
                    panes[src].page = 0;
                }
                redraw = true;
            }
        } else if('<' == c || KEY_PPAGE == c) {
            if ( inCategoryMode ) {
                changey = -1;
            } else {
                panes[src].page--;
                if( panes[src].page < 0 ) {
                    panes[src].page = panes[src].max_page;
                }
                redraw = true;
            }
        } else {
            switch(c) {
            case 'j':
            case KEY_DOWN:
                changey = 1;
                break;
            case 'k':
            case KEY_UP:
                changey = -1;
                break;
            case 'h':
            case KEY_LEFT:
                changex = 0;
                break;
            case 'l':
            case KEY_RIGHT:
                changex = 1;
                break;
            case '\t':
                changex = dest;
                break;
            default :
                donothing = true;
                break;
            }
        }
        if(!donothing) {
            if ( changey != 0 ) {
                for ( int l = 2; l > 0; l-- ) {
                    int new_index = panes[src].index;

                    if (panes[src].sortby == SORTBY_CATEGORY &&
                        !category_index_start.empty() && inCategoryMode) {
                        int prev_cat = 0, next_cat = 0, selected_cat = 0;

                        for (unsigned curr_cat = 0; curr_cat < category_index_start.size(); ++curr_cat) {
                            int next_cat_start = curr_cat + 1 < category_index_start.size() ?
                                                 curr_cat + 1 : category_index_start.size() - 1;
                            int actual_index = panes[src].index + panes[src].page * itemsPerPage;

                            if (actual_index >= category_index_start[curr_cat] &&
                                actual_index <= category_index_start[next_cat_start]) {
                                selected_cat = curr_cat;

                                prev_cat = (int(curr_cat) - 1) >= 0 ? curr_cat - 1 :
                                           category_index_start.size() - 1;
                                prev_cat = category_index_start[selected_cat] < actual_index ?
                                           selected_cat : prev_cat;

                                next_cat = (curr_cat + 1) < category_index_start.size() ? curr_cat + 1 : 0;
                            }
                        }

                        if (changey > 0) {
                            panes[src].page = category_index_start[next_cat] / itemsPerPage;
                            new_index = category_index_start[next_cat] % itemsPerPage;
                        } else {
                            panes[src].page = category_index_start[prev_cat] / itemsPerPage;
                            new_index = category_index_start[prev_cat] % itemsPerPage;

                            panes[src].max_index = panes[src].page < panes[src].max_page - 1 ?
                                                   itemsPerPage : panes[src].items.size() % itemsPerPage;
                        }
                    } else {
                        new_index = panes[src].index + changey;
                    }

                    panes[src].index = new_index;
                    if ( panes[src].index < 0 ) {
                        panes[src].page--;
                        if( panes[src].page < 0 ) {
                            panes[src].page = panes[src].max_page - 1;
                            panes[src].index = panes[src].items.size() - 1 - ( panes[src].page * itemsPerPage );
                        } else {
                            panes[src].index = itemsPerPage - 1; // corrected at the start of next iteration
                            if ( panes[src].items[list_pos - 1].idx == -8 ) panes[src].index--; // If the previous item would be a category header at the end of the previous page, we actually have to go back again.
                        }
                    } else if ( panes[src].index >= panes[src].max_index ) {
                        panes[src].page++;
                        if( panes[src].page >= panes[src].max_page ) {
                            panes[src].page = 0;
                        }
                        panes[src].index = 0;
                    }
                    unsigned lpos = panes[src].index + (panes[src].page * itemsPerPage);
                    if ( lpos < panes[src].items.size() && panes[src].items[lpos].volume != -8 ) {
                        l = 0;
                    }

                }
                panes[src].redraw = true;
            }
            if ( changex >= 0 ) {
                src = changex;
                redraw = true;
            }
        }
    }

    uistate.adv_inv_last_coords.x = u.posx;
    uistate.adv_inv_last_coords.y = u.posy;
    uistate.adv_inv_leftarea = panes[left].area;
    uistate.adv_inv_rightarea = panes[right].area;
    uistate.adv_inv_leftindex = panes[left].index;
    uistate.adv_inv_leftpage = panes[left].page;
    uistate.adv_inv_rightindex = panes[right].index;
    uistate.adv_inv_rightpage = panes[right].page;

    uistate.adv_inv_leftfilter = panes[left].filter;
    uistate.adv_inv_rightfilter = panes[right].filter;

    werase(head);
    werase(panes[left].window);
    werase(panes[right].window);
    delwin(head);
    delwin(panes[left].window);
    delwin(panes[right].window);
    g->refresh_all();
}

AdvancedInventory::InventoryPane::InventoryPane(std::string id, player *p) : InventoryPane(id, p->inv, p->volume_capacity(), p->weight_capacity()) { }

bool AdvancedInventory::selectPane(std::string id, SelectedPane area) {
  if (area == SelectedPane::None)
    area = _selectedPane;

  Pane *candidate = _panes[id];

  if (candidate == nullptr) {
    return false;
  } else if (_selections[SelectedPane::Left] == candidate || _selections[SelectedPane::Right] == candidate) {
    std::swap(_selections[SelectedPane::Left], _selections[SelectedPane::Right]);
    return true;
  } else {
    _selections[area] = candidate;
    return true;
  }
}

AdvancedInventory::AdvancedInventory(player *p, player *o) {
  if (o == nullptr) {
    _mode = Mode::Area;
  } else {
    _mode = Mode::Linear;
  }

  if (_mode == Mode::Area) {
    // set up the local area

    for (auto dir : helper::planarDirections) {
      std::string id(1, helper::directionToNumpad(dir));
      point mapTile(helper::directionToPoint(dir) + p->pos());

      int part_num;
      vehicle *veh(g->m.veh_at(mapTile, part_num));

      if (veh != nullptr) {
        part_num = veh->part_with_feature(part_num, "CARGO");

        if (part_num != -1) {
          _panes[id] = new ItemVectorPane(id, veh->parts[part_num].items, veh->max_volume(part_num), -1);
          _panes[id]->chevrons("< >");
          continue;
        }
      }

      _panes[id] = new ItemVectorPane(id, g->m.i_at(mapTile), g->m.max_volume(mapTile), -1);
    }

    // set up the ALL tab
    _panes["A"] = new AggregatePane("A", _panes);

    // Set up the inventory
    _panes["I"] = new InventoryPane("I", p);

    // set up the dragged inventory
    _panes["D"] = nullptr;

    helper::Direction gDir(helper::pointToDirection(p->grab_point));

    if (gDir != helper::Direction::Center) {
      _panes["D"] = _panes[std::string(1, directionToNumpad(gDir))];
    }

    // Set initial panes; for now they are as default
    selectPane("9", SelectedPane::Left);
    selectPane("6", SelectedPane::Right);
  }
}

void AdvancedInventory::Pane::restack() {
  _dirtyStack = false;

  if (_cursor >= _stackedItems.size())
    _cursor = _stackedItems.size() - 1;

  clampCursor();

  if (_sortRule > SortRule::Unsorted) {
    ItemCompare cmp(_sortRule, _sortAscending);

    std::sort(_stackedItems.begin(), _stackedItems.end(), cmp);
  } else if (!_sortAscending) {
    std::reverse(_stackedItems.begin(), _stackedItems.end());
  }
}

void AdvancedInventory::InventoryPane::restack () {
  _stackedItems.clear();

  invslice slice = _inv.slice();

  for (auto &iList : slice) {
    _stackedItems.push_back(std::list<item *>());

    for (auto &item : *iList) {
      _stackedItems.back().push_back(&item);
    }
  }

  Pane::restack();
}

void AdvancedInventory::ItemVectorPane::restack () {
  _stackedItems.clear();

  for (auto &item : _inv) {
    bool stacked = false;

    for (auto &stack : _stackedItems) {
      if (item.stacks_with(*(stack.front()))) {
        stack.push_back(&item);

        stacked = true;
        break;
      }
    }

    if (!stacked)
      _stackedItems.push_back({&item});
  }

  Pane::restack();
}

void AdvancedInventory::AggregatePane::restack () {
  _dirtyStack = false;

  std::vector<std::pair<std::list<item *>, Pane *>> keyedStacks;

  for (auto paneP : _panes) {
    Pane *pane(paneP.second);

    for (auto &iList : pane->stackedItems()) {
      std::pair<std::list<item *>, Pane *> iPair(iList, pane);

      keyedStacks.push_back(iPair);
    }
  }

  if (_sortRule > SortRule::Unsorted) {
    ItemCompare cmp(_sortRule, _sortAscending);

    std::sort(keyedStacks.begin(), keyedStacks.end(), cmp);
  } else if (!_sortAscending) {
    std::reverse(keyedStacks.begin(), keyedStacks.end());
  }

  _stackedItems.clear();
  _stackedItemsPane.clear();

  for (auto &pair : keyedStacks) {
    _stackedItems.push_back(pair.first);
    _stackedItemsPane.push_back(pair.second);
  }

  if (_cursor >= _stackedItems.size())
    _cursor = _stackedItems.size() - 1;

  clampCursor();
}

int AdvancedInventory::AggregatePane::maxVolume () {
  if (_maxVolume == -1)
    _maxVolume = const_cast<const AggregatePane *>(this)->maxVolume();

  return _maxVolume;
}

int AdvancedInventory::AggregatePane::maxVolume () const {
  if (_maxVolume == -1) {
    int mV = 0;

    for (auto &pair : _panes) {
      mV += pair.second->maxVolume();
    }

    return mV;
  } else {
    return _maxVolume;
  }
}

int AdvancedInventory::AggregatePane::maxWeight () {
  if (_maxWeight == -1)
    _maxWeight = const_cast<const AggregatePane *>(this)->maxWeight();

  return _maxWeight;
}

int AdvancedInventory::AggregatePane::maxWeight () const {
  if (_maxWeight == -1) {
    int mW = 0;

    for (auto &pair : _panes) {
      mW += pair.second->maxWeight();
    }

    return mW;
  } else {
    return _maxWeight;
  }
}

int AdvancedInventory::AggregatePane::volume () const {
  int volume = 0;

  for (auto &pair : _panes) {
    volume += pair.second->volume();
  }

  return volume;
}

int AdvancedInventory::AggregatePane::weight () const {
  int weight = 0;

  for (auto &pair : _panes) {
    weight += pair.second->weight();
  }

  return weight;
}

int AdvancedInventory::InventoryPane::volume () const {
  return _inv.volume();
}

int AdvancedInventory::InventoryPane::weight () const {
  return _inv.weight();
}

int AdvancedInventory::ItemVectorPane::volume () const {
  int volume = 0;

  for (auto &item : _inv) {
    volume += item.volume();
  }

  return volume;
}

int AdvancedInventory::ItemVectorPane::weight () const {
  int weight = 0;

  for (auto &item : _inv) {
    weight += item.weight();
  }

  return weight;
}

void AdvancedInventory::display (player *p, player *o) {
  AdvancedInventory advInv(p, o);

  int head_height(5);
  int min_w_height(10);
  int min_w_width(FULL_SCREEN_WIDTH);
  int max_w_width(120);

  int w_height((TERMY < min_w_height + head_height) ? min_w_height : TERMY - head_height);
  int w_width((TERMX < min_w_width) ? min_w_width : (TERMX > max_w_width) ? max_w_width : (int)TERMX);

  advInv.w_height = w_height;
  advInv.w_width = w_width;

  int headstart(0);
  int colstart((TERMX > w_width) ? (TERMX - w_width) / 2 : 0);

  WINDOW *head = newwin(head_height, w_width, headstart, colstart);
  WINDOW *left_window = newwin(w_height, w_width / 2, headstart + head_height, colstart);
  WINDOW *right_window = newwin(w_height, w_width / 2, headstart + head_height, colstart + w_width / 2);

  size_t itemsPerPage(w_height - ADVINVOFS);

  while (1) { // input loop
    advInv.displayHead(head);
    advInv.displayPanes(left_window, right_window);

    int input(getch());
    helper::Direction inputDir(helper::movementKeyToDirection(input));

    switch (inputDir) {
    case helper::Direction::None: // further processing is needed
      break;
    case helper::Direction::Up:
      advInv.selectedPane()->pageUp(itemsPerPage);
      continue;
    case helper::Direction::Down:
      advInv.selectedPane()->pageDown(itemsPerPage);
      continue;

    default: // Received a viable direction; switch focus
      advInv.selectPane(std::string(1, directionToNumpad(inputDir)));
    }

    switch (input) {
    case KEY_DOWN:
      advInv.selectedPane()->down();
      break;
    case KEY_UP:
      advInv.selectedPane()->up();
      break;
    case KEY_LEFT:
      advInv.left();
      break;
    case KEY_RIGHT:
      advInv.right();
      break;
    case '\t':
      advInv.swapFocus();
      break;
    case 'm':
      advInv.moveItem();
      break;
    case 'M':
    case KEY_ENTER:
    case '\n':
    case '\r':
      advInv.moveAll();
      break;
    case ',':
      advInv.moveALL();
      break;
    case 's':
      advInv.sort();
      break;
    case 'd':
    case 'D':
      advInv.selectPane("D");
      break;
    case 'a':
    case 'A':
      advInv.selectPane("A");
      break;
    case '0':
    case 'i':
    case 'I':
      advInv.selectPane("I");
      break;
    }

    if (input == KEY_ESCAPE)
      return;
  }

  delwin(head), delwin(left_window), delwin(right_window);
}

void AdvancedInventory::left () {
  if (_selectedPane != SelectedPane::Left)
    swapFocus();
}

void AdvancedInventory::right () {
  if (_selectedPane != SelectedPane::Right)
    swapFocus();
}

void AdvancedInventory::displayHead (WINDOW *head) {
  werase(head);
  draw_border(head);

  mvwprintz(head, 0, w_width - utf8_width(_("< [?] show log >")) - 2, c_white, _("< [?] show log >"));
  mvwprintz(head, 1, 2, c_white, _("hjkl or arrow keys to move cursor, [m]ove item between panes ([M]: all)"));
  mvwprintz(head, 2, 2, c_white, _("1-9 to select square for active tab, 0 for inventory, D for dragged item,"));
  mvwprintz(head, 3, 2, c_white, _("[e]xamine, [s]ort, toggle auto[p]ickup, [,] to move all items, [q]uit."));

  wrefresh(head);
}

void AdvancedInventory::displayPanes (WINDOW *lWin, WINDOW *rWin) {
  // TODO: fix this for linear mode
  _selections[SelectedPane::Left]->draw(lWin, _selectedPane == SelectedPane::Left);
  _selections[SelectedPane::Right]->draw(rWin, _selectedPane == SelectedPane::Right);

  drawAreaIndicator(lWin, _selectedPane == SelectedPane::Left);
  drawAreaIndicator(rWin, _selectedPane == SelectedPane::Right);
}

void AdvancedInventory::Pane::draw (WINDOW *window, bool active) {
  werase(window);
  draw_border(window);

  size_t columns(getmaxx(window));
  size_t itemsPerPage(getmaxy(window) - ADVINVOFS);

  std::string spaces(columns - 4, ' ');

  nc_color norm = active ? c_white : c_dkgray;

  int v(volume()), w(weight());
  int mV = maxVolume(), mW = maxWeight();

  double dWeight(helper::convertWeight(w));
  double dMaxWeight(helper::convertWeight(mW));

  // pagination
  int page(_cursor / itemsPerPage);
  int pages(stackedItems().size() / itemsPerPage + 1);

  if (active && pages > 1)
    mvwprintz(window, 4, 2, c_ltblue, "[<] page %d of %d [>]", page + 1, pages);

  // sorting info

  const char *sortName = _("none");

  switch (_sortRule) {
  case SortRule::Weight:
    sortName = _("weight");
    break;
  case SortRule::Volume:
    sortName = _("volume");
    break;
  case SortRule::Name:
    sortName = _("name");
    break;
  case SortRule::Charges:
    sortName = _("charges");
    break;
  }

  mvwprintz(window, 0, 4, norm, "< [s]ort: %s (%s)>", sortName, _sortAscending ? "^" : "v" );

  // header
  nc_color warnColor = norm;

  if (mW >= 0 && dWeight > dMaxWeight)
    warnColor = c_red;

  mvwprintz(window, 4, columns - 29, warnColor, "W: %4.1f", dWeight);

  if (mW >= 0)
    wprintz(window, norm, "/%4.1f ", dMaxWeight);
  else
    wprintz(window, norm, "/---- ");

  warnColor = norm;

  if (v > mV)
    warnColor = c_red;

  wprintz(window, warnColor, "V: %5.1d", v);
  wprintz(window, norm, "/%5.1d", mV);

  //print header row and determine max item name length
  const int lastcol = columns - 2; // Last printable column

  const int table_hdr_len1 = utf8_width(_("amt weight vol")); // Header length type 1

  mvwprintz( window, 5, 3, c_ltgray, _("Name (charges)") );
  mvwprintz( window, 5, lastcol - table_hdr_len1 + 1, c_ltgray, _("amt weight vol") );

  size_t x = 0;

  for (auto item = stackedItems().begin() + page * itemsPerPage; item != stackedItems().end() && item != stackedItems().begin() + (page + 1 ) * itemsPerPage; item++, x++) {
    wmove(window, 6 + x, 1);
    printItem(window, *item, active, x == _cursor % itemsPerPage);
  }
}

void AdvancedInventory::Pane::printItem(WINDOW *window, const std::list<item *> &iList, bool active, bool highlighted) {
  const item *item = iList.front();

  size_t columns(getmaxx(window));

  const int lastcol = columns - 2; // Last printable column
  const size_t name_startpos = 4;
  const size_t amt_startpos = lastcol - 14;
  int max_name_length = amt_startpos - name_startpos - 1; // Default name length

  nc_color norm = active ? c_white : c_dkgray;

  nc_color thiscolor = active ? item->color(&g->u) : norm;
  nc_color thiscolordark = c_dkgray;
  nc_color errorColor = (active && highlighted) ? hilite(c_red) : c_red;
  nc_color print_color = thiscolor;

  if (highlighted) {
    if (active) {
      thiscolor = hilite(thiscolor);
      thiscolordark = hilite(thiscolordark);
    }

    wprintz(window, thiscolor, ">> ");
  } else {
    wprintz(window, thiscolor, "   ");
  }

  //print item name
  std::string it_name = utf8_truncate(item->display_name(), max_name_length);
  wprintz(window, thiscolor, "%-*.*s ", max_name_length, max_name_length, it_name.c_str() );

  //print "amount" column
  int it_amt = iList.size();
  if( it_amt > 1 ) {
    print_color = thiscolor;

    if (it_amt > 9999) {
      it_amt = 9999;
      print_color = errorColor;
    }

    wprintz(window, print_color, "%4d ", it_amt);
  } else {
    wprintz(window, thiscolor, "     ");
  }

  //print weight column
  double it_weight = helper::convertWeight(item->weight()) * it_amt;
  size_t w_precision;
  print_color = (it_weight > 0) ? thiscolor : thiscolordark;

  if (it_weight >= 1000.0) {
    if (it_weight >= 10000.0) {
      print_color = errorColor;
      it_weight = 9999.0;
    }
    w_precision = 0;
  } else if (it_weight >= 100.0) {
    w_precision = 1;
  } else {
    w_precision = 2;
  }

  wprintz(window, print_color, "%5.*f ", w_precision, it_weight);

  //print volume column
  int it_vol = item->volume() * it_amt;
  print_color = (it_vol > 0) ? thiscolor : thiscolordark;
  if (it_vol > 9999) {
    it_vol = 9999;
    print_color = errorColor;
  }

  wprintz(window, print_color, "%4d", it_vol );
}

void AdvancedInventory::drawIndicatorAtom (WINDOW *window, std::string id, point location, bool active) const {
  Pane *pane(_panes.at(id));
  bool selected(active ? selectedPane() == pane : unselectedPane() == pane);

  if (pane != nullptr) {
    mvwprintz(window, location.y, location.x, selected ? c_cyan : active ? c_white : c_ltgray, pane->chevrons().c_str());
    mvwprintz(window, location.y, location.x + 1, selected ? c_green : active ? c_white : c_ltgray, id.c_str());
  } else {
    mvwprintz(window, location.y, location.x, c_red, "[%1.1s]", id.c_str());
  }
}

void AdvancedInventory::drawAreaIndicator (WINDOW *window, bool active) const {
  for (auto dir : helper::planarDirections) {
    point loc(helper::directionToPoint(dir));
    std::string id(1, helper::directionToNumpad(dir));

    drawIndicatorAtom(window, id, {areaIndicatorRoot.x + loc.x * 3, areaIndicatorRoot.y + loc.y}, active);
  }

  drawIndicatorAtom(window, "D", {linearIndicatorRoot.x, linearIndicatorRoot.y - 1}, true);
  drawIndicatorAtom(window, "I", {linearIndicatorRoot.x, linearIndicatorRoot.y}, active);
  drawIndicatorAtom(window, "A", {linearIndicatorRoot.x, linearIndicatorRoot.y + 1}, active);

  wrefresh(window);
}

bool AdvancedInventory::Pane::canTakeItem(const item *item, size_t &count) {
  if (item == nullptr)
    return false;

  do {
    if (item->volume() * count <= freeVolume())
      return true;
  } while (--count != 0);

  return false;
}

void AdvancedInventory::Pane::popItemAtCursor () {
  _stackedItems[_cursor].pop_front();
}

void AdvancedInventory::AggregatePane::addItem (const item *item) {

}

void AdvancedInventory::AggregatePane::removeItem (const item *item) {

}

void AdvancedInventory::InventoryPane::addItem (const item *item) {
  _inv.add_item(*item);

  _dirtyStack = true;
}

void AdvancedInventory::InventoryPane::removeItem (const item *item) {
  _inv.remove_item(item);

  popItemAtCursor();
  _dirtyStack = true;
}

void AdvancedInventory::ItemVectorPane::addItem (const item *item) {
  _dirtyStack = true;

  if (item->count_by_charges()) {
    for (auto &i : _inv) {
      if (i.stacks_with(*item)) {
        i.charges += item->charges;

        return;
      }
    }
  }

  _inv.push_back(*item);
}

void AdvancedInventory::ItemVectorPane::removeItem (const item *item) {
  for (auto i = _inv.begin(); i != _inv.end(); i++) {
    if (i->stacks_with(*item)) {
      _inv.erase(i);

      popItemAtCursor();
      _dirtyStack = true;

      return;
    }
  }
}

const item *AdvancedInventory::Pane::itemAtCursor (size_t &count) const {
  if (_stackedItems.size() == 0)
    return nullptr;

  std::list<item *> cursorItems(_stackedItems[_cursor]);

  count = cursorItems.size();

  return cursorItems.front();
}

bool AdvancedInventory::moveItem () {
  size_t count;
  const item *toMove = selectedPane()->itemAtCursor(count);

  if (unselectedPane()->canTakeItem(toMove, count)) {
    unselectedPane()->addItem(toMove);
    selectedPane()->removeItem(toMove);

    return true;
  }

  return false;
}

bool AdvancedInventory::moveAll () {
  size_t count;
  const item *toMove = selectedPane()->itemAtCursor(count);

  if (unselectedPane()->canTakeItem(toMove, count)) {
    while (count-- != 0) {
      moveItem();
    }

    return true;
  }

  return false;
}

void AdvancedInventory::moveALL () {
  selectedPane()->_cursor = 0;

  while (moveAll()) {
    selectedPane()->restack();
  };
}

void AdvancedInventory::sort () {
  uimenu sm;

  sm.text = _("Sort by... ");

  sm.entries.push_back(uimenu_entry((int)SortRule::Unsorted, true, 'u', _("Unsorted (recently added first)") ));
  sm.entries.push_back(uimenu_entry((int)SortRule::Name, true, 'n', _("name")));
  sm.entries.push_back(uimenu_entry((int)SortRule::Weight, true, 'w', _("weight")));
  sm.entries.push_back(uimenu_entry((int)SortRule::Volume, true, 'v', _("volume")));
  sm.entries.push_back(uimenu_entry((int)SortRule::Charges, true, 'x', _("charges")));

  sm.query();

  SortRule ret = (SortRule)sm.ret;

  selectedPane()->sortRule(ret);
  selectedPane()->restack();
}

bool AdvancedInventory::Pane::ItemCompare::operator() (const std::list<item *> &a, const std::list<item *> &b) const {
  const item &d1(*(a.front()));
  const item &d2(*(b.front()));

  int compa = 0, compb = 0;

  switch (_rule) {
  case SortRule::Weight:
    compa = d1.weight(), compb = d2.weight();
    break;

  case SortRule::Volume:
    compa = d1.volume(), compb = d2.volume();
    break;

  case SortRule::Charges:
    compa = d1.charges, compb = d2.charges;
    break;
  }

  if (compb == compa) {
    std::string n1(d1.tname(false)), n2(d2.tname(false));
    if (n1 == n2) {
      n1 = d1.tname(), n2 = d2.tname();
    }

    bool retval = std::lexicographical_compare( n1.begin(), n1.end(), n2.begin(), n2.end(), advanced_inv_sort_case_insensitive_less() );
    return _ascending ? retval : !retval;
  } else {
    return _ascending ? compa < compb : compa > compb;
  }
}

bool AdvancedInventory::Pane::ItemCompare::operator()(const std::pair<const std::list<item *> &, Pane *> &a, const std::pair<const std::list<item *> &, Pane *> &b) const {
  return operator()(a.first, b.first);
}
