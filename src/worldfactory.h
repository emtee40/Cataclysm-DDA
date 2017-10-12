#pragma once
#ifndef WORLDFACTORY_H
#define WORLDFACTORY_H

#include "cursesdef.h"
#include "enums.h"
#include "json.h"
#include "options.h"

#include <functional>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <iosfwd>

class JsonIn;

class save_t
{
    private:
        std::string name;

        save_t( const std::string &name );

    public:
        std::string player_name() const;
        std::string base_path() const;

        static save_t from_player_name( const std::string &name );
        static save_t from_base_path( const std::string &base_path );

        bool operator==( const save_t &rhs ) const {
            return name == rhs.name;
        }
        bool operator!=( const save_t &rhs ) const {
            return !operator==( rhs );
        }
        save_t &operator=( const save_t & ) = default;
};

struct WORLD {
    std::string world_path;
    std::string world_name;
    options_manager::options_container WORLD_OPTIONS;
    std::vector<save_t> world_saves;
    /**
     * A (possibly empty) list of (idents of) mods that
     * should be loaded for this world.
     */
    std::vector<std::string> active_mod_order;

    WORLD();

    bool save_exists( const save_t &name ) const;
    void add_save( const save_t &name );

    void load_options( JsonIn &jsin );
    void load_legacy_options( std::istream &fin );
};

class mod_manager;
class mod_ui;
class input_context;

typedef WORLD *WORLDPTR;

class worldfactory
{
    public:
        /** Default constructor */
        worldfactory();
        /** Default destructor */
        virtual ~worldfactory();

        // Generate a world
        WORLDPTR make_new_world( bool show_prompt = true );
        WORLDPTR make_new_world( special_game_id special_type );
        // Used for unit tests - does NOT verify if the mods can be loaded
        WORLDPTR make_new_world( const std::vector<std::string> &mods );
        WORLDPTR convert_to_world( std::string origin_path );
        /// Returns the *existing* world of given name.
        WORLDPTR get_world( const std::string &name );
        bool has_world( const std::string &name ) const;

        void set_active_world( WORLDPTR world );
        bool save_world( WORLDPTR world = NULL, bool is_conversion = false );

        void init();

        WORLDPTR pick_world( bool show_prompt = true );

        WORLDPTR active_world;

        std::vector<std::string> all_worldnames() const;

        mod_manager *get_mod_manager();

        void remove_world( std::string worldname );
        bool valid_worldname( std::string name, bool automated = false );

        /**
         * World need CDDA build with Lua support
         * @param world_name World name to test
         * @return True if world can't be loaded without Lua support. False otherwise. (When LUA is defined it's allways false).
         */
        bool world_need_lua_build( std::string world_name );
        /**
         * @param delete_folder If true: delete all the files and directories  of the given
         * world folder. Else just avoid deleting the config files and the directory
         * itself.
         */
        void delete_world( const std::string &worldname, bool delete_folder );

        static void draw_worldgen_tabs( WINDOW *win, unsigned int current );
        void show_active_world_mods( const std::vector<std::string> &world_mods );

    protected:
    private:
        std::map<std::string, WORLDPTR> all_worlds;

        std::string pick_random_name();
        int show_worldgen_tab_options( WINDOW *, WORLDPTR world );
        int show_worldgen_tab_modselection( WINDOW *win, WORLDPTR world );
        int show_worldgen_tab_confirm( WINDOW *win, WORLDPTR world );

        void draw_modselection_borders( WINDOW *win, input_context *ctxtp );
        void draw_mod_list( WINDOW *w, int &start, int &cursor, const std::vector<std::string> &mods,
                            bool is_active_list, const std::string &text_if_empty, WINDOW *w_shift );

        bool load_world_options( WORLDPTR &world );

        WORLDPTR add_world( WORLDPTR world );

        std::unique_ptr<mod_manager> mman;
        std::unique_ptr<mod_ui> mman_ui;

        typedef std::function<int( WINDOW *, WORLDPTR )> worldgen_display;

        std::vector<worldgen_display> tabs;
};

void load_world_option( JsonObject &jo );

extern std::unique_ptr<worldfactory> world_generator;

#endif
