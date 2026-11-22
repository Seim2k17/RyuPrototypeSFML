#pragma once

#include <map>
#include <memory>
//#include <SFML/Graphics.hpp>
#include <cassert>

//namespace ryu{

// generalized approach to hold all kinds of assets
template <typename Resource, typename Identifier>
class AssetManager
{
    public:
        void load(Identifier id, const std::string& filename);
        template <typename Parameter>
        void load(Identifier id, const std::string& filename, const Parameter& secondParam);
        bool isLoaded(Identifier id) const { return mResourceMap.find(id) != mResourceMap.end(); }
        Resource& getResource(Identifier id);
        const Resource& getResource(Identifier id) const;
        bool hasResource(Identifier id) const;

    private:
        std::map<Identifier, std::unique_ptr<Resource> > mResourceMap;
};
//} /// namespace ryu
#include "AssetManager.inl"
