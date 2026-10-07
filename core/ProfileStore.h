#pragma once

#include <string>
#include <vector>

#include "Profile.h"


// saved profiles, ~/.config/openaula/profiles.conf
class ProfileStore
{
private:

    std::vector<Profile> profiles;
    int activeIndex = -1;


public:

    static std::string filePath();


    const std::vector<Profile>& all() const { return profiles; }

    int activeProfileIndex() const { return activeIndex; }

    void setActiveProfileIndex(int index) { activeIndex = index; }


    int addProfile(const Profile& profile);

    void updateProfile(int index, const Profile& profile);

    void renameProfile(int index, const std::string& newName);

    // keeps activeIndex pointing at the same profile
    void removeProfile(int index);

    int indexByName(const std::string& name) const;


    void load(int keyCount);

    void save() const;

};
