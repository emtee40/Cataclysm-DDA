#pragma once
#ifndef OVERMAP_BIOME_H
#define OVERMAP_BIOME_H

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <set>

#include "int_id.h"
#include "string_id.h"

class JsonObject;

class overmap_biome
{
    public:
        void load( const JsonObject &jo, const std::string & );
        void check() const;
        void finalize();

    public:
        // Used by generic_factory
        string_id<overmap_biome> id;

        //Biome weight on OM
        int weight;
        std::string name;
        bool was_loaded = false;
};

namespace overmap_biomes
{
void load( const JsonObject &jo, const std::string &src );
void check_consistency();
void reset();
void finalize();

} // namespace overmap_biomes

using t_biomes_map = std::unordered_map<std::string, overmap_biome>;
using t_biomes_map_citr = t_biomes_map::const_iterator;
extern t_biomes_map overmap_biomes_map;

#endif // OVERMAP_BIOME_H
