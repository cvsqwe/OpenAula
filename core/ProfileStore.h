#pragma once

#include <string>
#include <vector>

#include "Profile.h"


// Persists the named library of saved Profile snapshots to
// ~/.config/openaula/profiles.conf. GUI-only (the daemon never touches
// this - see Profile.h) but deliberately kept Qt-free like the rest of
// core/, matching AppState.
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


    // Returns the new profile's index.
    int addProfile(const Profile& profile);

    void updateProfile(int index, const Profile& profile);

    void renameProfile(int index, const std::string& newName);

    // Removes the profile at `index`. Clears activeIndex if it pointed at
    // the removed entry or shifts it to track the same logical profile
    // otherwise.
    void removeProfile(int index);

    int indexByName(const std::string& name) const;


    void load(int keyCount);

    void save() const;

};
