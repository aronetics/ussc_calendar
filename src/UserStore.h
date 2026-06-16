#pragma once
#include <wx/colour.h>
#include <string>
#include <vector>
#include <functional>

struct UserEntry {
    std::string id;
    std::string name;
    std::string group;
    wxColour    color;
    bool        visible = true;
};

class UserStore {
public:
    UserStore();
    void Load();
    void Save() const;

    std::vector<UserEntry>&       Users()       { return users_; }
    const std::vector<UserEntry>& Users() const { return users_; }

    std::vector<std::string>      Groups() const;
    std::vector<const UserEntry*> UsersInGroup(const std::string& grp) const;
    const UserEntry*              FindUser(const std::string& id) const;

    void SetVisible(const std::string& id, bool vis);

    std::function<void()> OnVisibilityChanged;

private:
    std::vector<UserEntry> users_;
    std::string            path_;
    void CreateDefaultUsers();
};
