#include "ResourceManager.h"

#include <wx/artprov.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

wxBitmap ResourceManager::GetBitmap(const wxString &fileName) {
    auto it = m_cache.find(fileName);
    if (it != m_cache.end()) {
        return it->second;
    }

    wxBitmap bitmap = LoadFromDisk(fileName);
    m_cache.emplace(fileName, bitmap);
    return bitmap;
}

wxBitmap ResourceManager::LoadFromDisk(const wxString &fileName) const {
    wxString bitmapPath;

    const wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    wxFileName candidate(exe.GetPath(), "resource/image/" + fileName);
    if (candidate.FileExists()) {
        bitmapPath = candidate.GetFullPath();
    } else {
        wxFileName cwd(wxFileName::DirName(wxGetCwd()));
        wxFileName cwdCandidate(cwd.GetFullPath(), "resource/image/" + fileName);
        if (cwdCandidate.FileExists()) {
            bitmapPath = cwdCandidate.GetFullPath();
        }
    }

    if (!bitmapPath.empty()) {
        wxBitmap bitmap(bitmapPath, wxBITMAP_TYPE_PNG);
        if (bitmap.IsOk()) {
            return bitmap;
        }
    }

    return wxArtProvider::GetBitmap(wxART_MISSING_IMAGE);
}
