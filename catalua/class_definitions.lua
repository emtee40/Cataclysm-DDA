-- Defines which attributes are exported by our C++ classes.

player = {
    attributes = {
        hunger = {
            type = "int",
            writable = true
        },
        thirst = {
            type = "int",
            writable = true
        },
        fatigue = {
            type = "int",
            writable = true
        },
        health = {
            type = "int",
            writable = false
        },
        
        name = {
            type = "string",
            writable = false
        },
        
        male = {
            type = "bool",
            writable = false
        },
        
        str_cur = {
            type = "int",
            writable = false
        },
        dex_cur = {
            type = "int",
            writable = false
        },
        int_cur = {
            type = "int",
            writable = false
        },
        per_cur = {
            type = "int",
            writable = false
        },
        
        str_max = {
            type = "int",
            writable = false
        },
        dex_max = {
            type = "int",
            writable = false
        },
        int_max = {
            type = "int",
            writable = false
        },
        per_max = {
            type = "int",
            writable = false
        },

        stim = {
            type = "int",
            writable = true
        }
    },
    functions = {
        has_disease = {
            args = { "string" },
            rval = "bool"
        },
        rem_disease = {
            args = { "string" },
            rval = nil
        },
        add_disease = {
            args = { "string", "int", "int", "int" },
            rval = nil
        },
        morale_level = {
            args = { },
            rval = "int"
        },
        is_npc = {
            args = {},
            rval = "bool"
        }
    }
}

--[[

 std::string name;
 bool male;
 profession* prof;
 bool my_traits[PF_MAX2];
 bool my_mutations[PF_MAX2];
 int mutation_category_level[NUM_MUTATION_CATEGORIES];
 int next_climate_control_check;
 bool last_climate_control_ret;
 std::vector<bionic> my_bionics;
// Current--i.e. modified by disease, pain, etc.
 int str_cur, dex_cur, int_cur, per_cur;
// Maximum--i.e. unmodified by disease
 int str_max, dex_max, int_max, per_max;
 int power_level, max_power_level;
 int hunger, thirst, fatigue, health;
 bool underwater;
 int oxygen;
 unsigned int recoil;
 unsigned int driving_recoil;
 unsigned int scent;
 int dodges_left, blocks_left;
 int stim, pain, pkill, radiation;
 int cash;
 int moves;
 int movecounter;
 int hp_cur[num_hp_parts], hp_max[num_hp_parts];
 signed int temp_cur[num_bp], frostbite_timer[num_bp], temp_conv[num_bp];
 void temp_equalizer(body_part bp1, body_part bp2); // Equalizes heat between body parts
 bool nv_cached;

]]

item = {
    attributes = {
    },
    functions = {
        tname = {
            args = { "game" },
            rval = "string"
        }
    }
}

global_functions = {
    add_msg = {
        cpp_name = "g->add_msg",
        args = { "cstring" },
        rval = nil
    }
}
