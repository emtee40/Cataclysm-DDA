#ifndef CATA_UI_H
#define CATA_UI_H

#include "output.h"
#include "enums.h"

#include <utility>
#include <list>
#include <string>

typedef int nc_color;

/**
 * \defgroup Cata_ui "Cataclysm's ui framework."
 *
 * As you might expect, the ui framework is implemented with the composite pattern.
 * This allows us to nest some ui elements within another. We only need to call 'render'
 * on the top level object, and the composite pattern takes care of the rest.
 * @{
 */

/**
* Class that represents a rectangle.
*
*  Holds dimensions and position.
*/
struct ui_rect {
    // Size of the rect.
    size_t size_x, size_y;
    //position of the rect, as offsets to the anchor. (within the parent)
    int x, y;

    ui_rect( size_t size_x, size_t size_y, int x, int y );
};

class window_buffer {
    private:
        WINDOW *current;
        WINDOW *buffer;
    public:
        window_buffer(size_t size_x, size_t size_y, unsigned int x, unsigned int y);
        ~window_buffer();

        WINDOW *get_buffer() const;
        void flush();
};

class ui_window;

enum ui_anchor {
    top_left,
    top_center,
    top_right,
    center_left,
    center_center,
    center_right,
    bottom_left,
    bottom_center,
    bottom_right
};

/**
* Most basic ui element.
*
* This abstract class is used to implement the frameworks's composite pattern.
*/
class ui_element {
    friend class ui_window; // so we don't have to make draw and set_parent public
    private:
        const ui_window *parent;
        virtual void set_parent(const ui_window *parent);

        ui_anchor anchor = top_left;

        unsigned int anchored_x, anchored_y;

        bool show = true;
        ui_rect rect;
        void calc_anchored_values();
    protected:
        virtual void draw() = 0;
        virtual WINDOW *get_win() const;
        virtual bool is_window() const { return false; }
    public:
        ui_element(size_t size_x, size_t size_y, int x = 0, int y = 0, ui_anchor anchor = top_left);
        ui_element(const ui_rect &o_rect);
        virtual ~ui_element() = default;

        const ui_rect &get_rect() const;
        virtual void set_rect(const ui_rect &new_rect);

        virtual void set_anchor(ui_anchor new_anchor);

        virtual void set_visible(bool visible);
        virtual bool is_visible() const;

        unsigned int get_ax() const;
        unsigned int get_ay() const;

        void above(const ui_element &other, int x = 0, int y = 0);
        void below(const ui_element &other, int x = 0, int y = 0);
        void after(const ui_element &other, int x = 0, int y = 0);
        void before(const ui_element &other, int x = 0, int y = 0);
};

/**
* The basis for a ui composition.
*
* This is the class in the framework that holds nested elements.
* It is also the only class in the framework with a public 'draw' function (render).
*/
class ui_window : public ui_element {
    friend class ui_element;
    private:
        void set_parent( const ui_window *parent ) override;

        int global_x, global_y;

        std::list<ui_element *> children;

        void adjust_window();
        window_buffer win;
        void draw_children();
        void draw_window_children();
    protected:
        WINDOW *get_win() const override;
        virtual void local_draw() {}
        size_t child_count() const;
        const std::list<ui_element *> &get_children() const;
        virtual bool is_window() const override { return true; }
    public:
        ui_window(size_t size_x, size_t size_y, int x = 0, int y = 0, ui_anchor anchor = top_left);
        // TODO : add copy constructor
        ~ui_window() override;

        void draw() override;

        virtual void set_anchor(ui_anchor new_anchor) override;

        virtual void set_rect(const ui_rect &new_rect) override;
        virtual void add_child( ui_element *child );
};

/**
* A simple text label
*/
class ui_label : public ui_element {
    private:
        std::string text;
    protected:
        virtual void draw() override;
    public:
        ui_label( std::string text, int x = 0, int y = 0, ui_anchor anchor = top_left );

        void set_text( std::string );

        nc_color text_color = c_white;
};

/**
* A window with a border
*/
class bordered_window : public ui_window {
    protected:
        virtual void local_draw() override;
    public:
        bordered_window(size_t size_x, size_t size_y, int x = 0, int y = 0, ui_anchor anchor = top_left);

        nc_color border_color = BORDER_COLOR;
};

class health_bar : public ui_element {
    private:
        unsigned int max_health;
        unsigned int current_health;

        std::string bar_str = "";
        nc_color bar_color = c_green;

        static const unsigned int points_per_char = 2;

        void refresh_bar( bool overloaded, float percentage );
    protected:
        virtual void draw() override;
    public:
        health_bar(size_t size_x, int x = 0, int y = 0, ui_anchor anchor = top_left);

        void set_health_percentage( float percentage );
};

class smiley_indicator : public ui_element {
    private:
        enum smiley_state {
            very_unhappy,
            unhappy,
            neutral,
            happy,
            very_happy
        };

        smiley_state state = neutral;
        nc_color smiley_color = c_white;
        std::string smiley_str = "";
    protected:
        virtual void draw() override;
    public:
        smiley_indicator(int x = 0, int y = 0, ui_anchor anchor = top_left);

        void set_state( smiley_state new_state );
};

/**
* Class that represents a basic cataclysm tile.
*
* This basic class has just a color and symbol, and a virtual
* draw function. To draw other kinds off tiles (like ones using a tile set),
* extend from this class and override it's draw method.
*/
class ui_tile {
    public:
        long sym;
        nc_color color;

        ui_tile(long tile_char = ' ', nc_color tile_color = c_black);
        virtual ~ui_tile() = default;

        virtual void draw( WINDOW *, int, int ) const;
};

/**
* A panel that draws tiles.
*
* This tile will always have an odd size in both x and y.
* A 'center' tripoint is passed that represents the focus of this panel.
* Along with that a function to retrieve tiles in specific locations is passed,
* this is used to fill in the area around the center.
*/
template<class T = ui_tile>
class tile_panel : public ui_element {
    private:
        T *tiles;
        size_t num_tiles;
    protected:
        virtual void draw() override;
    public:
        tile_panel(size_t size_x, size_t size_y, int x = 0, int y = 0, ui_anchor anchor = top_left);
        ~tile_panel() override;

        virtual void set_rect(const ui_rect &new_rect) override;
        void set_tile( const T &tile, unsigned int x, unsigned int y );
};

/**
* A window with tabs at the top.
*/
class tabbed_window : public bordered_window {
    private:
        std::vector<std::pair<std::string, ui_window *>> tabs;
        unsigned int tab_index;
    protected:
        virtual void local_draw() override;
    public:
        tabbed_window(size_t size_x, size_t size_y, int x = 0, int y = 0, ui_anchor anchor = top_left);

        template<class T = ui_window>
        T *create_tab(std::string tab);

        void next_tab();
        void previous_tab();
        const std::pair<std::string, ui_window *> &current_tab() const;
};

/**
* A window that fills in blanks with border.
*
* The idea is that you nest a bunch of windows in this one, and it automatically draws borders around them.
*/
class auto_bordered_window : public ui_window {
    private:
        bool *uncovered;
        void recalc_uncovered();
        bool is_uncovered(int x, int y) const;
        long get_border_char(unsigned int x, unsigned int y) const;
    protected:
        virtual void local_draw() override;
    public:
        auto_bordered_window(size_t size_x, size_t size_y, int x = 0, int y = 0, ui_anchor anchor = top_left);
        ~auto_bordered_window() override;

        virtual void set_rect(const ui_rect &new_rect) override;
        virtual void add_child( ui_element *child ) override;

        nc_color border_color = BORDER_COLOR;
};

/**
* Basically, a list of text.
*
* One of the lines of text is highlighted, selected.
* The list also has a scroll bar, if it's big enough.
*/
class ui_vertical_list : public ui_element {
    private:
        std::vector<std::string> text;
        unsigned int scroll = 0;
        unsigned int window_scroll = 0;
    protected:
        virtual void draw() override;
    public:
        ui_vertical_list(size_t size_x, size_t size_y, int x = 0, int y = 0, ui_anchor anchor = top_left);

        nc_color text_color = c_white;
        nc_color bar_color = c_ltblue;

        void set_text(std::vector<std::string> text);

        void scroll_up();
        void scroll_down();
        const std::string &current() const;
};

///@}

void ui_test_func();

#endif // CATA_UI_H
