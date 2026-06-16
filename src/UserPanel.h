#pragma once
#include <wx/wx.h>
#include <wx/scrolwin.h>
#include "UserStore.h"
#include <functional>

class UserPanel : public wxScrolledWindow {
public:
    UserPanel(wxWindow* parent, UserStore* store);
    void Rebuild();

    std::function<void()> OnVisibilityChanged;

private:
    UserStore* store_;
};
