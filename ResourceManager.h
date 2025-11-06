#pragma once

#include <map>
#include <wx/bitmap.h>

class ResourceManager {
public:
    wxBitmap GetBitmap(const wxString &fileName);

private:
    wxBitmap LoadFromDisk(const wxString &fileName) const;

    std::map<wxString, wxBitmap> m_cache;
};
