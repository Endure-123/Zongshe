#include "ResourceManager.h"

wxImageList* ResourceManager::LoadImageList(const wxString& imgDir, std::vector<wxString>& gateNames)
{
    wxImageList* imgList = new wxImageList(24, 24);

    // 先加载 folder.png 作为根节点图标
    wxImage folderImg(imgDir + "folder.png", wxBITMAP_TYPE_PNG);
    if (folderImg.IsOk()) {
        if (!folderImg.HasAlpha()) folderImg.InitAlpha();
        folderImg.Rescale(24, 24, wxIMAGE_QUALITY_HIGH);
        imgList->Add(wxBitmap(folderImg));
    }
    else {
        wxImage placeholder(24, 24);
        placeholder.SetRGB(wxRect(0, 0, 24, 24), 200, 200, 200);
        imgList->Add(wxBitmap(placeholder));
    }

    // 遍历所有 PNG 文件
    wxArrayString files;
    wxDir::GetAllFiles(imgDir, &files, "*.png", wxDIR_FILES);

    // 排除不需要的文件
    std::set<wxString> exclude = { "folder", "Arrow", "BigA", "hands" ,"delete" ,"select","drawLine"};

    for (auto& f : files) {
        wxFileName fn(f);
        wxString name = fn.GetName();
        if (exclude.count(name)) continue;

        wxImage img(f, wxBITMAP_TYPE_PNG);
        if (!img.IsOk()) {
            wxImage placeholder(24, 24);
            placeholder.SetRGB(wxRect(0, 0, 24, 24), 255, 0, 0);
            img = placeholder;
        }
        else {
            if (!img.HasAlpha()) img.InitAlpha();
            img.Rescale(24, 24, wxIMAGE_QUALITY_HIGH);
        }

        imgList->Add(wxBitmap(img));
        gateNames.push_back(name);
    }

    return imgList;
}

wxBitmap ResourceManager::LoadBitmap(const wxString& imgDir, const wxString& name, int size)
{
    wxImage img(imgDir + name + ".png", wxBITMAP_TYPE_PNG);
    if (!img.IsOk()) {
        wxImage placeholder(32, 32);
        placeholder.SetRGB(wxRect(0, 0, 32, 32), 255, 0, 0);
        return wxBitmap(placeholder);
    }

    if (size > 0) {
        img.Rescale(size, size, wxIMAGE_QUALITY_HIGH);
    }
    return wxBitmap(img);
}