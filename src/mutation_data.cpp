#include "mutation.h"
#include "json.h"
#include "pldata.h" // traits
#include "enums.h" // tripoint
#include "bodypart.h"
#include "debug.h"
#include "trait_group.h"
#include "translations.h"

#include <set>
#include <map>
#include <vector>

typedef std::map<trait_group::Trait_group_tag, trait_group::Trait_creation_data *> TraitGroupMap;
typedef std::set<trait_group::Trait_id> TraitSet;

TraitSet trait_blacklist;
TraitGroupMap trait_groups = {
    // An empty dummy group, it will not generate any traits. However, it makes that trait group
    // id valid, so it can be used all over the place without need to explicitly check for it.
    {"EMPTY_GROUP", new trait_group::Trait_group_collection(100)}
};

std::vector<dream> dreams;
std::map<std::string, std::vector<trait_id> > mutations_category;
std::map<std::string, mutation_category_trait> mutation_category_traits;
std::unordered_map<trait_id, mutation_branch> mutation_data;

template<>
const mutation_branch& string_id<mutation_branch>::obj() const
{
    const auto iter = mutation_data.find( *this );
    if( iter == mutation_data.end() ) {
        debugmsg( "invalid trait/mutation type id %s", c_str() );
        static const mutation_branch dummy{};
        return dummy;
    }
    return iter->second;
}

template<>
bool string_id<mutation_branch>::is_valid() const
{
    return mutation_data.count( *this ) > 0;
}

static void extract_mod(JsonObject &j, std::unordered_map<std::pair<bool, std::string>, int> &data,
                        std::string mod_type, bool active, std::string type_key)
{
    int val = j.get_int(mod_type, 0);
    if (val != 0) {
        data[std::make_pair(active, type_key)] = val;
    }
}

static void load_mutation_mods(JsonObject &jsobj, std::string member, std::unordered_map<std::pair<bool, std::string>, int> &mods)
{
    if (jsobj.has_object(member)) {
        JsonObject j = jsobj.get_object(member);
        bool active = false;
        if (member == "active_mods") {
            active = true;
        }
        //                   json field             type key
        extract_mod(j, mods, "str_mod",     active, "STR");
        extract_mod(j, mods, "dex_mod",     active, "DEX");
        extract_mod(j, mods, "per_mod",     active, "PER");
        extract_mod(j, mods, "int_mod",     active, "INT");
    }
}

void load_mutation_category(JsonObject &jsobj)
{
    mutation_category_trait new_category;
    new_category.id = jsobj.get_string("id");
    new_category.name =_(jsobj.get_string("name").c_str());
    new_category.category = jsobj.get_string( "category" );
    // @todo Remove
    new_category.category_full = jsobj.get_string( "category_full", "MUTCAT_" + new_category.category );
    // @todo Remove default, make it required
    new_category.threshold_mut = trait_id( jsobj.get_string( "threshold_mut", "THRESH_" + new_category.category ) );
    new_category.mutagen_flag = jsobj.get_string( "mutagen_flag", "MUTAGEN_" + new_category.category );

    new_category.mutagen_message = _(jsobj.get_string("mutagen_message", "You drink your mutagen").c_str());
    new_category.mutagen_hunger  = jsobj.get_int("mutagen_hunger", 10);
    new_category.mutagen_thirst  = jsobj.get_int("mutagen_thirst", 10);
    new_category.mutagen_pain    = jsobj.get_int("mutagen_pain", 2);
    new_category.mutagen_fatigue = jsobj.get_int("mutagen_fatigue", 5);
    new_category.mutagen_morale  = jsobj.get_int("mutagen_morale", 0);
    new_category.iv_message = _(jsobj.get_string("iv_message", "You inject yourself").c_str());
    new_category.iv_min_mutations    = jsobj.get_int("iv_min_mutations", 1);
    new_category.iv_additional_mutations = jsobj.get_int("iv_additional_mutations", 2);
    new_category.iv_additional_mutations_chance = jsobj.get_int("iv_additional_mutations_chance", 3);
    new_category.iv_hunger   = jsobj.get_int("iv_hunger", 10);
    new_category.iv_thirst   = jsobj.get_int("iv_thirst", 10);
    new_category.iv_pain     = jsobj.get_int("iv_pain", 2);
    new_category.iv_fatigue  = jsobj.get_int("iv_fatigue", 5);
    new_category.iv_morale   = jsobj.get_int("iv_morale", 0);
    new_category.iv_morale_max   = jsobj.get_int("iv_morale_max", 0);
    new_category.iv_sound = jsobj.get_bool("iv_sound", false);
    new_category.iv_sound_message = _(jsobj.get_string("iv_sound_message", "You inject yoursel-arRGH!").c_str());
    new_category.iv_noise = jsobj.get_int("iv_noise", 0);
    new_category.iv_sleep = jsobj.get_bool("iv_sleep", false);
    new_category.iv_sleep_message =_(jsobj.get_string("iv_sleep_message", "Fell asleep").c_str());
    new_category.iv_sleep_dur = jsobj.get_int("iv_sleep_dur", 0);
    new_category.memorial_message = _(jsobj.get_string("memorial_message", "Crossed a threshold").c_str());
    new_category.junkie_message = _(jsobj.get_string("junkie_message", "Oh, yeah! That's the stuff!").c_str());

    mutation_category_traits[new_category.id] = new_category;
}

const std::map<std::string, mutation_category_trait> &mutation_category_trait::get_all()
{
    return mutation_category_traits;
}

void mutation_category_trait::reset()
{
    mutation_category_traits.clear();
}

void mutation_category_trait::check_consistency()
{
    for( const auto &pr : mutation_category_traits ) {
        const mutation_category_trait &cat = pr.second;
        if( !cat.threshold_mut.is_empty() && !cat.threshold_mut.is_valid() ) {
            debugmsg( "Mutation category %s has threshold mutation %s, which does not exist",
                      cat.id.c_str(), cat.threshold_mut.c_str() );
        }
    }
}

static mut_attack load_mutation_attack( JsonObject &jo )
{
    mut_attack ret;
    jo.read( "attack_text_u", ret.attack_text_u );
    jo.read( "attack_text_npc", ret.attack_text_npc );
    jo.read( "required_mutations", ret.required_mutations );
    jo.read( "blocker_mutations", ret.blocker_mutations );
    jo.read( "hardcoded_effect", ret.hardcoded_effect );

    if( jo.has_string( "body_part" ) ) {
        ret.bp = get_body_part_token( jo.get_string( "body_part" ) );
    }

    jo.read( "chance", ret.chance );

    if( jo.has_array( "base_damage" ) ) {
        JsonArray jo_dam = jo.get_array( "base_damage" );
        ret.base_damage = load_damage_instance( jo_dam );
    } else if( jo.has_object( "base_damage" ) ) {
        JsonObject jo_dam = jo.get_object( "base_damage" );
        ret.base_damage = load_damage_instance( jo_dam );
    }

    if( jo.has_array( "strength_damage" ) ) {
        JsonArray jo_dam = jo.get_array( "strength_damage" );
        ret.strength_damage = load_damage_instance( jo_dam );
    } else if( jo.has_object( "strength_damage" ) ) {
        JsonObject jo_dam = jo.get_object( "strength_damage" );
        ret.strength_damage = load_damage_instance( jo_dam );
    }

    if( ret.attack_text_u.empty() || ret.attack_text_npc.empty() ) {
        jo.throw_error( "Attack message unset" );
    }

    if( !ret.hardcoded_effect && ret.base_damage.empty() && ret.strength_damage.empty() ) {
        jo.throw_error( "Damage unset" );
    } else if( ret.hardcoded_effect && ( !ret.base_damage.empty() || !ret.strength_damage.empty() ) ) {
        jo.throw_error( "Damage and hardcoded effect are both set (must be one, not both)" );
    }

    if( ret.chance <= 0 ) {
        jo.throw_error( "Chance of procing must be set and positive" );
    }

    return ret;
}

void mutation_branch::load( JsonObject &jsobj )
{
    const trait_id id( jsobj.get_string( "id" ) );
    mutation_branch &new_mut = mutation_data[id];

    JsonArray jsarr;
    new_mut.name = _(jsobj.get_string("name").c_str());
    new_mut.description = _(jsobj.get_string("description").c_str());
    new_mut.points = jsobj.get_int("points");
    new_mut.visibility = jsobj.get_int("visibility", 0);
    new_mut.ugliness = jsobj.get_int("ugliness", 0);
    new_mut.startingtrait = jsobj.get_bool("starting_trait", false);
    new_mut.mixed_effect = jsobj.get_bool("mixed_effect", false);
    new_mut.activated = jsobj.get_bool("active", false);
    new_mut.starts_active = jsobj.get_bool("starts_active", false);
    new_mut.destroys_gear = jsobj.get_bool("destroys_gear", false);
    new_mut.allow_soft_gear = jsobj.get_bool("allow_soft_gear", false);
    new_mut.cost = jsobj.get_int("cost", 0);
    new_mut.cooldown = jsobj.get_int("time",0);
    new_mut.hunger = jsobj.get_bool("hunger",false);
    new_mut.thirst = jsobj.get_bool("thirst",false);
    new_mut.fatigue = jsobj.get_bool("fatigue",false);
    new_mut.valid = jsobj.get_bool("valid", true);
    new_mut.purifiable = jsobj.get_bool("purifiable", true);
    if( jsobj.has_object( "spawn_item" ) ) {
        JsonObject spawn_item = jsobj.get_object( "spawn_item" );
        new_mut.spawn_item = spawn_item.get_string( "type", "" );
        new_mut.spawn_item_message = spawn_item.get_string( "message", "" );
    }
    for( auto & s : jsobj.get_string_array( "initial_ma_styles" ) ) {
        new_mut.initial_ma_styles.push_back( matype_id( s ) );
    }

    JsonArray bodytemp_array = jsobj.get_array( "bodytemp_modifiers" );
    if( bodytemp_array.has_more() ) {
        new_mut.bodytemp_min = bodytemp_array.get_int( 0 );
        new_mut.bodytemp_max = bodytemp_array.get_int( 1 );
    }
    new_mut.bodytemp_sleep = jsobj.get_int( "bodytemp_sleep", 0 );
    new_mut.threshold = jsobj.get_bool("threshold", false);
    new_mut.profession = jsobj.get_bool("profession", false);

    auto vr = jsobj.get_array( "vitamin_rates" );
    while( vr.has_more() ) {
        auto pair = vr.next_array();
        new_mut.vitamin_rates[ vitamin_id( pair.get_string( 0 ) ) ] = pair.get_int( 1 );
    }

    new_mut.healing_awake = jsobj.get_float( "healing_awake", 0.0f );
    new_mut.healing_resting = jsobj.get_float( "healing_resting", 0.0f );
    new_mut.hp_modifier = jsobj.get_float( "hp_modifier", 0.0f );
    new_mut.hp_modifier_secondary = jsobj.get_float( "hp_modifier_secondary", 0.0f );
    new_mut.hp_adjustment = jsobj.get_float( "hp_adjustment", 0.0f );

    new_mut.metabolism_modifier = jsobj.get_float( "metabolism_modifier", 0.0f );
    new_mut.thirst_modifier = jsobj.get_float( "thirst_modifier", 0.0f );
    new_mut.fatigue_modifier = jsobj.get_float( "fatigue_modifier", 0.0f );
    new_mut.fatigue_regen_modifier = jsobj.get_float( "fatigue_regen_modifier", 0.0f );

    new_mut.stamina_regen_modifier = jsobj.get_float( "stamina_regen_modifier", 0.0f );

    load_mutation_mods(jsobj, "passive_mods", new_mut.mods);
    /* Not currently supported due to inability to save active mutation state
    load_mutation_mods(jsobj, "active_mods", new_mut.mods); */

    for( auto &t : jsobj.get_string_array( "prereqs" ) ) {
        new_mut.prereqs.emplace_back( t );
    }
    // Helps to be able to have a trait require more than one other trait
    // (Individual prereq-lists are "OR", not "AND".)
    // Traits shoud NOT appear in both lists for a given mutation, unless
    // you want that trait to satisfy both requirements.
    // These are additional to the first list.
    for( auto &t : jsobj.get_string_array( "prereqs2" ) ) {
        new_mut.prereqs2.emplace_back( t );
    }
    // Dedicated-purpose prereq slot for Threshold mutations
    // Stuff like Huge might fit in more than one mutcat post-threshold, so yeah
    for( auto &t : jsobj.get_string_array( "threshreq" ) ) {
        new_mut.threshreq.emplace_back( t );
    }
    for( auto &t : jsobj.get_string_array( "cancels" ) ) {
        new_mut.cancels.emplace_back( t );
    }
    for( auto &t : jsobj.get_string_array( "changes_to" ) ) {
        new_mut.replacements.emplace_back( t );
    }
    for( auto &t : jsobj.get_string_array( "leads_to" ) ) {
        new_mut.additions.emplace_back( t );
    }
    new_mut.flags = jsobj.get_tags( "flags" );
    jsarr = jsobj.get_array("category");
    while (jsarr.has_more()) {
        std::string s = jsarr.next_string();
        new_mut.category.push_back(s);
        mutations_category[s].push_back( trait_id( id ) );
    }
    jsarr = jsobj.get_array("wet_protection");
    while (jsarr.has_more()) {
        JsonObject jo = jsarr.next_object();
        std::string part_id = jo.get_string("part");
        int ignored = jo.get_int("ignored", 0);
        int neutral = jo.get_int("neutral", 0);
        int good = jo.get_int("good", 0);
        tripoint protect = tripoint(ignored, neutral, good);
        new_mut.protection[get_body_part_token( part_id )] = protect;
    }

    jsarr = jsobj.get_array("encumbrance_always");
    while (jsarr.has_more()) {
        JsonArray jo = jsarr.next_array();
        std::string part_id = jo.next_string();
        int enc = jo.next_int();
        new_mut.encumbrance_always[get_body_part_token( part_id )] = enc;
    }

    jsarr = jsobj.get_array("encumbrance_covered");
    while (jsarr.has_more()) {
        JsonArray jo = jsarr.next_array();
        std::string part_id = jo.next_string();
        int enc = jo.next_int();
        new_mut.encumbrance_covered[get_body_part_token( part_id )] = enc;
    }

    jsarr = jsobj.get_array("restricts_gear");
    while( jsarr.has_more() ) {
        new_mut.restricts_gear.insert( get_body_part_token( jsarr.next_string() ) );
    }

    jsarr = jsobj.get_array( "armor" );
    while( jsarr.has_more() ) {
        JsonObject jo = jsarr.next_object();
        auto parts = jo.get_tags( "parts" );
        std::set<body_part> bps;
        for( const std::string &part_string : parts ) {
            if( part_string == "ALL" ) {
                // Shorthand, since many muts protect whole body
                for( size_t i = 0; i < num_bp; i++ ) {
                    bps.insert( static_cast<body_part>( i ) );
                }
            } else {
                bps.insert( get_body_part_token( part_string ) );
            }
        }

        resistances res = load_resistances_instance( jo );

        for( body_part bp : bps ) {
            new_mut.armor[ bp ] = res;
        }
    }

    if( jsobj.has_array( "attacks" ) ) {
        jsarr = jsobj.get_array( "attacks" );
        while( jsarr.has_more() ) {
            JsonObject jo = jsarr.next_object();
            new_mut.attacks_granted.emplace_back( load_mutation_attack( jo ) );
        }
    } else if( jsobj.has_object( "attacks" ) ) {
        JsonObject jo = jsobj.get_object( "attacks" );
        new_mut.attacks_granted.emplace_back( load_mutation_attack( jo ) );
    }
}

static void check_consistency( const std::vector<trait_id> &mvec, const trait_id &mid, const std::string &what )
{
    for( const auto &m : mvec ) {
        if( !m.is_valid() ) {
            debugmsg( "mutation %s refers to undefined %s %s", mid.c_str(), what.c_str(), m.c_str() );
        }
    }
}

void mutation_branch::check_consistency()
{
    for( const auto & m : mutation_data ) {
        const auto &mid = m.first;
        const auto &mdata = m.second;
        for( const auto & style : mdata.initial_ma_styles ) {
            if( !style.is_valid() ) {
                debugmsg( "mutation %s refers to undefined martial art style %s", mid.c_str(), style.c_str() );
            }
        }
        ::check_consistency( mdata.prereqs, mid, "prereq" );
        ::check_consistency( mdata.prereqs2, mid, "prereqs2" );
        ::check_consistency( mdata.threshreq, mid, "threshreq" );
        ::check_consistency( mdata.cancels, mid, "cancels" );
        ::check_consistency( mdata.replacements, mid, "replacements" );
        ::check_consistency( mdata.additions, mid, "additions" );
    }
}

nc_color mutation_branch::get_display_color() const
{
    if( threshold || profession ) {
        return c_white;
    } else if( mixed_effect ) {
        return c_pink;
    } else if( points > 0 ) {
        return c_ltgreen;
    } else if( points < 0 ) {
        return c_ltred;
    } else {
        return c_yellow;
    }
}

const std::string &mutation_branch::get_name( const trait_id &mutation_id )
{
    return mutation_id->name;
}

const mutation_branch::MutationMap &mutation_branch::get_all()
{
    return mutation_data;
}

void mutation_branch::reset_all()
{
    mutations_category.clear();
    mutation_data.clear();
    for ( auto &trait : trait_groups ) {
        delete trait.second;
    }
    trait_blacklist.clear();
    trait_groups.clear();
    trait_groups["EMPTY_GROUP"] = new trait_group::Trait_group_collection(100);
}

void mutation_branch::load_trait_blacklist( JsonObject &jsobj ) {
    JsonArray jarr = jsobj.get_array( "traits" );
    while (jarr.has_more()) {
        trait_group::Trait_id id(jarr.next_string());
        trait_blacklist.insert(id);
    }
}

void mutation_branch::load_trait_group(JsonObject &jsobj) {
    const trait_group::Trait_group_tag group_id = jsobj.get_string("id");
    const std::string subtype = jsobj.get_string("subtype", "old");
    load_trait_group(jsobj, group_id, subtype);
}

trait_group::Trait_group *make_group_or_throw(const trait_group::Trait_group_tag &gid, trait_group::Trait_creation_data *&tcd, bool is_collection) {
    trait_group::Trait_group* tg = dynamic_cast<trait_group::Trait_group *>(tcd);

    // TODO(sm): not yet clear whether or not this misses anything from make_group_or_throw
    if (tg == nullptr) {
        if (is_collection) {
            tcd = tg = new trait_group::Trait_group_collection(100);
        } else {
            tcd = tg = new trait_group::Trait_group_distribution(100);
        }
    } else {
        // Evidently, making the collection/distribution separation better has made the code for this check worse.
        if (is_collection) {
            if (trait_group::Trait_group_distribution *tgd =
                    dynamic_cast<trait_group::Trait_group_distribution*>(tcd)) {
                throw std::runtime_error("item group \"" + gid + "\" already defined with type \"distribution\"" );
            }
        } else {
            if (trait_group::Trait_group_collection *tgc =
                    dynamic_cast<trait_group::Trait_group_collection*>(tcd)) {
                throw std::runtime_error("item group \"" + gid + "\" already defined with type \"collection\"" );
            }
        }
    }
    return tg;
}

void mutation_branch::load_trait_group(JsonArray &entries, const trait_group::Trait_group_tag &gid, const bool is_collection) {
    trait_group::Trait_creation_data *&tcd = trait_groups[gid];
    trait_group::Trait_group* tg = make_group_or_throw(gid, tcd, is_collection);

    while(entries.has_more()) {
        // Backwards-compatibility with old format ["TRAIT", 100]
        if (entries.test_array()) {
            JsonArray subarr = entries.next_array();

            trait_group::Trait_id id(subarr.get_string(0));
            std::unique_ptr<trait_group::Trait_creation_data> ptr(
                    new trait_group::Single_trait_creator(id, subarr.get_int(1)));
            tg->add_entry(ptr);
        // Otherwise load new format {"trait": ... } or {"group": ...}
        } else {
            JsonObject subobj = entries.next_object();
            add_entry(tg, subobj);
        }
    }
}

void mutation_branch::load_trait_group(JsonObject &jsobj, const trait_group::Trait_group_tag &gid, const std::string &subtype) {
    trait_group::Trait_creation_data *&tcd = trait_groups[gid];
    trait_group::Trait_group *tg = dynamic_cast<trait_group::Trait_group *>(tcd);

    if (subtype != "distribution" && subtype != "collection" && subtype != "old") {
        jsobj.throw_error("unknown trait group type", "subtype");
    }
    tg = make_group_or_throw(gid, tcd, (subtype == "collection" || subtype == "old"));

    // TODO(sm): Looks like this makes the new code backwards-compatible with the old format. Great if so!
    if (subtype == "old") {
        JsonArray traits = jsobj.get_array("traits");
        while (traits.has_more()) {
            JsonArray pair = traits.next_array();
            tg->add_trait_entry(trait_group::Trait_id(pair.get_string(0)), pair.get_int(1));
        }
        return;
    }

    // TODO(sm): Taken from item_factory.cpp almost verbatim. Ensure that these work!
    if (jsobj.has_member("entries")) {
        JsonArray traits = jsobj.get_array("entries");
        while( traits.has_more() ) {
            JsonObject subobj = traits.next_object();
            add_entry( tg, subobj );
        }
    }
    if (jsobj.has_member("traits")) {
        JsonArray traits = jsobj.get_array("traits");
        while (traits.has_more()) {
            if (traits.test_string()) {
                tg->add_trait_entry(trait_group::Trait_id(traits.next_string()), 100);
            } else if (traits.test_array()) {
                JsonArray subtrait = traits.next_array();
                tg->add_trait_entry(trait_group::Trait_id(subtrait.get_string(0)), subtrait.get_int(1));
            } else {
                JsonObject subobj = traits.next_object();
                add_entry(tg, subobj);
            }
        }
    }
    if (jsobj.has_member("groups")) {
        JsonArray traits = jsobj.get_array("groups");
        while (traits.has_more()) {
            if (traits.test_string()) {
                tg->add_group_entry(traits.next_string(), 100);
            } else if (traits.test_array()) {
                JsonArray subtrait = traits.next_array();
                tg->add_group_entry(subtrait.get_string(0), subtrait.get_int(1));
            } else {
                JsonObject subobj = traits.next_object();
                add_entry(tg, subobj);
            }
        }
    }
}

void mutation_branch::add_entry(trait_group::Trait_group *tg, JsonObject &obj) {
    std::unique_ptr<trait_group::Trait_creation_data> ptr;
    int probability = obj.get_int("prob", 100);
    JsonArray jarr;

    if (obj.has_member("collection")) {
        ptr.reset(new trait_group::Trait_group_collection(probability));
        jarr = obj.get_array("collection");
    } else if (obj.has_member("distribution")) {
        ptr.reset(new trait_group::Trait_group_distribution(probability));
        jarr = obj.get_array("distribution");
    }

    if (ptr) {
        trait_group::Trait_group *tg2 = dynamic_cast<trait_group::Trait_group *>(ptr.get());
        while (jarr.has_more()) {
            JsonObject job2 = jarr.next_object();
            add_entry(tg2, job2);
        }
        tg->add_entry(ptr);
        return;
    }

    if (obj.has_member("trait")) {
        trait_group::Trait_id id(obj.get_string("trait"));
        ptr.reset(new trait_group::Single_trait_creator(id, probability));
    } else if (obj.has_member("group")) {
        ptr.reset(new trait_group::Trait_group_creator(obj.get_string("group"), probability));
    }

    if (!ptr) {
        return;
    }

    tg->add_entry(ptr);
}

void mutation_branch::finalize() {
    finalize_trait_blacklist();
}

void mutation_branch::finalize_trait_blacklist() {
    for (auto &trait : trait_blacklist) {
        if (!has_trait(trait)) {
            debugmsg("trait on blacklist %s does not exist", trait.c_str());
        }
    }

    for (auto &md: mutation_data) {
        if (!trait_is_blacklisted(md.first)) {
            continue;
        }
        for (auto &grp : trait_groups) {
            grp.second->remove_trait(md.first);
        }
    }
}

trait_group::Trait_creation_data* mutation_branch::get_group( const trait_group::Trait_group_tag &gid ) {
    if (trait_groups.count(gid) > 0) {
        return trait_groups[gid];
    }
    return nullptr;
}

std::vector<trait_group::Trait_group_tag> mutation_branch::get_all_group_names() {
    std::vector<std::string> rval;
    for (auto &group: trait_groups) {
        rval.push_back(group.first);
    }
    return rval;
}

bool mutation_branch::trait_is_blacklisted( const trait_group::Trait_id &tid ) {
    return trait_blacklist.count( tid );
}

bool mutation_branch::has_trait( const trait_group::Trait_id &tid ) {
    return mutation_data.find(tid) != mutation_data.end();
}

void load_dream(JsonObject &jsobj)
{
    dream newdream;

    newdream.strength = jsobj.get_int("strength");
    newdream.category = jsobj.get_string("category");

    JsonArray jsarr = jsobj.get_array("messages");
    while (jsarr.has_more()) {
        newdream.messages.push_back(_(jsarr.next_string().c_str()));
    }

    dreams.push_back(newdream);
}

bool trait_display_sort( const trait_id &a, const trait_id &b ) noexcept
{
    return a->name < b->name;
}
